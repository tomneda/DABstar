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
 *
 *	MOT handling is a crime, here we have a single class responsible
 *	for handling a single MOT message with a given transportId
 */

#include "mot-object.h"
#include "dabradio.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(sLogMotObject, "MotObject", QtWarningMsg)

MotObject::MotObject(DabRadio * mr, bool dirElement, u16 transportId, const u8 * segment, i32 segmentSize, bool lastFlag)
  : mTransportId(transportId)
  , mDirElement(dirElement)
{
  (void)segmentSize;
  (void)lastFlag;

  connect(this, &MotObject::signal_new_MOT_object, mr, &DabRadio::slot_handle_mot_object);

  mHeaderSize = /*((segment [3] & 0x0F) << 9) |*/ (segment[4] << 1) | ((segment[5] >> 7) & 0x01);
  mBodySize = (segment[0] << 20) | (segment[1] << 12) | (segment[2] << 4) | ((segment[3] & 0xF0) >> 4);

  // Extract the content type
  //	int b	= (segment [5] >> 1) & 0x3F;
  u16 rawContentType = 0;
  rawContentType |= ((segment[5] >> 1) & 0x3F) << 8;
  rawContentType |= ((segment[5] & 0x01) << 8) | segment[6];
  mContentType = static_cast<MOTContentType>(rawContentType);

  //	fprintf (stdout, "headerSize %d, bodySize %d. contentType %d, transportId %d, segmentSize %d, lastFlag %d\n",
  //	                  headerSize, bodySize, b, transportId,
  //	                  segmentSize, lastFlag);
  //	we are actually only interested in the name, if any

  i32 pointer = 7;

  while ((u16)pointer < mHeaderSize)
  {
    u8 PLI = (segment[pointer] & 0300) >> 6;
    u8 paramId = (segment[pointer] & 077);
    u16 length = 0;

    switch (PLI)
    {
    case 00: pointer += 1; break;
    case 01: pointer += 2; break;
    case 02: pointer += 5; break;
    case 03:
      if ((segment[pointer + 1] & 0200) != 0)
      {
        length = (segment[pointer + 1] & 0177) << 8 | segment[pointer + 2];
        pointer += 3;
      }
      else
      {
        length = segment[pointer + 1] & 0177;
        pointer += 2;
      }

      switch (paramId)
      {
      case 12:
      {
        for (i16 i = 0; i < length - 1; i++)
        {
          mName.append((QChar)segment[pointer + i + 1]);
        }
      }
        pointer += length;
        break;

      case 2:  // creation time
      case 3:  // start validity
      case 4:  // expiretime
      case 5:  // triggerTime
      case 6:  // version number
      case 7:  // retransmission distance
      case 8:  // group reference
      case 10:  // priority
      case 11:  // label
      case 15:  // content description
        pointer += length;
        break;

      default: pointer += mHeaderSize;  // this is so wrong!!!
        break;
      }
    }
  }

  //	fprintf (stdout, "creating mot object %x\n", transportId);
}

u16 MotObject::get_transport_id()
{
  return mTransportId;
}

//      type 4 is a segment.
//	The pad/dir software will only call this whenever it has
//	established that the current slide has the right transportId
//
//	Note that segments do not need to come in in the right order
void MotObject::add_body_segment(const u8 * bodySegment, i16 segmentNumber, i32 segmentSize, bool lastFlag)
{
  if (segmentNumber < 0 || segmentNumber >= 8192)
  {
    return;
  }

  if (mMotMap.find(segmentNumber) != mMotMap.end())
  {
    qCDebug(sLogMotObject) << "Duplicate segment number" << segmentNumber;
    return;
  }

  //      Note that the last segment may have a different size
  if (!lastFlag && (this->mSegmentSize == -1))
  {
    this->mSegmentSize = segmentSize;
  }

  QByteArray segment;
  segment.resize(segmentSize);

  for (i32 i = 0; i < segmentSize; i++)
  {
    segment[i] = bodySegment[i];
  }

  qCDebug(sLogMotObject) << "Adding segment" << segmentNumber << "to motMap with size" << segmentSize;
  mMotMap.insert(std::make_pair(segmentNumber, segment));

  if (lastFlag)
  {
    mNumofSegments = segmentNumber + 1;
  }

  if (mNumofSegments == -1)
  {
    return;
  }

  // shortcut map search if we have more segments than collected so far
  if (mNumofSegments > (signed)mMotMap.size())
  {
    qCDebug(sLogMotObject) << "MOT object" << mTransportId << "with segment number" << mNumofSegments << "is larger than collected" << mMotMap.size();
    return;
  }

  //	once we know how many segments there are/should be, we check for completeness
  for (i32 i = 0; i < mNumofSegments; i++)
  {
    if (mMotMap.find(i) == mMotMap.end())
    {
      qCDebug(sLogMotObject) << "MOT object" << mTransportId << "not complete yet, missing at least segment" << i;
      return;
    }
  }

  qCDebug(sLogMotObject) << "MOT object" << mTransportId << "complete with numofSegments" << mNumofSegments << "and" << mMotMap.size() << "segments in motMap";
  //	The motObject is (seems to be) complete
  _handle_complete();
}


void MotObject::_handle_complete()
{
  QByteArray result;
  for (const auto & it : mMotMap)
  {
    result.append(it.second);
  }

  if (mName.isEmpty())
  {
    mName = QString("trid_") + QString::number(get_transport_id());
  }

  emit signal_new_MOT_object(result, mName, (int)mContentType, mDirElement);
}

int MotObject::get_header_size()
{
  return mHeaderSize;
}
