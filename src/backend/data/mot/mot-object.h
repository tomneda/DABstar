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

#ifndef  MOT_OBJECT_H
#define  MOT_OBJECT_H

#include  "dab-constants.h"
#include  "mot-content-types.h"
#include  <QObject>
#include  <QImage>
#include  <QLabel>
#include  <QByteArray>
#include  <QString>
#include  <QDir>
#include  <map>
#include  <iterator>

class DabRadio;

class MotObject : public QObject
{
Q_OBJECT
public:
  MotObject(DabRadio * mr, bool dirElement);
  MotObject(DabRadio * mr, bool dirElement, u16 transportId, const u8 * segment, i32 segmentSize, bool lastFlag);
  ~MotObject() override = default;

  void set_header(const u8 * segment, i32 segmentSize, bool lastFlag, i32 transportId);
  void add_body_segment(const u8 * bodySegment, i16 segmentNumber, i32 segmentSize, bool lastFlag, i32 transportId);
  //bool check_and_set_transport_id(u16 transportId);
  //u16 get_transport_id();
  int get_header_size();
  void reset();

private:
  DabRadio * const mpDR;
  const bool mDirElement;
  i32 mTransportId = -1; // -1 = not yet set
  QString mPicturePath;
  i16 mNumOfSegments = -1;
  i32 mSegmentSize = -1;
  u32 mHeaderSize = 0;
  u32 mBodySize = 0;
  MOTContentType mContentType = (MOTContentType)0;
  QString mName;
  std::map<int, QByteArray> mMotMap;

  bool _check_if_complete();
  void _handle_complete();
  void _process_parameter_id(const u8 * ipSegment, i32 & ioPointer, u8 iParamId, u16 iLength);

signals:
  void signal_new_MOT_object(const QByteArray &, const QString &, int, bool);
};

#endif

