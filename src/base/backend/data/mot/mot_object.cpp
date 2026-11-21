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

#include "mot_object.h"
#include "dabradio.h"
#include "bit_extractors.h"
#include "qt_compat.h"
#include <QLoggingCategory>

// Q_LOGGING_CATEGORY(sLogMotObject, "MotObject", QtDebugMsg)
Q_LOGGING_CATEGORY(sLogMotObject, "MotObject", QtWarningMsg)


MotObject::MotObject(DabRadio* ipDR, const bool iDirElement, const bool iPadElement, const u16 iTransportId, const std::span<const u8>& iSegment, const bool iLastFlag, const bool iIsSpi)
  : mpDR(ipDR)
  , mIsDirElement(iDirElement)
  , mIsPadElement(iPadElement)
  , mIsSpi(iIsSpi)
{
  qCDebug(sLogMotObject()) << "Init MotObject() (1) with dirElement" << iDirElement << "and transportId" << iTransportId << "and segmentSize" << iSegment.size() << "and lastFlag" << iLastFlag;

  _connect_signals();
  set_header(iSegment, iLastFlag, iTransportId);
}

MotObject::MotObject(DabRadio* const ipDR, const bool iDirElement, const bool iPadElement, const bool iIsSpi)
  : mpDR(ipDR)
  , mIsDirElement(iDirElement)
  , mIsPadElement(iPadElement)
  , mIsSpi(iIsSpi)
{
  qCDebug(sLogMotObject()) << "Init MotObject() (2) with dirElement" << iDirElement;

  _connect_signals();
}

void MotObject::_connect_signals()
{
  if (mIsSpi)
  {
    connect(this, &MotObject::signal_new_spi_object, mpDR, &DabRadio::slot_handle_spi_object);
  }
  else
  {
    connect(this, &MotObject::signal_new_mot_object, mpDR, &DabRadio::slot_handle_mot_object);
  }
  connect(this, &MotObject::signal_pad_mot_progress, mpDR, &DabRadio::slot_pad_mot_progress);
}


void MotObject::set_header(const std::span<const u8>& iSegment, const bool iLastFlag, const i32 iTransportId)
{
  qCDebug(sLogMotObject) << "SegmentSize" << iSegment.size() << "LastFlag" << iLastFlag << "TransportId" << iTransportId;

  if (mTransportId != iTransportId) // if transportId changes here this is a new series of segments
  {
    qCDebug(sLogMotObject) << "TransportId changed from" << mTransportId << "to" << iTransportId;
    reset();
  }

  mTransportId = iTransportId;

  if (iSegment.size() < 7)
  {
    qCWarning(sLogMotObject) << "MOT header of" << iTransportId << "too short with" << iSegment.size() << "bytes";
    return;
  }

  // ETSI EN301 234, ch. 6.1 "Header core" — 7-byte big-endian bitfield (b55 = MSB of iSegment[0])
  const u8* const pSegment = iSegment.data();
  mHeaderCore.bodySize       = (i32)extract_bits_from_byte_stream(pSegment, 55-55, 28); // b55..b28
  mHeaderCore.headerSize     = (i32)extract_bits_from_byte_stream(pSegment, 55-27, 13); // b27..b15
  mHeaderCore.contentType    = (i32)extract_bits_from_byte_stream(pSegment, 55-14,  6); // b14..b9
  mHeaderCore.contentSubType = (i32)extract_bits_from_byte_stream(pSegment, 55- 8,  9); // b8..b0

  // check if method mHeaderCore.get_content_type() is compatible
  if (mHeaderCore.contentType > (MOTCTBaseTypeMask >> 8) || mHeaderCore.contentSubType > MOTCTSubTypeMask)
  {
    qCritical() << "Incompatible contentType" << mHeaderCore.contentType << "contentSubType" << mHeaderCore.contentSubType;
  }

  if (mHeaderCore.headerSize < 7 || mHeaderCore.headerSize > (i32)iSegment.size())
  {
    qCWarning(sLogMotObject) << "MOT header of" << iTransportId << "with invalid header size" << mHeaderCore.headerSize << "(" << iSegment.size() << "bytes available)";
    mHeaderCore = {};
    return;
  }

  mHeaderCore.initialized = true;

  qCDebug(sLogMotObject) << "HeaderSize" << mHeaderCore.headerSize << "BodySize" << mHeaderCore.bodySize << "ContentType" << mHeaderCore.contentType << "ContentSubType" << mHeaderCore.contentSubType;

  i32 pointer = 7; // 7 bytes are the HeaderCore size
  const std::span<const u8> header = iSegment.first(mHeaderCore.headerSize);

  while (pointer < (signed)mHeaderCore.headerSize) // Header extension data available?
  {
    if (!_process_header_extension(header, pointer))
    {
      qCWarning(sLogMotObject) << "MOT header extension of" << iTransportId << "exceeds the header size";
      pointer = mHeaderCore.headerSize;
      break;
    }
  }

  if (pointer != (signed)mHeaderCore.headerSize)
  {
    qCritical() << "Header size mismatch" << pointer << "!=" << mHeaderCore.headerSize;
  }

  if (_check_if_complete()) // maybe only the header was missed finally
  {
    _handle_complete();
  }
}

