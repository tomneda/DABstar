/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
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
#include  "dab-constants.h"
#include  "dabradio.h"
#include  "msc-handler.h"
#include  "backend.h"
//
//	Interface program for processing the MSC.
//	The DabProcessor assumes the existence of an msc-handler, whether
//	a service is selected or not. 

constexpr u32 CUSize = 4 * 16;
static constexpr i16 cifTable[] = { 18, 72, 0, 36 }; // for each DAB-Mode

//	Note CIF counts from 0 .. 3
//
MscHandler::MscHandler(DabRadio * const iRI, RingBuffer<u8> * const ipFrameBuffer)
  : mpRadioInterface(iRI)
  , mpFrameBuffer(ipFrameBuffer)
{
  mCifVector.resize(55296);
  mBitsPerBlock = c2K;
  mNumberOfBlocksPerCif = cifTable[0]; // first entry is for DAB-Mode 1
}

MscHandler::~MscHandler()
{
  mMutex.lock();
  for (auto & b: mBackendList)
  {
    b->stopRunning();
    b.reset();
  }
  mBackendList.resize(0);
  mMutex.unlock();
}

void MscHandler::reset_channel()
{
  fprintf(stdout, "channel reset: all services will be stopped\n");
  mMutex.lock();
  for (auto & b: mBackendList)
  {
    b->stopRunning();
    b.reset();
  }
  mBackendList.resize(0);
  mMutex.unlock();
}

void MscHandler::stop_service(const DescriptorType * const iDescType, const i32 iFlag)
{
  fprintf(stderr, "obsolete function stopService\n");
  mMutex.lock();

  for (qsizetype i = 0; i < mBackendList.size(); i++)
  {
    if (auto & b = mBackendList[i];
        b->subChId == iDescType->subchId && b->borf == iFlag)
    {
      fprintf(stdout, "stopping (sub)service at subchannel %d\n", iDescType->subchId);
      b->stopRunning();
      b.reset();
      mBackendList.removeAt(i);
      --i; // we removed one element
    }
  }

  mMutex.unlock();
}

void MscHandler::stop_service(const i32 iSubChId, const i32 iFlag)
{
  mMutex.lock();

  for (qsizetype i = 0; i < mBackendList.size(); i++)
  {
    if (auto & b = mBackendList[i];
        b->subChId == iSubChId && b->borf == iFlag)
    {
      fprintf(stdout, "stopping subchannel %d\n", iSubChId);
      b->stopRunning();
      b.reset();
      mBackendList.removeAt(i);
      --i; // we removed one element
    }
  }

  mMutex.unlock();
}

bool MscHandler::set_channel(const DescriptorType * d, RingBuffer<i16> * ipoAudioBuffer, RingBuffer<u8> * ipoDataBuffer, FILE * dump, i32 flag)
{
  fprintf(stdout, "going to open %s\n", d->serviceName.toLatin1().data());
  //	locker. lock();
  //	for (i32 i = 0; i < theBackends. size (); i ++) {
  //	   if (d -> subchId == theBackends. at (i) -> subChId) {
  //	      fprintf (stdout, "The service is already running\n");
  //	      theBackends. at (i) -> stopRunning ();
  //	      delete theBackends. at (i);
  //	      theBackends. erase (theBackends. begin () + i);
  //	   }
  //	}
  //	locker. unlock ();
  const QSharedPointer<Backend> backend(new Backend(mpRadioInterface, d, ipoAudioBuffer, ipoDataBuffer, mpFrameBuffer, dump, flag));
  mBackendList.append(backend);
  //mBackendList.append(new Backend(mpRadioInterface, d, ipoAudioBuffer, ipoDataBuffer, mpFrameBuffer, dump, flag));
  fprintf(stdout, "we have now %d backends running\n", (i32)mBackendList.size());
  return true;
}

//
//	add blocks. First is (should be) block 4, last is (should be) 
//	nrBlocks -1.
//	Note that this method is called from within the ofdm-processor thread
//	while the set_xxx methods are called from within the 
//	gui thread, so some locking is added
//

void MscHandler::process_block(const std::vector<i16> & iSoftBits, const i16 iBlockNr)
{
  const i16 curBlockIdx = (i16)((iBlockNr - 4) % mNumberOfBlocksPerCif);
  //	and the normal operation is:
  memcpy(&mCifVector[curBlockIdx * mBitsPerBlock], iSoftBits.data(), mBitsPerBlock * sizeof(i16));

  if (curBlockIdx < mNumberOfBlocksPerCif - 1)
  {
    return;
  }

  // OK, now we have a full CIF and it seems there is some work to be done.
  // We assume that the backend itself does the work in a separate thread.
  mMutex.lock();
  for (const auto & b: mBackendList)
  {
    const i16 startAddr = b->startAddr;
    const i16 length = b->Length;
    if (length > 0) // Length = 0? should not happen
    {
      b->process(&mCifVector[startAddr * CUSize], length * CUSize);
    }
  }
  mMutex.unlock();
}
