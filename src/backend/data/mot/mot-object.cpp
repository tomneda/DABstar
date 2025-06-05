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

Q_LOGGING_CATEGORY(sLogMotObject, "MotObject", QtDebugMsg)
//Q_LOGGING_CATEGORY(sLogMotObject, "MotObject", QtWarningMsg)


MotObject::MotObject(DabRadio * mr, bool dirElement, u16 transportId, const u8 * segment, i32 segmentSize, bool lastFlag)
  : mpDR(mr)
  , mDirElement(dirElement)
{
  (void)segmentSize;
  (void)lastFlag;

  qCDebug(sLogMotObject()) << "Init MotObject() (1) with dirElement" << dirElement << "and transportId" << transportId << "and segmentSize" << segmentSize << "and lastFlag" << lastFlag;

  connect(this, &MotObject::signal_new_MOT_object, mr, &DabRadio::slot_handle_mot_object);

  //reset();

  set_header(segment, segmentSize, lastFlag, transportId);
}

MotObject::MotObject(DabRadio * mr, bool dirElement)
  : mpDR(mr)
  , mDirElement(dirElement)
{
  qCDebug(sLogMotObject()) << "Init MotObject() (2) with dirElement" << dirElement;

  connect(this, &MotObject::signal_new_MOT_object, mr, &DabRadio::slot_handle_mot_object);

  //reset();
}


void MotObject::set_header(const u8 * segment, i32 segmentSize, bool lastFlag, i32 transportId)
{
  qCDebug(sLogMotObject) << "set_header()" << "segmentSize" << segmentSize << "lastFlag" << lastFlag << "transportId" << transportId;

  if (mTransportId != transportId) // if transportId changes here this is a new series of segments
  {
    reset();
  }

  // if (lastFlag) // now we know how many segments there are
  // {
  //   mNumOfSegments = segmentNumber + 1;
  // }

  mTransportId = transportId;
  mSegmentSize = segmentSize;
  mHeaderSize = /*((segment [3] & 0x0F) << 9) |*/ (segment[4] << 1) | ((segment[5] >> 7) & 0x01);
  mBodySize = (segment[0] << 20) | (segment[1] << 12) | (segment[2] << 4) | ((segment[3] & 0xF0) >> 4);

  // Extract the content type
  //	int b	= (segment [5] >> 1) & 0x3F;
  u16 rawContentType = 0;
  rawContentType |= ((segment[5] >> 1) & 0x3F) << 8;
  rawContentType |= ((segment[5] & 0x01) << 8) | segment[6];
  mContentType = static_cast<MOTContentType>(rawContentType);

  i32 pointer = 7;

  while ((u16)pointer < mHeaderSize)
  {
    const u8 PLI = (segment[pointer] & 0xC0) >> 6;
    const u8 paramId = (segment[pointer] & 0x3F);
    u16 length = 0;

    // qCDebug(sLogMotObject) << "Process PLI" << PLI << "at pointer" << pointer;

    switch (PLI)
    {
    case 0x0: pointer += 1; break;
    case 0x1: pointer += 2; break;
    case 0x2: pointer += 5; break;
    case 0x3:
      if ((segment[pointer + 1] & 0x80) != 0)
      {
        length = (segment[pointer + 1] & 0x7F) << 8 | segment[pointer + 2];
        pointer += 3;
      }
      else
      {
        length = segment[pointer + 1] & 0x7F;
        pointer += 2;
      }
      qCDebug(sLogMotObject()) << "Process parameter id with length" << length << "at pointer" << pointer << "for paramId" << paramId;
      _process_parameter_id(segment, pointer, paramId, length);
    default:; // never happened as we have only 2 bits to process
    }
  }

  if (_check_if_complete()) // maybe only the header was missed finally
  {
    _handle_complete();
  }
}


void MotObject::_process_parameter_id(const u8 * const ipSegment, i32 & ioPointer, const u8 iParamId, const u16 iLength)
{
  switch (iParamId)
  {
  case 0x0C:
    for (u16 i = 0; i < iLength - 1; i++)
    {
      mName.append((QChar)ipSegment[ioPointer + i + 1]);
    }
    qCDebug(sLogMotObject) << "MOT object" << mTransportId << "has name" << mName;
    ioPointer += iLength;
    break;

  case 0x02:  // creation time
  case 0x03:  // start validity
  case 0x04:  // expiretime
  case 0x05:  // triggerTime
  case 0x06:  // version number
  case 0x07:  // retransmission distance
  case 0x08:  // group reference
  case 0x0A:  // priority
  case 0x0B:  // label
  case 0x0F:  // content description
    ioPointer += iLength;
    break;

  default: ioPointer += mHeaderSize;  // this is so wrong!!!
    break;
  }
}

