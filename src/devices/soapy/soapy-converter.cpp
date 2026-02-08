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

#include "ringbuffer.h"
#include "soapy-converter.h"
#include "dab-constants.h"

SoapyConverter::SoapyConverter(SoapySDR::Device * device, const int sampleRate)
  : theBuffer(16 * 32768)
{
  this->theDevice = device;
  std::vector<size_t> xxx;
  stream = device->setupStream(SOAPY_SDR_RX, "CF32", xxx, SoapySDR::Kwargs());
  this->sampleRate = sampleRate;
  if (sampleRate != INPUT_RATE)
  {
#ifdef HAVE_LIQUID
    const float resampRatio = (float)INPUT_RATE / sampleRate;
    constexpr u32 filterLen = 13;
    constexpr float resampBw = 0.5f * (float)(cK * cCarrDiff + 2*35000) / (float)INPUT_RATE;
    constexpr float sideLopeSuppr = 40.0f;
    constexpr u32 numFiltersInBank = 32;
    mLiquidResampler = resamp_crcf_create(resampRatio, filterLen, resampBw, sideLopeSuppr, numFiltersInBank);
    resamp_crcf_print(mLiquidResampler);
    mResampBuffer.resize(2048 + 10); // add some exaggerated samples
#else
    // we process chunks of 1 msec
    mMapTable_int.resize(2048);
    mMapTable_float.resize(2048);

    for (i32 i = 0; i < 2048; i++)
    {
      const f32 inVal = (f32)sampleRate / 1000.0f;
      mMapTable_int[i] = (i16)std::floor((f32)i * (inVal / 2048.0f));
      mMapTable_float[i] = (f32)i * (inVal / 2048.0f) - (f32)mMapTable_int[i];
    }
    mConvIndex = 0;
    mResampBuffer.resize(2048);
#endif
    mConvBufferSize = sampleRate / 1000;
    mConvBuffer.resize(std::max(mConvBufferSize + 1, 4096));
  }
  start();
}

SoapyConverter::~SoapyConverter(void)
{
  running = false;
  while (isRunning())
  {
    usleep(1000);
  }
  theDevice->deactivateStream(stream);
#ifdef HAVE_LIQUID
  if (sampleRate != INPUT_RATE)
  {
    resamp_crcf_destroy(mLiquidResampler);
  }
#endif
}

i32 SoapyConverter::Samples(void)
{
  return theBuffer.get_ring_buffer_read_available();
}

i32 SoapyConverter::getSamples(cf32 * v, i32 amount)
{
  i32 realAmount;
  realAmount = theBuffer.get_data_from_ring_buffer(v, amount);
  return realAmount;
}

void SoapyConverter::run(void)
{
  i32 flag = 0;
  long long timeNS;
  cf32 buffer[4096];
  void * const buffs[] = {buffer};
  running = true;

  theDevice->activateStream(stream);
  while (running)
  {
    i32 numSamples = theDevice->readStream(stream, buffs, 4096, flag, timeNS, 10000);
    if (numSamples > 0)
    {
      if (sampleRate == INPUT_RATE)
      {
        theBuffer.put_data_into_ring_buffer(buffer, numSamples);
      }
      else
      {
        for (i32 i = 0; i < numSamples; i++)
        {
          mConvBuffer[mConvIndex++] = buffer[i];
          if (mConvIndex > mConvBufferSize)
          {
#ifdef HAVE_LIQUID
            u32 usedResultSamples = 0;
            resamp_crcf_execute_block(mLiquidResampler, mConvBuffer.data(), mConvBufferSize, mResampBuffer.data(), &usedResultSamples);
            assert(usedResultSamples <= mResampBuffer.size());
            theBuffer.put_data_into_ring_buffer(mResampBuffer.data(), (i32)usedResultSamples);
#else
            for (i32 j = 0; j < 2048; j++)
            {
              const i16 inpBase = mMapTable_int[j];
              const f32 inpRatio = mMapTable_float[j];
              mResampBuffer[j] = mConvBuffer[inpBase + 1] * inpRatio + mConvBuffer[inpBase] * (1 - inpRatio);
            }
            theBuffer.put_data_into_ring_buffer(mResampBuffer.data(), 2048);
#endif
            mConvBuffer[0] = mConvBuffer[mConvBufferSize];
            mConvIndex = 1;
          } //if (mConvIndex > mConvBufferSize)
        } // for (i32 i = 0; i < numSamples; i++)
      } // else
    } // if (nSamples > 0)
  } // while (running)
}
