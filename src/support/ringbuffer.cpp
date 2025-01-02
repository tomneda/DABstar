/*
 * Copyright (c) 2024 by Thomas Neder (https://github.com/tomneda)
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ringbuffer.h"
#include "audiofifo.h"

template <>
RingBufferFactory<uint8_t>::RingBufferFactory()
{
  create_ringbuffer(EId::FrameBuffer, "FrameBuffer", 2 * 32768);
  create_ringbuffer(EId::DataBuffer,  "DataBuffer",      32768);
}

template <>
RingBufferFactory<int16_t>::RingBufferFactory()
{
  create_ringbuffer(EId::AudioFromDecoder, "AudioFromDecoder", 4096 * 2 /*stereo*/ * 2 /*security*/, true);
  create_ringbuffer(EId::AudioToOutput,    "AudioToOutput",    AUDIO_FIFO_SIZE_SAMPLES_BOTH_CHANNELS, true);
}

template <>
RingBufferFactory<float>::RingBufferFactory()
{
  create_ringbuffer(EId::CarrBuffer,     "CarrBuffer",     2 * 1536);
  create_ringbuffer(EId::ResponseBuffer, "ResponseBuffer", 2 * 2048 /*32768*/);
}

template <>
RingBufferFactory<cmplx>::RingBufferFactory()
{
  create_ringbuffer(EId::SpectrumBuffer, "SpectrumBuffer",     2048);
  create_ringbuffer(EId::IqBuffer,       "IqBuffer",       2 * 1536);
}

const char * RingBufferFactoryBase::_show_progress_bar(float iPercentStop, float iPercentStart /*= -100*/) const
{
  char * p = mProgressBarBuffer.data();
  *p++ = '[';
  const int32_t posStart = (int32_t)((float)cBarWidth * iPercentStart / 100.0f); // < 0 -> no startmarker should be shown
  const int32_t posStop  = (int32_t)((float)cBarWidth * iPercentStop  / 100.0f);
  for (int32_t i = 0; i < cBarWidth; ++i)
  {
    if      (i == posStart || i == posStop) *p++ = '|';
    else if (i >  posStart && i <  posStop) *p++ = '=';
    else                                    *p++ = '-';
  }
  *p++ = ']';
  return mProgressBarBuffer.data();
}

template <>
void RingBufferFactory<uint8_t>::print_status(bool iResetMinMax /*= false*/) const
{
  uint32_t showDataCnt = 0;
  float globMinPrc = std::numeric_limits<float>::max();
  float globMaxPrc = 0.0f;

  for (const auto & [_, list] : mMap)
  {
    if (!list.ShowStatistics) continue;
    if (showDataCnt++ == 0) { for (int32_t i = 0; i < 170; ++i) printf(iResetMinMax ? "=" : "-");  printf("\n"); }

    const RingBuffer<uint8_t>::SFillState fs = list.pRingBuffer->get_fill_state();

    if (iResetMinMax)
    {
      list.MinVal = fs.Total;
      list.MaxVal = 0;
    }

    if (list.MinVal > fs.Filled) list.MinVal = fs.Filled;
    if (list.MaxVal < fs.Filled) list.MaxVal = fs.Filled;
    const float minPrc = 100.0f * (float)list.MinVal / (float)fs.Total;
    const float maxPrc = 100.0f * (float)list.MaxVal / (float)fs.Total;
    if (globMinPrc > minPrc) globMinPrc = minPrc;
    if (globMaxPrc < maxPrc) globMaxPrc = maxPrc;
    const char * const progBar = _show_progress_bar(fs.Percent);
    printf("RingbufferUInt8 Id: %2d, Name: %20s, FillLevel: %3.0f%% %s (%3.0f%%, %3.0f%% | %7u, %7u, %7u)\n",
      (int)list.Id, list.pName, fs.Percent, progBar, minPrc, maxPrc, fs.Filled, fs.Free, fs.Free + fs.Filled);
  }
  if (showDataCnt > 0)
  {
    const char * const globProgBar = _show_progress_bar(globMaxPrc, globMinPrc);
    printf("RingbufferUInt8         Name: %20s, FillLevel:      %s (%3.0f%%, %3.0f%%)\n", "MIN/MAX", globProgBar, globMinPrc, globMaxPrc);
  }
}

template <>
void RingBufferFactory<int16_t>::print_status(bool iResetMinMax /*= false*/) const
{
  uint32_t showDataCnt = 0;
  float globMinPrc = std::numeric_limits<float>::max();
  float globMaxPrc = 0.0f;

  for (const auto & [_, list] : mMap)
  {
    if (!list.ShowStatistics) continue;
    if (showDataCnt++ == 0) { for (int32_t i = 0; i < 170; ++i) printf(iResetMinMax ? "=" : "-");  printf("\n"); }

    const RingBuffer<int16_t>::SFillState fs = list.pRingBuffer->get_fill_state();

    if (iResetMinMax)
    {
      list.MinVal = fs.Total;
      list.MaxVal = 0;
    }

    if (list.MinVal > fs.Filled) list.MinVal = fs.Filled;
    if (list.MaxVal < fs.Filled) list.MaxVal = fs.Filled;
    const float minPrc = 100.0f * (float)list.MinVal / (float)fs.Total;
    const float maxPrc = 100.0f * (float)list.MaxVal / (float)fs.Total;
    if (globMinPrc > minPrc) globMinPrc = minPrc;
    if (globMaxPrc < maxPrc) globMaxPrc = maxPrc;
    const char * const progBar = _show_progress_bar(fs.Percent);
    printf("RingbufferInt16 Id: %2d, Name: %20s, FillLevel: %3.0f%% %s (%3.0f%%, %3.0f%% | %7u, %7u, %7u)\n",
      (int)list.Id, list.pName, fs.Percent, progBar, minPrc, maxPrc, fs.Filled, fs.Free, fs.Free + fs.Filled);
  }
  if (showDataCnt > 0)
  {
    const char * const globProgBar = _show_progress_bar(globMaxPrc, globMinPrc);
    printf("RingbufferInt16         Name: %20s, FillLevel:      %s (%3.0f%%, %3.0f%%)\n", "MIN/MAX", globProgBar, globMinPrc, globMaxPrc);
  }
}

