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

#pragma once

#include "mot_object.h"
#include "charsets.h"
#include <cstring>
#include <cstdint>
#include <array>
#include <atomic>
#include <memory>
#include <span>
#include <vector>
#include <QObject>
#include <QScopedPointer>

class DabRadio;
class MotObject;
class VirtualDataHandler;

class PadHandler : public QObject
{
  Q_OBJECT

public:
  explicit PadHandler(DabRadio*);
  ~PadHandler() override;

  void process_PAD(const u8* iBuffer, i16 iLast, u8 iL1, u8 iL0);

public slots:
  void slot_set_xpad_data_group_app_type(EXPadDataGroupApp iApp, i16 iXPadAppTy); // iXPadAppTy = -1 disables the application in the X-PAD

private:
  DabRadio* const mpRadioInterface;
  void _handle_variable_PAD(const u8* iBuffer, i16 iLast, bool iCiFlag);
  void _handle_short_PAD(const u8* iBuffer, i16 iLast, bool iCIFlag);
  void _dynamic_label(const std::vector<u8>&, u8 iApplType);
  void _new_MSC_element(const std::vector<u8>&);
  void _add_MSC_element(const std::vector<u8>&);
  void _build_MSC_segment(const std::span<const u8>& iData);
  void _handle_xpad_data_group_app(u8 iAppType, const std::vector<u8>& iData, bool iContinuationOnly);
  void _new_xpad_data_group_element(EXPadDataGroupApp iApp, const std::vector<u8>& iData);
  void _add_xpad_data_group_element(EXPadDataGroupApp iApp, const std::vector<u8>& iData);
  void _emit_xpad_data_group_if_complete(EXPadDataGroupApp iApp);
  void _slot_handle_xpad_data_group(EXPadDataGroupApp iApp, const QByteArray& iDataGroup) const;
  bool _pad_crc(const u8*, i16);
  void _reset_charset_change();
  void _check_charset_change();


  QByteArray mDynamicLabelTextUnConverted;
  ECharacterSet mCharSet = ECharacterSet::EbuLatin;
  ECharacterSet mLastConvCharSet = ECharacterSet::Invalid;
  QScopedPointer<MotObject> mpMotObject;
  u8 mLastAppType = 0;
  bool mMscGroupElement = false;
  i32 mXPadLength = -1;
  i16 mStillToGo = 0;
  std::vector<u8> mShortPadData;
  bool mLastSegment = false;
  bool mFirstSegment = false;
  i16 mSegmentNumber = -1;
  i32 mDataGroupLength = 0; // mDataGroupLength is set when having processed an appType 1
  std::vector<u8> mMscDataGroupBuffer; // The msc_dataGroupBuffer is used for assembling the msc_data group.
  std::vector<u8> mDataBuffer; // data buffer to revert the input data
  i32 mSegmentNo = -1;
  i16 mRemainDataLength = 0;
  bool mIsLastSegment = false;
  bool mMoreXPad = false;

  // User applications carried as MSC data groups in the X-PAD (signalled via FIG 0/13 of the audio service component),
  // the collected data groups are given to the same data handlers as used for packet mode services
  struct SXPadDataGroupApp
  {
    std::atomic<i16> XPadAppTy{-1}; // X-PAD AppTy for the start of a data group, continuation is +1
    std::vector<u8> dataGroupBuffer; // only accessed in the backend thread
    i32 dataGroupLength = 0;
    std::unique_ptr<VirtualDataHandler> pDataHandler; // only accessed in the GUI thread
  };
  std::array<SXPadDataGroupApp, cNumXPadDataGroupApps> mXPadDataGroupApps; // index EXPadDataGroupApp

signals:
  void signal_show_label(const QString&);
  void signal_show_mot_handling();  // triggers MOT indicator to "green" for a while
  void signal_xpad_data_group(EXPadDataGroupApp oApp, const QByteArray& oDataGroup);
};

