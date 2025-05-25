/*
 *    Copyright (C) 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
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
#ifndef    TDC_DATAHANDLER_H
#define    TDC_DATAHANDLER_H

#include  "dab-constants.h"
#include  <vector>
#include  "virtual-datahandler.h"
#include  "ringbuffer.h"

class DabRadio;

class tdc_dataHandler : public virtual_dataHandler
{
Q_OBJECT
public:
  tdc_dataHandler(DabRadio *, RingBuffer<u8> *, i16);
  ~tdc_dataHandler() override = default;

  void add_mscDatagroup(const std::vector<u8> &) override;

private:
  DabRadio * myRadioInterface;
  RingBuffer<u8> * dataBuffer;

  i32 handleFrame_type_0(u8 * data, i32 offset, i32 length);
  i32 handleFrame_type_1(u8 * data, i32 offset, i32 length);
  bool serviceComponentFrameheaderCRC(const u8 *, i16, i16);

signals:
  void bytesOut(int, int);
};

#endif



