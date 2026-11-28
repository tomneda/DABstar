/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2019
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "dab_constants.h"
#include "dabradio.h"
#include "backend.h"
#include "frame_processor.h"
#include <cassert>

// Interleaving is - for reasons of simplicity - done inline rather than through a special class-object
constexpr i32 cCuSizeBits = 64;

//  fragmentsize == Length * CUSize
Backend::Backend(DabRadio* ipRI, const SDescriptorType* ipDescType, RingBuffer<i16>* ipoAudiobuffer, RingBuffer<u8>* ipoDatabuffer, RingBuffer<u8>* frameBuffer, EProcessFlag iProcessFlag)
  : deconvolver(ipDescType)
  , outV(ipDescType->bitRate /*kbit/s*/ * 24 /*ms*/)
  , driver(ipRI, ipDescType, ipoAudiobuffer, ipoDatabuffer, frameBuffer)
#ifdef  __THREADED_BACKEND__
  , freeSlots(NUMBER_SLOTS)
#endif
{
  this->radioInterface = ipRI;
  this->CuStartAddr = ipDescType->CuStartAddr;
  this->CuSize = ipDescType->CuSize;
  this->fragmentSize = ipDescType->CuSize * cCuSizeBits;
  this->bitRate = ipDescType->bitRate;
  this->serviceId = ipDescType->SId;
  // this->serviceName = ipDescType->serviceName;
  this->shortForm = ipDescType->shortForm;
  this->protLevel = ipDescType->protLevel;
  this->subChId = ipDescType->SubChId;
  this->processFlag = iProcessFlag;

  //fprintf(stdout, "starting a backend for %s (%X) %d\n", serviceName.toUtf8().data(), serviceId, startAddr);
  interleaveData.resize(16);
  for (i32 i = 0; i < 16; i++)
  {
    interleaveData[i].resize(fragmentSize);
    memset(interleaveData[i].data(), 0, fragmentSize * sizeof(i16));
  }
  for (auto& h : history)
  {
    h.resize(fragmentSize);
  }
  knownBits.resize(bitRate * 24);
  redecodedBits.resize(bitRate * 24);

  countforInterleaver = 0;
  interleaverIndex = 0;

  tempX.resize(fragmentSize);

  u8 shiftRegister[9];
  disperseVector.resize(24 * bitRate);
  memset(shiftRegister, 1, 9);
  for (i32 i = 0; i < bitRate * 24; i++)
  {
    const u8 b = shiftRegister[8] ^ shiftRegister[4];
    for (i32 j = 8; j > 0; j--)
    {
      shiftRegister[j] = shiftRegister[j - 1];
    }
    shiftRegister[0] = b;
    disperseVector[i] = b;
  }
#ifdef  __THREADED_BACKEND__
  //    for local buffering the input, we have
  nextIn = 0;
  nextOut = 0;
  for (i32 i = 0; i < NUMBER_SLOTS; i++)
  {
    theData[i].resize(fragmentSize);
  }

  running.store(true);
  start();
#endif
}

Backend::~Backend()
{
#ifdef  __THREADED_BACKEND__
  running.store(false);
  while (this->isRunning())
  {
    usleep(1000);
  }
#endif
}

i32 Backend::process(const std::span<const i16>& iV)
{
  assert((i32)iV.size() >= fragmentSize);
#ifdef  __THREADED_BACKEND__
  while (!freeSlots.tryAcquire(1, 200))
  {
    if (!running)
    {
      return 0;
    }
  }
  memcpy(theData[nextIn].data(), iV.data(), fragmentSize * sizeof(i16));
  nextIn = (nextIn + 1) % NUMBER_SLOTS;
  usedSlots.release(1);
#else
  _process_segment(iV.data());
#endif
  return 1;
}

static constexpr i16 interleaveMap[] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

