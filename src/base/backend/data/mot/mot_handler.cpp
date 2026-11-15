/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2015 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
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

//	Interface between msc packages and real MOT handling
#include "mot_handler.h"
#include "mot_object.h"
#include "mot_dir.h"
#include "dabradio.h"
#include "bit_extractors.h"
#include "crc.h"

MotHandler::MotHandler(DabRadio* const ipRadio, const bool iIsSpi)
  : mpRadioInterface(ipRadio)
  , mIsSpi(iIsSpi)
{
  for (auto& mt: mMotTable)
  {
    mt.orderNumber = -1;
    mt.motSlide = nullptr;
  }
}

MotHandler::~MotHandler()
{
  for (auto& mt: mMotTable)
  {
    if (mt.orderNumber >= 0)
    {
      if (mt.motSlide != nullptr)
      {
        delete mt.motSlide;
        mt.motSlide = nullptr;
      }
    }
  }

  if (mpDirectory != nullptr)
  {
    delete mpDirectory;
  }
}

void MotHandler::add_MSC_data_group(const std::vector<u8>& msc)
{
  const u8* const data = msc.data();
  const bool extensionFlag = getBits_1(data, 0) != 0;
  const bool crcFlag = getBits_1(data, 1) != 0;
  const bool segmentFlag = getBits_1(data, 2) != 0;
  const bool userAccessFlag = getBits_1(data, 3) != 0;
  const u8 groupType = getBits_4(data, 4);
  const u8 CI = getBits_4(data, 8);
  i32 next = 16;    // bits
  bool lastFlag = false;
  u16 segmentNumber = 0;
  bool transportIdFlag = false;
  u16 transportId = 0;
  u8 lengthInd;

  (void)CI;
  if (msc.size() <= 0)
  {
    return;
  }

  if (crcFlag && !check_CRC_bits(msc))
  {
    return;
  }

  if (mIsSpi)
  {
    _signal_activity();
  }

  if (extensionFlag)
  {
    next += 16;
  }

  if (segmentFlag)
  {
    lastFlag = getBits_1(data, next) != 0;
    segmentNumber = getBits(data, next + 1, 15);
    next += 16;
  }

  if (userAccessFlag)
  {
    transportIdFlag = getBits_1(data, next + 3);
    lengthInd = getBits_4(data, next + 4);
    next += 8;
    if (transportIdFlag)
    {
      transportId = getBits(data, next, 16);
    }
    next += lengthInd * 8;
  }

  const i32 sizeInBits = msc.size() - next - (crcFlag != 0 ? 16 : 0);

  if (!transportIdFlag)
  {
    return;
  }

  std::vector<u8> motVector;
  motVector.resize(sizeInBits / 8);
  for (i32 i = 0; i < sizeInBits / 8; i++)
  {
    u8 t = 0;
    for (i32 j = 0; j < 8; j++)
    {
      t = (t << 1) | data[next + 8 * i + j];
    }
    motVector[i] = t;
  }

  if (motVector.size() < 2)
  {
    return;
  }

  u32 segmentSize = ((motVector[0] & 0x1F) << 8) | motVector[1];
  if (motVector.size() < 2 + segmentSize)
  {
    return;
  }
  const std::span<const u8> segment = std::span(motVector).subspan(2, segmentSize);

  switch (groupType)
  {
  case 3:
    if (segmentNumber == 0)
    {
      MotObject* h = getHandle(transportId);
      if (h != nullptr)
      {
        break;
      }
      h = new MotObject(mpRadioInterface, false, false, // not within a directory
                        transportId, segment, lastFlag, mIsSpi);
      setHandle(h, transportId);
    }
    break;

  case 4:
  {
    // Body segments without a known header (data group type 3) or directory entry are dropped. The header is
    // not known yet or the directory is not completely received, the segments are repeated in the carousel.
    MotObject* const h = getHandle(transportId);
    if (h != nullptr)
    {
      h->add_body_segment(segment, segmentNumber, lastFlag, transportId);
    }
  }
    break;

  case 6:
    if (segmentNumber == 0)
    {  // MOT directory
      if (mpDirectory != nullptr)
      {
        if (mpDirectory->get_transportId() == transportId)
        {
          break;
        }
      }  // already existing

      if (mpDirectory != nullptr)  // an old one, replace it
      {
        delete mpDirectory;
      }

      mpDirectory = new MotDirectory(mpRadioInterface, transportId, segment, lastFlag, mIsSpi);
    }
    else
    {
      if ((mpDirectory == nullptr) || (mpDirectory->get_transportId() != transportId))
      {
        break;
      }
      mpDirectory->directorySegment(transportId, segment, (i16)segmentNumber, lastFlag);
    }
    break;

  default: fprintf(stdout, "mot groupType %d\n", groupType);
    return;
  }
}

// Shows the receiving of SPI (EPG) data on the EPG button (called within the backend thread)
void MotHandler::_signal_activity()
{
  const auto now = std::chrono::steady_clock::now();

  if (now - mLastActivityTime < cActivityMinInterval)
  {
    return;
  }

  mLastActivityTime = now;
  DabRadio* const pRadio = mpRadioInterface;
  QMetaObject::invokeMethod(pRadio, [pRadio]() { pRadio->slot_spi_data_received(); }, Qt::QueuedConnection);
}

MotObject* MotHandler::getHandle(u16 transportId) const
{
  for (const auto& mt: mMotTable)
  {
    if ((mt.orderNumber >= 0) && (mt.transportId == transportId))
    {
      return mt.motSlide;
    }
  }

  if (mpDirectory != nullptr)
  {
    return mpDirectory->getHandle(transportId);
  }
  return nullptr;
}

void MotHandler::setHandle(MotObject* h, u16 transportId)
{
  for (auto& mt: mMotTable)
  {
    if (mt.orderNumber == -1)
    {
      mt.orderNumber = mOrderNumber++;
      mt.transportId = transportId;
      mt.motSlide = h;
      return;
    }
  }

  //	if here, the cache is full, so we delete the older than current one
  i32 oldest = mOrderNumber;
  i32 index = 0;

  for (i32 i = 0; i < (i32)mMotTable.size(); i++)
  {
    if (mMotTable[i].orderNumber < oldest)
    {
      oldest = mMotTable[i].orderNumber;
      index = i;
    }
  }

  delete mMotTable[index].motSlide;
  mMotTable[index].orderNumber = mOrderNumber++;
  mMotTable[index].transportId = transportId;
  mMotTable[index].motSlide = h;
}