template <>
void RingBufferFactory<float>::print_status(bool iResetMinMax /*= false*/) const
{
  uint32_t showDataCnt = 0;
  float globMinPrc = std::numeric_limits<float>::max();
  float globMaxPrc = 0.0f;

  for (const auto & [_, list] : mMap)
  {
    if (!list.ShowStatistics) continue;
    if (showDataCnt++ == 0) { for (int32_t i = 0; i < 170; ++i) printf(iResetMinMax ? "=" : "-");  printf("\n"); }

    const RingBuffer<float>::SFillState fs = list.pRingBuffer->get_fill_state();

    if (iResetMinMax)
    {
      list.MinVal = fs.Total;
      list.MaxVal = 0;
    }

    if (list.MinVal > fs.Filled) list.MinVal = fs.Filled;
    if (list.MaxVal < fs.Filled) list.MaxVal = fs.Filled;
    const float minPrc = 100.0f * (float)list.MinVal / (float)fs.Total;
    const float maxPrc = 100.0f * (float)list.MaxVal / (float)fs.Total;
    if (globMinPrc > minPrc) globMinPrc = minPrc;
    if (globMaxPrc < maxPrc) globMaxPrc = maxPrc;
    const char * const progBar = _show_progress_bar(fs.Percent);
    printf("RingbufferFloat Id: %2d, Name: %20s, FillLevel: %3.0f%% %s (%3.0f%%, %3.0f%% | %7u, %7u, %7u)\n",
      (int)list.Id, list.pName, fs.Percent, progBar, minPrc, maxPrc, fs.Filled, fs.Free, fs.Free + fs.Filled);
  }
  if (showDataCnt > 0)
  {
    const char * const globProgBar = _show_progress_bar(globMaxPrc, globMinPrc);
    printf("RingbufferFloat         Name: %20s, FillLevel:      %s (%3.0f%%, %3.0f%%)\n", "MIN/MAX", globProgBar, globMinPrc, globMaxPrc);
  }
}

template <>
void RingBufferFactory<cmplx>::print_status(bool iResetMinMax /*= false*/) const
{
  uint32_t showDataCnt = 0;
  float globMinPrc = std::numeric_limits<float>::max();
  float globMaxPrc = 0.0f;

  for (const auto & [_, list] : mMap)
  {
    if (!list.ShowStatistics) continue;
    if (showDataCnt++ == 0) { for (int32_t i = 0; i < 170; ++i) printf(iResetMinMax ? "=" : "-");  printf("\n"); }

    const RingBuffer<cmplx>::SFillState fs = list.pRingBuffer->get_fill_state();

    if (iResetMinMax)
    {
      list.MinVal = fs.Total;
      list.MaxVal = 0;
    }

    if (list.MinVal > fs.Filled) list.MinVal = fs.Filled;
    if (list.MaxVal < fs.Filled) list.MaxVal = fs.Filled;
    const float minPrc = 100.0f * (float)list.MinVal / (float)fs.Total;
    const float maxPrc = 100.0f * (float)list.MaxVal / (float)fs.Total;
    if (globMinPrc > minPrc) globMinPrc = minPrc;
    if (globMaxPrc < maxPrc) globMaxPrc = maxPrc;
    const char * const progBar = _show_progress_bar(fs.Percent);
    printf("RingbufferCmplx Id: %2d, Name: %20s, FillLevel: %3.0f%% %s (%3.0f%%, %3.0f%% | %7u, %7u, %7u)\n",
      (int)list.Id, list.pName, fs.Percent, progBar, minPrc, maxPrc, fs.Filled, fs.Free, fs.Free + fs.Filled);
  }
  if (showDataCnt > 0)
  {
    const char * const globProgBar = _show_progress_bar(globMaxPrc, globMinPrc);
    printf("RingbufferCmplx         Name: %20s, FillLevel:      %s (%3.0f%%, %3.0f%%)\n", "MIN/MAX", globProgBar, globMinPrc, globMaxPrc);
  }
}


// instantiation concretion
RingBufferFactory<uint8_t> sRingBufferFactoryUInt8;
RingBufferFactory<int16_t> sRingBufferFactoryInt16;
RingBufferFactory<float>   sRingBufferFactoryFloat;
RingBufferFactory<cmplx>   sRingBufferFactoryCmplx;
