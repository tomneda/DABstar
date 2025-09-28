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

#include  <vector>
#include  <cstdio>
#include  <cstring>
#include  <QObject>

#include  "frame-processor.h"
#include  "ringbuffer.h"

class DabRadio;
class virtual_dataHandler;
class SPacketData;

class DataProcessor : public QObject, public FrameProcessor
{
Q_OBJECT
public:
  DataProcessor(DabRadio * mr, const SPacketData * pd, RingBuffer<u8> * dataBuffer);
  ~DataProcessor() override;

  void add_to_frame(const std::vector<u8> &) override;

private:
  DabRadio * myRadioInterface;
  i16 bitRate;
  u8 DSCTy;
  i16 appType;
  i16 packetAddress;
  u8 DGflag;
  i16 FEC_scheme;
  RingBuffer<u8> * dataBuffer;
  i16 expectedIndex;
  std::vector<u8> series;
  u8 packetState;
  i32 streamAddress;    // int since we init with -1

  //	result handlers
  void handleTDCAsyncstream(const u8 *, i32);
  void handlePackets(const u8 *, i32);
  void handlePacket(const u8 *);
  virtual_dataHandler * my_dataHandler;
  
signals:
  void show_mscErrors(int);
};

#endif

