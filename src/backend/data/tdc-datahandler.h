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

class IDabRadio;

class tdc_dataHandler : public virtual_dataHandler
{
Q_OBJECT
public:
  tdc_dataHandler(IDabRadio *, RingBuffer<uint8_t> *, int16_t);
  ~tdc_dataHandler() override = default;

  void add_mscDatagroup(const std::vector<uint8_t> &) override;

private:
  IDabRadio * myRadioInterface;
  RingBuffer<uint8_t> * dataBuffer;

  int32_t handleFrame_type_0(uint8_t * data, int32_t offset, int32_t length);
  int32_t handleFrame_type_1(uint8_t * data, int32_t offset, int32_t length);
  bool serviceComponentFrameheaderCRC(const uint8_t *, int16_t, int16_t);

signals:
  void bytesOut(int, int);
};

#endif



