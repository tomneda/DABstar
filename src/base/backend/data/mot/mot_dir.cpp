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
 *	for handling a MOT directory
 */
#include "mot_dir.h"
#include "mot_object.h"
#include "bit_extractors.h"
#include <QLoggingCategory>
#include <algorithm>

Q_LOGGING_CATEGORY(sLogMotDir, "MotDir", QtWarningMsg)

MotDirectory::MotDirectory(DabRadio* const ipRadio, const u16 iTransportId, const std::span<const u8>& iFirstSegment, const bool iLastSegment, const bool iIsSpi)
  : mpRadioInterface(ipRadio)
  , mTransportId(iTransportId)
  , mIsSpi(iIsSpi)
{
  // ETSI EN 301 234, 7.2.1: RfU(2) DirectorySize(30) NumberOfObjects(16) DataCarouselPeriod(24) RfU(1) RfA(2) SegmentSize(13) DirectoryExtensionLength(16)
  if (iFirstSegment.size() < 13)
  {
    qCWarning(sLogMotDir) << "First segment of MOT directory" << iTransportId << "too short with" << iFirstSegment.size() << "bytes";
    mIsAnalysed = true; // ignore this directory
    return;
  }

  const i32 dirSize = (i32)(((iFirstSegment[0] & 0x3F) << 24) | (iFirstSegment[1] << 16) | (iFirstSegment[2] << 8) | iFirstSegment[3]);
  mNumObjects = (iFirstSegment[4] << 8) | iFirstSegment[5];
  mDirSegmentSize = (i32)iFirstSegment.size();

  if (dirSize < 13 || dirSize > cDirSizeMax)
  {
    qCWarning(sLogMotDir) << "MOT directory" << iTransportId << "has an invalid size of" << dirSize << "bytes";
    mIsAnalysed = true; // ignore this directory
    return;
  }

  qCDebug(sLogMotDir) << "New MOT directory" << iTransportId << "with size" << dirSize << "and" << mNumObjects << "objects";

  mDirData.resize(dirSize);
  mSegmentReceived.resize((dirSize + mDirSegmentSize - 1) / mDirSegmentSize, false);
  _add_segment(iFirstSegment, 0, iLastSegment);
}

MotDirectory::~MotDirectory() = default;

MotObject* MotDirectory::getHandle(const u16 iTransportId) const
{
  for (const auto& mc : mMotComponents)
  {
    if (mc.transportId == iTransportId)
    {
      return mc.pMotObject.get();
    }
  }

  return nullptr;
}

// unfortunately, directory segments do not need to come in in order
void MotDirectory::directorySegment(const u16 iTransportId, const std::span<const u8>& iSegment, const i16 iSegmentNumber, const bool iLastSegment)
{
  if (mTransportId != iTransportId || mIsAnalysed || iSegmentNumber < 0)
  {
    return;
  }

  _add_segment(iSegment, iSegmentNumber, iLastSegment);
}

void MotDirectory::_add_segment(const std::span<const u8>& iSegment, const i32 iSegmentNumber, const bool iLastSegment)
{
  if (iSegmentNumber >= (i32)mSegmentReceived.size())
  {
    qCWarning(sLogMotDir) << "MOT directory" << mTransportId << "segment number" << iSegmentNumber << "out of range";
    return;
  }

  if (mSegmentReceived[iSegmentNumber])
  {
    return;
  }

  const usize offset = (usize)iSegmentNumber * mDirSegmentSize;
  const usize size = std::min(iSegment.size(), mDirData.size() - offset);
  std::copy_n(iSegment.begin(), size, mDirData.begin() + (isize)offset);
  mSegmentReceived[iSegmentNumber] = true;

  if (iLastSegment)
  {
    mNumDirSegments = iSegmentNumber + 1;
  }

  if (mIsSpi)
  {
    _parse_received_prefix();
  }

  // we are "complete" if all segments are "in" (the number of segments is known by the directory size or the last segment)
  const i32 numSegments = (mNumDirSegments >= 0 ? std::min(mNumDirSegments, (i32)mSegmentReceived.size()) : (i32)mSegmentReceived.size());

  for (i32 i = 0; i < numSegments; i++)
  {
    if (!mSegmentReceived[i])
    {
      return;
    }
  }

  _analyse_the_directory();
}

