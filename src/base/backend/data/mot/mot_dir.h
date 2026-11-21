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
#pragma once

#include "glob_data_types.h"
#include "spi_types.h"
#include <chrono>
#include <map>
#include <memory>
#include <span>
#include <vector>

class DabRadio;
class MotObject;

// Collects the segments of a MOT directory (ETSI EN 301 234, 7.2) and holds the MOT objects described by it
class MotDirectory
{
public:
  MotDirectory(DabRadio* ipRadio, u16 iTransportId, const std::span<const u8>& iFirstSegment, bool iLastSegment, bool iIsSpi);
  ~MotDirectory();

  MotObject* getHandle(u16 iTransportId) const;
  void directorySegment(u16 iTransportId, const std::span<const u8>& iSegment, i16 iSegmentNumber, bool iLastSegment);
  u16 get_transportId() const { return mTransportId; }
  [[nodiscard]] Spi::EContentKind get_content_kind(u16 iTransportId) const; // SPI kind of a listed object, known before the directory is complete

private:
  static constexpr i32 cDirSizeMax = 4 * 1024 * 1024; // protection against corrupt data, SPI allows only 8 kByte

  struct SMotComponent
  {
    u16 transportId;
    std::unique_ptr<MotObject> pMotObject;
  };

  DabRadio* const mpRadioInterface;
  const u16 mTransportId;
  const bool mIsSpi;
  std::vector<u8> mDirData;
  std::vector<bool> mSegmentReceived;
  i32 mDirSegmentSize = 0;
  i32 mNumDirSegments = -1; // known after receiving the last segment
  i32 mNumObjects = 0;
  bool mIsAnalysed = false;
  std::vector<SMotComponent> mMotComponents;
  // the kinds of the objects are taken from the already received leading part of the directory (for the SPI indicators)
  std::map<u16, Spi::EContentKind> mEarlyKinds; // transportId -> kind
  usize mEarlyParsePos = 0;                     // position of the next directory entry to parse, 0 = not started yet
  i32 mEarlyNumParsed = 0;
  bool mEarlyParseDone = false;
  const std::chrono::steady_clock::time_point mCreationTime = std::chrono::steady_clock::now();

  void _add_segment(const std::span<const u8>& iSegment, i32 iSegmentNumber, bool iLastSegment);
  void _parse_received_prefix();
  void _analyse_the_directory();
};