void MotObject::add_body_segment(const std::span<const u8>& iBodySegment, const i16 iSegmentNumber, const bool iLastFlag, const i32 iTransportId)
{
  const i32 iSegmentSize = (i32)iBodySegment.size();
  if (iSegmentNumber < 0 || iSegmentNumber >= 8192)
  {
    qCCritical(sLogMotObject)  << "Invalid segment number" << iSegmentNumber;
    return;
  }

  if (mTransportId != iTransportId)
  {
    qCDebug(sLogMotObject) << "TransportId changed from" << mTransportId << "to" << iTransportId;
    reset();
    mTransportId = iTransportId;
  }

  if (mMotMap.find(iSegmentNumber) == mMotMap.end())
  {
    qCDebug(sLogMotObject) << "Adding segment" << iSegmentNumber << "to motMap with size" << iSegmentSize << "- lastFlag" << iLastFlag << "- transportId" << mTransportId;
    QByteArray segment(reinterpret_cast<const char*>(iBodySegment.data()), iSegmentSize);
    mMotMap.emplace(iSegmentNumber, std::move(segment));
    mSumSegmentSize += iSegmentSize;
  }
  else
  {
    qCDebug(sLogMotObject) << "Duplicate segment" << iSegmentNumber << "to motMap with size" << iSegmentSize << "- lastFlag" << iLastFlag << "- transportId" << mTransportId;
    return;
  }

  if (iLastFlag) // now we know how many segments there are
  {
    mNumOfSegments = iSegmentNumber + 1;
  }

  if (mStartSegment < 0)
  {
    mStartSegment = iSegmentNumber;
  }

  if (mNumOfSegments > 0 && mProgressMax < 0)
  {
    mProgressMax = mNumOfSegments - 1;
  }

  if (mIsPadElement && mHeaderCore.bodySize > 0 && getContentBaseType(mHeaderCore.get_content_type()) == MOTBaseTypeImage) // emit only picture based content
  {
    i32 loadedPart = 100 * mSumSegmentSize / mHeaderCore.bodySize;
    if (loadedPart > 100)
    {
      qCWarning(sLogMotObject) << "Loaded part is greater than 100% with" << loadedPart;
      loadedPart = 100;
    }
    emit signal_pad_mot_progress(loadedPart);
  }

  if (_check_if_complete()) // hopefully we collected all segments already
  {
    _handle_complete();
  }
}

void MotObject::_process_parameter_id(const u8* const ipSegment, i32& ioPointer, const u8 iParamId, const u16 iLength)
{
  // ioPointer points to the parameter data with iLength bytes, the pointer is moved behind the data in any case
  const QByteArray data(reinterpret_cast<const char*>(ipSegment + ioPointer), iLength);

  switch (iParamId)
  {
  case 0x0C: // ContentName, the first byte contains the character set indicator
    mName.clear();
    for (u16 i = 1; i < iLength; i++)
    {
      mName.append((QChar)ipSegment[ioPointer + i]);
    }
    qCDebug(sLogMotObject) << "MOT object" << mTransportId << "has name" << mName;
    break;

  case 0x11: mSpiParams.isCompressed = true; break; // CompressionType
  case 0x21: mSpiParams.profileSubset = data; break; // ProfileSubset
  case 0x23: mSpiParams.isEncrypted = true; break;   // CAInfo
  case 0x25: mSpiParams.scopeStart = data; break;    // ScopeStart (ETSI TS 102 371, 6.4.6)
  case 0x26: mSpiParams.scopeEnd = data; break;      // ScopeEnd (ETSI TS 102 371, 6.4.7)
  case 0x27: mSpiParams.scopeId = data; break;       // ScopeID (ETSI TS 102 371, 6.4.8)

  case 0x01:  // PermitOutdatedVersions
  case 0x02:  // creation time
  case 0x03:  // start validity
  case 0x04:  // expiretime
  case 0x05:  // triggerTime
  case 0x06:  // version number
  case 0x07:  // retransmission distance
  case 0x08:  // group reference
  case 0x0A:  // priority
  case 0x0B:  // label
  case 0x0D:  // UniqueBodyVersion
  case 0x0F:  // content description
  case 0x10:  // MimeType
    break;

  default:
    qCDebug(sLogMotObject) << "MOT object" << mTransportId << "has unknown parameter" << iParamId << "of length" << iLength;
    break;
  }

  ioPointer += iLength;
}

