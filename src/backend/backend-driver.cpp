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
#include  "backend-driver.h"
#include  "mp2processor.h"
#include  "mp4processor.h"
#include  "data-processor.h"

// Driver program for the selected backend. Embodying that in a separate class simplifies the "Backend" class.

BackendDriver::BackendDriver(DabRadio * ipDR, const SDescriptorType * ipDT, RingBuffer<i16> * const ipAudioBuffer, RingBuffer<u8> * const ipDataBuffer, RingBuffer<u8> * const ipFrameBuffer)
{
  if (ipDT->TMId == ETMId::StreamModeAudio)
  {
    if (((SAudioData *)ipDT)->ASCTy != 077)
    {
      mpFrameProcessor = std::make_unique<Mp2Processor>(ipDR, ipDT->bitRate, ipAudioBuffer, ipFrameBuffer);
    }
    else // if (((AudioData *)d)->ASCTy == 077)
    {
      mpFrameProcessor = std::make_unique<Mp4Processor>(ipDR, ipDT->bitRate, ipAudioBuffer, ipFrameBuffer);
    }
  }
  else if (ipDT->TMId == ETMId::PacketModeData)
  {
    mpFrameProcessor = std::make_unique<DataProcessor>(ipDR, static_cast<const SPacketData *>(ipDT), ipDataBuffer);
  }
  else
  {
    mpFrameProcessor = std::make_unique<FrameProcessor>();
  }
}

void BackendDriver::add_to_frame(const std::vector<u8> & iData) const
{
  mpFrameProcessor->add_to_frame(iData);
}