// u16 MotObject::get_transport_id()
// {
//   return mTransportId;
// }

//      type 4 is a segment.
//	The pad/dir software will only call this whenever it has
//	established that the current slide has the right transportId
//
//	Note that segments do not need to come in in the right order
void MotObject::add_body_segment(const u8 * bodySegment, i16 segmentNumber, i32 segmentSize, bool lastFlag, i32 transportId)
{
  if (segmentNumber < 0 || segmentNumber >= 8192)
  {
    qCCritical(sLogMotObject)  << "Invalid segment number" << segmentNumber;
    return;
  }

  // Note that the last segment may have a different size (but what happens if there is only one (last) segment?!)
  if (!lastFlag && (mSegmentSize == -1))
  {
    mSegmentSize = segmentSize;
  }

  if (mTransportId != transportId)
  {
    reset();
    qCDebug(sLogMotObject) << "TransportId changed from" << mTransportId << "to" << transportId;
    mTransportId = transportId;
  }

  if (mMotMap.find(segmentNumber) == mMotMap.end())
  {
    qCDebug(sLogMotObject) << "Adding segment" << segmentNumber << "to motMap with size" << segmentSize << "- lastFlag" << lastFlag << "- transportId" << mTransportId;
    QByteArray segment;
    segment.resize(segmentSize);

    for (i32 i = 0; i < segmentSize; i++)
    {
      segment[i] = bodySegment[i]; // TODO: check if there is a way without the copy
    }

    mMotMap.insert(std::make_pair(segmentNumber, segment));
  }
  else
  {
    qCDebug(sLogMotObject) << "Duplicate segment" << segmentNumber << "to motMap with size" << segmentSize << "- lastFlag" << lastFlag << "- transportId" << mTransportId;
    return;
  }

  if (lastFlag) // now we know how many segments there are
  {
    mNumOfSegments = segmentNumber + 1;
  }

  if (_check_if_complete()) // hopefully we collected all segments already
  {
    _handle_complete();
  }
}

bool MotObject::_check_if_complete()
{
  if (mSegmentSize < 0)
  {
    qCDebug(sLogMotObject) << "MOT object" << mTransportId << "not complete yet, missing segment size";
    return false;
  }

  if (mNumOfSegments < 0)
  {
    qCDebug(sLogMotObject) << "MOT object" << mTransportId << "not complete yet, missing number of segments";
    return false;
  }

  if ((i16)mMotMap.size() < mNumOfSegments)
  {
    qCDebug(sLogMotObject) << "MOT object" << mTransportId << "not complete yet, missing" << mNumOfSegments - (i16)mMotMap.size() << "segments in motMap";
    return false;
  }

  // once we know how many segments there are/should be, we check for completeness
  bool missed = false;
  for (i32 i = 0; i < mNumOfSegments; i++)
  {
    if (mMotMap.find(i) == mMotMap.end())
    {
      qCDebug(sLogMotObject) << "MOT object" << mTransportId << "not complete yet, missing segment" << i;
      missed = true; // show all possible missing segments
    }
  }

  if (missed)
  {
    qCDebug(sLogMotObject) << "MOT object" << mTransportId << "not complete yet, missing at least one element";
    return false;
  }

  return true;
}


void MotObject::_handle_complete()
{
  qCDebug(sLogMotObject) << "MOT object" << mTransportId << "complete with mNumOfSegments" << mNumOfSegments << "and" << mMotMap.size() << "segments in motMap";

  QByteArray result;
  for (const auto & it : mMotMap)
  {
    result.append(it.second);
  }

  if (mName.isEmpty())
  {
    qCDebug(sLogMotObject) << "MOT object" << mTransportId << "has no name, using trid_" << mTransportId;
    mName = QString("trid_") + QString::number(mTransportId);
  }

  qCDebug(sLogMotObject) << "emit signaL_new_MOT_object" << mTransportId << "with name" << mName << "and content type" << (int)mContentType << "and dirElement" << mDirElement;
  emit signal_new_MOT_object(result, mName, (int)mContentType, mDirElement);

  reset();
}

int MotObject::get_header_size()
{
  return mHeaderSize;
}

void MotObject::reset()
{
  qCDebug(sLogMotObject) << "reset()";
  // mTransportId = -1; // -1 = not yet set
  mNumOfSegments = -1;
  mSegmentSize = -1;
  mHeaderSize = 0;
  mBodySize = 0;
  mContentType = (MOTContentType)0;
  mName.clear();
  mMotMap.clear();
}