bool MotObject::_process_header_extension(const std::span<const u8>& iSegment, i32& ioPointer)
{
  const i32 size = (i32)iSegment.size();
  const u8* const pSegment = iSegment.data();
  const u8 PLI = (pSegment[ioPointer] & 0xC0) >> 6;
  const u8 paramId = (pSegment[ioPointer] & 0x3F);

  u16 length = 0;

  switch (PLI) // PLI = Parameter Length Indicator
  {
  case 0x0: length = 0; ioPointer += 1; break;
  case 0x1: length = 1; ioPointer += 1; break;
  case 0x2: length = 4; ioPointer += 1; break;
  case 0x3:
    if (ioPointer + 1 >= size)
    {
      return false;
    }
    if ((pSegment[ioPointer + 1] & 0x80) != 0) // Ext field: 7 or 15 bit length field is used
    {
      if (ioPointer + 2 >= size)
      {
        return false;
      }
      length = (pSegment[ioPointer + 1] & 0x7F) << 8 | pSegment[ioPointer + 2];
      ioPointer += 3;
    }
    else
    {
      length = pSegment[ioPointer + 1] & 0x7F;
      ioPointer += 2;
    }
    break;
  default:; // never happened as we have only 2 bits to process
  }

  if (ioPointer + length > size)
  {
    return false;
  }

  qCDebug(sLogMotObject) << "Process parameter id" << paramId << "with length" << length << "at pointer" << ioPointer;
  _process_parameter_id(pSegment, ioPointer, paramId, length);
  return true;
}

bool MotObject::_check_if_complete()
{
  if (!mHeaderCore.initialized)
  {
    qCDebug(sLogMotObject) << "MOT object" << mTransportId << "not complete yet, missing header core information";
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
      qCWarning(sLogMotObject) << "MOT object" << mTransportId << "not complete yet, missing segment" << i;
      missed = true; // do not break, show all possible missing segments
    }
  }

  if (missed)
  {
    qCWarning(sLogMotObject) << "MOT object" << mTransportId << "not complete yet, missing at least one element";
    return false;
  }

  return true;
}


void MotObject::_handle_complete()
{
  assert(mHeaderCore.initialized);
  qCDebug(sLogMotObject) << "MOT object" << mTransportId << "complete with mNumOfSegments" << mNumOfSegments << "and" << mMotMap.size() << "segments in motMap";

  QByteArray result;
  result.reserve(mHeaderCore.bodySize);
  for (const auto& it : mMotMap)
  {
    result.append(it.second);
  }

  if (mName.isEmpty())
  {
    mName = QSL("trid_") + QString::number(mTransportId);
    qCDebug(sLogMotObject) << "MOT object" << mTransportId << "has no name, using" << mName;
  }

  if (mIsSpi)
  {
    qCDebug(sLogMotObject) << "emit signal_new_spi_object" << mTransportId << "with name" << mName << "and content type" << mHeaderCore.get_content_type();
    SMotSpiObject spiObject = mSpiParams;
    spiObject.body = result;
    spiObject.contentName = mName;
    spiObject.contentType = mHeaderCore.get_content_type();
    emit signal_new_spi_object(spiObject);
    return;
  }

  qCDebug(sLogMotObject) << "emit signal_new_mot_object" << mTransportId << "with name" << mName << "and content type" << mHeaderCore.get_content_type() << "and dirElement" << mIsDirElement;
  emit signal_new_mot_object(result, mName, mHeaderCore.get_content_type(), mIsDirElement);
}

int MotObject::get_header_size() const
{
  if (!mHeaderCore.initialized)
  {
    qCritical() << "Header size not known yet";
    return 0;
  }
  return mHeaderCore.headerSize;
}

Spi::EContentKind MotObject::get_spi_content_kind() const
{
  if (!mHeaderCore.initialized)
  {
    return Spi::EContentKind::None;
  }

  const MOTContentType contentType = mHeaderCore.get_content_type();

  switch (getContentBaseType(contentType))
  {
  case MOTBaseTypeImage: return Spi::EContentKind::Logo; // images of an SPI service are logos
  case MOTBaseTypeApplication:
    // ETSI TS 102 371, table 12
    switch (getContentSubType(contentType))
    {
    case (u8)Spi::EContentSubType::ServiceInformation:   return Spi::EContentKind::ServiceInformation;
    case (u8)Spi::EContentSubType::ProgrammeInformation:
    case (u8)Spi::EContentSubType::GroupInformation:     return Spi::EContentKind::ProgrammeInformation;
    default: return Spi::EContentKind::None;
    }
  default: return Spi::EContentKind::None;
  }
}

void MotObject::reset()
{
  qCDebug(sLogMotObject) << "reset()";
  mStartSegment = -1;
  mProgressMax = -1;
  mNumOfSegments = -1;
  mSumSegmentSize = 0;
  mHeaderCore = {};
  mName.clear();
  mMotMap.clear();
  mSpiParams = {};
}
