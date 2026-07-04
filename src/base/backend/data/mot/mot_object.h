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

#pragma once

#include "dab_constants.h"
#include "mot_content_types.h"
#include <QObject>
#include <QLabel>
#include <QByteArray>
#include <QString>
#include <QDir>
#include <map>

class DabRadio;

class MotObject : public QObject
{
Q_OBJECT
public:
  MotObject(DabRadio * ipDR, bool iDirElement, bool iPadElement);
  MotObject(DabRadio * ipDR, bool iDirElement, bool iPadElement, u16 iTransportId, const u8 * ipSegment, i32 iSegmentSize, bool iLastFlag);
  ~MotObject() override = default;

  void set_header(const u8 * iSegment, i32 iSegmentSize, bool iLastFlag, i32 iTransportId);
  void add_body_segment(const u8 * iBodySegment, i16 iSegmentNumber, i32 iSegmentSize, bool iLastFlag, i32 iTransportId);
  int get_header_size() const;
  void reset();

private:
  DabRadio * const mpDR;
  const bool mIsDirElement;
  const bool mIsPadElement;

  struct SHeaderCore
  {
    bool initialized = false;
    i32 bodySize = 0;       // 28 bits
    i32 headerSize = 0;     // 13 bits
    i32 contentType = 0;    // part of MOTContentType, 6 bits, see TS 101756 [7], table 17
    i32 contentSubType = 0; // part of MOTContentType, 9 bits, see TS 101756 [7], table 17

    [[nodiscard]] MOTContentType get_content_type() const
    {
      // this method gives not back the exact bit pattern of "(contentType << 9) | contentSubType" but use the logic of MOTContentType
      return static_cast<MOTContentType>(((contentType << 8) & MOTCTBaseTypeMask) | (contentSubType & MOTCTSubTypeMask));
    }
  };

  std::map<i32, QByteArray> mMotMap;
  SHeaderCore mHeaderCore{};
  QString mPicturePath;
  QString mName;
  i32 mTransportId = -1; // -1 = not yet set
  i32 mNumOfSegments = -1;
  i32 mStartSegment = -1;
  i32 mProgressMax = -1;
  i32 mSumSegmentSize = 0;

  bool _check_if_complete();
  void _handle_complete();
  void _process_parameter_id(const u8 * ipSegment, i32 & ioPointer, u8 iParamId, u16 iLength);
  void _process_header_extension(const u8 * iSegment, i32 & ioPointer);

signals:
  void signal_new_mot_object(const QByteArray & oData, const QString & oName, i32 oContentType, bool oDirElement);
  void signal_pad_mot_progress(i32 oPercent);
};
