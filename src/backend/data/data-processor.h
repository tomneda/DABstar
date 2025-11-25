/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2015
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
 */
#ifndef  DATA_PROCESSOR_H
#define  DATA_PROCESSOR_H

#include "frame-processor.h"
#include "ringbuffer.h"
#include "virtual-datahandler.h"
#include <vector>
#include <cstdio>
#include <cstring>
#include <QObject>

class DabRadio;

class DataProcessor : public QObject, public FrameProcessor
{
Q_OBJECT
public:
  DataProcessor(DabRadio * ipDR, const SPacketData * ipPD, RingBuffer<u8> * ipDataBuffer);
  ~DataProcessor() override = default;

  void add_to_frame(const std::vector<u8> &) override;

private:
  DabRadio * const mpDabRadio;
  const i16 mBitRate;
  const u8 mDSCTy;
  const i16 mAppType;
  const i16 mPacketAddress;
  const u8 mDGflag;
  const i16 mFEC_scheme;
  const i16 mSubChannel;
  RingBuffer<u8> * const mpDataBuffer;
  QScopedPointer<VirtualDataHandler> mpDataHandler;

  i16 mExpectedIndex = 0;
  bool mFirstPacket = true; // only to suppress message while startup
  std::vector<u8> mSeriesVec;
  u8 mPacketState;
  i32 mStreamAddress;    // int since we init with -1

  // result handlers
  void _handle_TDC_async_stream(const u8 *, i32);
  void _handle_packets(const u8 *, i32);
  void _handle_packet(const u8 *);

signals:
  void signal_show_MSC_errors(int);
};

#endif