void Backend::_process_segment(const i16* iData)
{
  if(!driver_exist)
  {
    fprintf(stderr, "SetFrameRedecoder in backend\n");
    driver.setFrameRedecoder(this);
    driver_exist = true;
  }
  for (i32 i = 0; i < fragmentSize; i++)
  {
    tempX[i] = interleaveData[(interleaverIndex + interleaveMap[i & 0x0F]) & 0x0F][i];
    interleaveData[interleaverIndex][i] = iData[i];
  }

  interleaverIndex = (interleaverIndex + 1) & 0x0F;
#ifdef  __THREADED_BACKEND__
  nextOut = (nextOut + 1) % NUMBER_SLOTS;
  freeSlots.release(1);
#endif

  // only continue when de-interleaver is filled
  if (countforInterleaver <= 15)
  {
    countforInterleaver++;
    return;
  }

  // Kept for redecodeFrame(), which is called from addtoFrame()
  history[historyNext] = tempX;
  historyNext = (historyNext + 1) % historyFrames;
  if (historyCount < historyFrames)
    historyCount++;

  deconvolver.deconvolve(tempX, outV.data());

  // Reverse the energy dispersal
  for (i32 i = 0; i < bitRate * 24; i++)
  {
    outV[i] ^= disperseVector[i];
  }

  driver.add_to_frame(outV);
}

// The ETI data is processed synchronously (also with __THREADED_BACKEND__) as the expensive Viterbi decoding is not necessary.
void Backend::process_eti(const std::span<const u8>& iData)
{
  const i32 iSize = (i32)iData.size();
  const i32 numBits = bitRate * 24;

  if (iSize * 8 != numBits)
  {
    if (!etiSizeWarned)
    {
      qWarning() << "ETI sub-channel" << subChId << "has" << iSize << "bytes but expected" << numBits / 8 << "bytes";
      etiSizeWarned = true;
    }
    return;
  }

  for (i32 i = 0; i < numBits; i++) // unpack bits MSB first
  {
    outV[i] = (iData[i >> 3] >> (7 - (i & 7))) & 1;
  }

  driver.add_to_frame(outV);
}

#ifdef  __THREADED_BACKEND__

void Backend::run()
{
  while (running.load())
  {
    while (!usedSlots.tryAcquire(1, 200))
    {
      if (!running)
      {
        return;
      }
    }
    _process_segment(theData[nextOut].data());
  }
}

#endif

//  It might take a msec for the task to stop
void Backend::stop_running()
{
#ifdef  __THREADED_BACKEND__
  running = false;
  while (this->isRunning())
  {
    usleep(1);
  }
  //    myAudioSink -> stop();
#endif
}

// Called from addtoFrame() in the thread of run(): the frames are not
// changed meanwhile
bool Backend::redecodeFrame(i32 age, const u8 *known, u8 *bytes)
{
  if (age < 0 || age >= historyCount)
    return false;

  // Known bytes (after the energy dispersal) to known bits of the
  // Viterbi decoder (before it), most significant bit first
  const i32 numBytes = knownBits.size() / 8;
  for (i32 i = 0; i < numBytes; i++)
  {
    for (i32 j = 0; j < 8; j++)
    {
      const i32 bit = 8 * i + j;
      knownBits[bit] = known[i] ? int8_t(((bytes[i] >> (7 - j)) & 1) ^ disperseVector[bit]) : -1;

    }
  }

  const i32 index = (historyNext - 1 - age + historyFrames) % historyFrames;
  deconvolver.deconvolve(history[index], redecodedBits.data(), knownBits.data());

  // Reverse the energy dispersal
  for (i32 i = 0; i < bitRate * 24; i++)
  {
    redecodedBits[i] ^= disperseVector[i];
  }

  for (i32 i = 0; i < numBytes; i++)
  {
    u8 b = 0;
    for (i32  j = 0; j < 8; j++)
      b = (b << 1) | (redecodedBits[8 * i + j] & 1);
    bytes[i] = b;
  }
  return true;
}
