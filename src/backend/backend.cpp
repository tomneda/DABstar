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
#


#include  "dab-constants.h"
#include  "dabradio.h"
#include  "backend.h"

// Interleaving is - for reasons of simplicity - done inline rather than through a special class-object
constexpr i16 cCuSizeBytes = 64;

//	fragmentsize == Length * CUSize
Backend::Backend(DabRadio * ipRI, const SDescriptorType * ipDescType, RingBuffer<i16> * ipoAudiobuffer, RingBuffer<u8> * ipoDatabuffer, RingBuffer<u8> * frameBuffer, EProcessFlag iProcessFlag)
  : deconvolver(ipDescType)
  , outV(ipDescType->bitRate * 24)
  , driver(ipRI, ipDescType, ipoAudiobuffer, ipoDatabuffer, frameBuffer)
#ifdef  __THREADED_BACKEND__
  , freeSlots(NUMBER_SLOTS)
#endif
{
  this->radioInterface = ipRI;
  this->CuStartAddr = ipDescType->CuStartAddr;
  this->CuSize = ipDescType->CuSize;
  this->fragmentSize = ipDescType->CuSize * cCuSizeBytes;
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

  countforInterleaver = 0;
  interleaverIndex = 0;

  tempX.resize(fragmentSize);

  u8 shiftRegister[9];
  disperseVector.resize(24 * bitRate);
  memset(shiftRegister, 1, 9);
  for (i32 i = 0; i < bitRate * 24; i++)
  {
    u8 b = shiftRegister[8] ^ shiftRegister[4];
    for (i32 j = 8; j > 0; j--)
    {
      shiftRegister[j] = shiftRegister[j - 1];
    }
    shiftRegister[0] = b;
    disperseVector[i] = b;
  }
#ifdef  __THREADED_BACKEND__
  //	for local buffering the input, we have
  nextIn = 0;
  nextOut = 0;
  for (i = 0; i < NUMBER_SLOTS; i++)
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

i32 Backend::process(const i16 * const iV, const i32 cnt)
{
  (void)cnt;
#ifdef  __THREADED_BACKEND__
  while (!freeSlots.tryAcquire(1, 200))
  {
    if (!running)
    {
      return 0;
    }
  }
  memcpy(theData[nextIn].data(), iV, fragmentSize * sizeof(i16));
  nextIn = (nextIn + 1) % NUMBER_SLOTS;
  usedSlots.release(1);
#else
  processSegment(iV);
#endif
  return 1;
}

const i16 interleaveMap[] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

void Backend::processSegment(const i16 * iData)
{
  for (i16 i = 0; i < fragmentSize; i++)
  {
    tempX[i] = interleaveData[(interleaverIndex + interleaveMap[i & 0x0F]) & 0x0F][i];
    interleaveData[interleaverIndex][i] = iData[i];
  }

  interleaverIndex = (interleaverIndex + 1) & 0x0F;
#ifdef  __THREADED_BACKEND__
  nextOut = (nextOut + 1) % NUMBER_SLOTS;
  freeSlots.release(1);
#endif

  //	only continue when de-interleaver is filled
  if (countforInterleaver <= 15)
  {
    countforInterleaver++;
    return;
  }

  deconvolver.deconvolve(tempX.data(), fragmentSize, outV.data());

  // Reverse the energy dispersal
  for (i16 i = 0; i < bitRate * 24; i++)
  {
    outV[i] ^= disperseVector[i];
  }

  driver.addtoFrame(outV);
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
    processSegment(theData[nextOut].data());
  }
}

#endif

//	It might take a msec for the task to stop
void Backend::stopRunning()
{
#ifdef  __THREADED_BACKEND__
  running = false;
  while (this->isRunning())
  {
    usleep(1);
  }
  //	myAudioSink	-> stop();
#endif
}
