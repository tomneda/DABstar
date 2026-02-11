#
/*
 *    Copyright (C) 2014 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation recorder 2 of the License.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#pragma once

#include <SoapySDR/Device.hpp>
#include "soapy-worker.h"
#ifdef HAVE_LIQUID
  #include <liquid/liquid.h>
#endif

class SoapyConverter: public soapyWorker
{
public:
  SoapyConverter (SoapySDR::Device *, SoapySDR::Stream *stream, const int sampleRate);
  ~SoapyConverter(void);
  i32 Samples    (void);
  i32 getSamples (cf32 *, i32);
  void run       (void);
private:
  SoapySDR::Device *theDevice;
  SoapySDR::Stream *stream;
  RingBuffer<cf32> theBuffer;
  bool running;
  int sampleRate;

  // for the conversion - if any
  std::vector<cf32> mConvBuffer;
  i16 mConvIndex = 0;
  i16 mConvBufferSize = 0;
  std::vector<cf32> mResampBuffer;
#ifdef HAVE_LIQUID
  resamp_crcf mLiquidResampler = nullptr;
#else
  std::vector<i16> mMapTable_int;
  std::vector<f32> mMapTable_float;
#endif
};
