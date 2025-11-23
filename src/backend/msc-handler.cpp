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

// Interface program for processing the MSC.
// The DabProcessor assumes the existence of an msc-handler, whether a service is selected or not.

constexpr i32 cCUSizeBits = 64;
constexpr i32 cCifSizeBits = 55296; // bits for one CIF (== 864 CUs with a 64 bits), there are 4 CIFs per Frame, need 4 * 18 = 72 symbols
constexpr i32 cNumberOfBlocksPerCif = 18; // 18, 72, 0(?), 36 for DAB-Mode 1..4

// Note: CIF counts from 0 .. 3
MscHandler::MscHandler(DabRadio * const iRI, RingBuffer<u8> * const ipFrameBuffer)
  : mpRadioInterface(iRI)
  , mpFrameBuffer(ipFrameBuffer)
{
  mCifVector.resize(cCifSizeBits);
}

MscHandler::~MscHandler()
{
  QMutexLocker lock(&mMutex);
  for (auto & b: mBackendList)
  {
    b->stopRunning();
    b.reset();
  }
  mBackendList.clear();
}

void MscHandler::reset_channel()
{
  qDebug() << "Channel reset: all services will be stopped";
  QMutexLocker lock(&mMutex);
  for (auto & b: mBackendList)
  {
    b->stopRunning();
    b.reset();
  }
  mBackendList.clear();
}

void MscHandler::stop_service(const i32 iSubChId, const EProcessFlag iProcessFlag)
{
  QMutexLocker lock(&mMutex);
  for (qsizetype i = 0; i < mBackendList.size(); i++)
  {
    if (auto & b = mBackendList[i];
        b->subChId == iSubChId && b->processFlag == iProcessFlag)
    {
      qDebug() << "Stopping SubChannel" << iSubChId;
      b->stopRunning();
      b.reset();
      mBackendList.removeAt(i);
      --i; // we removed one element
    }
  }
}

void MscHandler::stop_all_services()
{
  QMutexLocker lock(&mMutex);
  for (auto & b: mBackendList)
  {
    qDebug() << "Stopping SId" << b->serviceId << "SubChannel" << b->subChId << "ProcessFlag" << (b->processFlag == EProcessFlag::Primary ? "Primary" : "Secondary");
    b->stopRunning();
    b.reset();
  }
  mBackendList.clear();
}

bool MscHandler::is_service_running(const i32 iSubChId, const EProcessFlag iProcessFlag) const
{
  QMutexLocker lock(&mMutex);
  for (const auto & b : mBackendList)
  {
    if (b->subChId == iSubChId && b->processFlag == iProcessFlag)
    {
      return true;
    }
  }
  return false;
}

bool MscHandler::set_channel(const SDescriptorType * d, RingBuffer<i16> * ipoAudioBuffer, RingBuffer<u8> * ipoDataBuffer, const EProcessFlag iProcessFlag)
{
  qInfo() << "Create backend" << mBackendList.size() + 1 << "for SId" << d->SId
          << "and ServiceLabel" << (d->serviceLabel.isEmpty() ? "(Unknown yet)" : d->serviceLabel.trimmed())
          << "Audio" << (ipoAudioBuffer != nullptr) << "Data" << (ipoDataBuffer != nullptr)
          << "ProcessFlag" << (iProcessFlag == EProcessFlag::Primary ? "Primary" : "Secondary");

  QMutexLocker lock(&mMutex);
  const QSharedPointer<Backend> backend(new Backend(mpRadioInterface, d, ipoAudioBuffer, ipoDataBuffer, mpFrameBuffer, iProcessFlag));
  mBackendList.append(backend);
  return true;
}

// Add blocks. First is (should be) block 4, last is (should be) nrBlocks -1.
// Note that this method is called from within the ofdm-processor thread while the set_xxx methods
// are called from within the gui thread, so some locking is added.
void MscHandler::process_block(const std::vector<i16> & iSoftBits, const i32 iBlockNr)
{
  assert(iBlockNr >= 4);
  assert(iSoftBits.size() == c2K);

  const i32 curBlockIdx = (iBlockNr - 4) % cNumberOfBlocksPerCif;
  memcpy(&mCifVector[curBlockIdx * c2K], iSoftBits.data(), c2K * sizeof(i16));

  if (curBlockIdx < cNumberOfBlocksPerCif - 1)
  {
    return;
  }

  // OK, now we have a full CIF and it seems there is some work to be done.
  // We assume that the backend itself does the work in a separate thread.
  QMutexLocker lock(&mMutex);
  for (const auto & b: mBackendList)
  {
    b->process(&mCifVector[b->CuStartAddr * cCUSizeBits], b->CuSize * cCUSizeBits);
  }
}