Spi::EContentKind MotDirectory::get_content_kind(const u16 iTransportId) const
{
  if (mIsAnalysed)
  {
    const MotObject* const pMotObject = getHandle(iTransportId);
    return (pMotObject != nullptr ? pMotObject->get_spi_content_kind() : Spi::EContentKind::None);
  }

  const auto it = mEarlyKinds.find(iTransportId);
  return (it != mEarlyKinds.end() ? it->second : Spi::EContentKind::None);
}

// Parses the entries of the directory as far as its leading segments are received without a gap. Only the transportId and
// the content type of the header core are needed (ETSI EN 301 234, 7.2.1), so the kind of an object is known before the
// complete directory is received (which could take a further carousel period if a segment is lost).
void MotDirectory::_parse_received_prefix()
{
  if (mEarlyParseDone)
  {
    return;
  }

  usize numLeadingSegments = 0;
  while (numLeadingSegments < mSegmentReceived.size() && mSegmentReceived[numLeadingSegments])
  {
    ++numLeadingSegments;
  }
  const usize prefixLen = std::min(numLeadingSegments * (usize)mDirSegmentSize, mDirData.size());

  if (mEarlyParsePos == 0)
  {
    if (prefixLen < 13)
    {
      return;
    }
    mEarlyParsePos = 13 + ((mDirData[11] << 8) | mDirData[12]); // skip the directory extension
  }

  constexpr usize cHeaderCoreSize = 7;

  while (mEarlyNumParsed < mNumObjects && mEarlyParsePos + 2 + cHeaderCoreSize <= prefixLen)
  {
    const u8* const pEntry = mDirData.data() + mEarlyParsePos;
    const u16 transportId = (pEntry[0] << 8) | pEntry[1];
    const u8* const pHeaderCore = pEntry + 2;
    const usize headerSize   = extract_bits_from_byte_stream(pHeaderCore, 55-27, 13); // b27..b15
    const i32 contentType    = (i32)extract_bits_from_byte_stream(pHeaderCore, 55-14,  6); // b14..b9
    const i32 contentSubType = (i32)extract_bits_from_byte_stream(pHeaderCore, 55- 8,  9); // b8..b0

    if (transportId == 0 || headerSize < cHeaderCoreSize) // dummy or corrupt entry
    {
      mEarlyParseDone = true;
      return;
    }

    const auto motContentType = static_cast<MOTContentType>(((contentType << 8) & MOTCTBaseTypeMask) | (contentSubType & MOTCTSubTypeMask));
    mEarlyKinds[transportId] = MotObject::content_type_to_spi_kind(motContentType);
    mEarlyParsePos += 2 + headerSize;
    ++mEarlyNumParsed;
  }

  mEarlyParseDone = (mEarlyNumParsed >= mNumObjects);
}

// This is the tough one, we collected the bits, and now we need to extract the MotObjects from it
void MotDirectory::_analyse_the_directory()
{
  mIsAnalysed = true;

  const std::span<const u8> dir(mDirData);
  const usize extensionLength = (dir[11] << 8) | dir[12];
  usize pos = 13 + extensionLength;

  for (i32 i = 0; i < mNumObjects; i++)
  {
    if (pos + 2 + 7 > dir.size()) // transport id and header core
    {
      qCWarning(sLogMotDir) << "MOT directory" << mTransportId << "ends after" << i << "of" << mNumObjects << "objects";
      break;
    }

    const u16 transportId = (dir[pos] << 8) | dir[pos + 1];

    if (transportId == 0) // just a dummy
    {
      break;
    }

    auto pMotObject = std::make_unique<MotObject>(mpRadioInterface, true, false, transportId, dir.subspan(pos + 2), false, mIsSpi);

    if (!pMotObject->has_header())
    {
      qCWarning(sLogMotDir) << "MOT directory" << mTransportId << "contains an invalid header for object" << transportId;
      break;
    }

    pos += 2 + pMotObject->get_header_size();
    mMotComponents.push_back({ transportId, std::move(pMotObject) });
  }

  mEarlyKinds.clear(); // the kinds are now given by the MOT objects

  const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - mCreationTime).count();
  qCDebug(sLogMotDir) << "MOT directory" << mTransportId << "analysed with" << mMotComponents.size() << "objects," << durationMs << "ms after its first segment";
}
