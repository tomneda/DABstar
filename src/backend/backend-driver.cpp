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

//	Driver program for the selected backend. Embodying that in a separate class makes the "Backend" class simpler.

BackendDriver::BackendDriver(DabRadio * mr, const DescriptorType * d, RingBuffer<i16> * audioBuffer, RingBuffer<u8> * dataBuffer, RingBuffer<u8> * frameBuffer)
{
  if (d->TMId == ETMId::StreamModeAudio)
  {
    if (((AudioData *)d)->ASCTy != 077)
    {
      theProcessor.reset(new Mp2Processor(mr, d->bitRate, audioBuffer, frameBuffer));
    }
    else // if (((AudioData *)d)->ASCTy == 077)
    {
      theProcessor.reset(new Mp4Processor(mr, d->bitRate, audioBuffer, frameBuffer));
    }
  }
  else if (d->TMId == ETMId::PacketModeData)
  {
    theProcessor.reset(new DataProcessor(mr, (PacketData *)d, dataBuffer));
  }
  else
  {
    theProcessor.reset(new FrameProcessor());
  }
}


void BackendDriver::addtoFrame(const std::vector<u8> & theData)
{
  theProcessor->add_to_frame(theData);
}

