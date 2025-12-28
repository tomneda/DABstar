/*
 *    Copyright (C) 2014 .. 2017
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

#include "dab-constants.h"
#include "frame-processor.h"
#include "ringbuffer.h"
#include <vector>
#include <memory>

class DabRadio;

class BackendDriver
{
public:
  BackendDriver(DabRadio * ipDR, const SDescriptorType * ipDT, RingBuffer<i16> * ipAudioBuffer, RingBuffer<u8> * ipDataBuffer, RingBuffer<u8> * ipFrameBuffer);
  ~BackendDriver() = default;

  void add_to_frame(const std::vector<u8> & outData) const;

private:
  std::unique_ptr<FrameProcessor> mpFrameProcessor;
};

