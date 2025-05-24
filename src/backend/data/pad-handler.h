/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013 .. 2017
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

#ifndef  PAD_HANDLER_H
#define  PAD_HANDLER_H

#include  <QObject>
#include  <cstring>
#include  <cstdint>
#include  <vector>

class DabRadio;
class MotObject;

class PadHandler : public QObject
{
Q_OBJECT
public:
  explicit PadHandler(DabRadio *);
  ~PadHandler() override;

  void process_PAD(uint8_t *, int16_t, uint8_t, uint8_t);

private:
  DabRadio * const mpRadioInterface;
  void _handle_variable_PAD(const uint8_t *, int16_t, uint8_t);
  void _handle_short_PAD(const uint8_t *, int16_t, uint8_t);
  void _dynamic_label(const uint8_t *, int16_t, uint8_t);
  void _new_MSC_element(const std::vector<uint8_t> &);
  void _add_MSC_element(const std::vector<uint8_t> &);
  void _build_MSC_segment(const std::vector<uint8_t> &);
  bool _pad_crc(const uint8_t *, int16_t);
  void _reset_charset_change();
  void _check_charset_change();

  QByteArray mDynamicLabelTextUnConverted;
  int16_t mCharSet = 0;
  int16_t mLastConvCharSet = -1;
  MotObject * mpCurrentSlide = nullptr;
  uint8_t mLastAppType = 0;
  bool mMscGroupElement = false;
  int32_t mXPadLength = -1;
  int16_t mStillToGo = 0;
  std::vector<uint8_t> mShortPadData;
  bool mLastSegment = false;
  bool mFirstSegment = false;
  int16_t mSegmentNumber = -1;
  int32_t mDataGroupLength = 0; // mDataGroupLength is set when having processed an appType 1
  std::vector<uint8_t> mMscDataGroupBuffer; // The msc_dataGroupBuffer is used for assembling the msc_data group.

signals:
  void signal_show_label(const QString &);
  void signal_show_mot_handling(bool);
};

#endif
