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
RingBufferFactory<u8>::RingBufferFactory()
{
  create_ringbuffer(EId::FrameBuffer, "FrameBuffer",  8192);
  create_ringbuffer(EId::DataBuffer,  "DataBuffer",  32768);
}

template <>
RingBufferFactory<i16>::RingBufferFactory()
{
  create_ringbuffer(EId::AudioFromDecoder, "AudioFromDecoder", 4096 * 2 /*stereo*/ * 2 /*security*/);
  create_ringbuffer(EId::AudioToOutput,    "AudioToOutput",    SAudioFifo::cAudioFifoSizeSamplesBothChannels, true);
  create_ringbuffer(EId::TechDataBuffer,   "TechDataBuffer",   2 * 1024);
}

template <>
RingBufferFactory<ci16>::RingBufferFactory()
{
  create_ringbuffer(EId::DeviceSampleBuffer,  "DeviceSampleBuffer", 4 * 1024 * 1024);
}

template <>
RingBufferFactory<f32>::RingBufferFactory()
{
  create_ringbuffer(EId::CarrBuffer,     "CarrBuffer",     2 * 1536);
  create_ringbuffer(EId::ResponseBuffer, "ResponseBuffer", 2 * 2048 /*32768*/);
}

template <>
RingBufferFactory<cf32>::RingBufferFactory()
{
  create_ringbuffer(EId::SpectrumBuffer, "SpectrumBuffer",     2048);
  create_ringbuffer(EId::IqBuffer,       "IqBuffer",       2 * 1536);
  create_ringbuffer(EId::CirBuffer,      "CirBuffer",     97 * 2048);
}

const char * RingBufferFactoryBase::_show_progress_bar(f32 iPercentStop, f32 iPercentStart /*= -100*/) const
{
  char * p = mProgressBarBuffer.data();
  *p++ = '[';
  const i32 posStart = (i32)((f32)cBarWidth * iPercentStart / 100.0f); // < 0 -> no startmarker should be shown
  const i32 posStop  = (i32)((f32)cBarWidth * iPercentStop  / 100.0f);
  for (i32 i = 0; i < cBarWidth; ++i)
  {
    if      (i == posStart || i == posStop) *p++ = '|';
    else if (i >  posStart && i <  posStop) *p++ = '=';
    else                                    *p++ = '-';
  }
  *p++ = ']';
  return mProgressBarBuffer.data();
}

void RingBufferFactoryBase::_print_line(bool iResetMinMax) const
{
  for (i32 i = 0; i < 170; ++i) printf(iResetMinMax ? "=" : "-");
  printf("\n");
}

void RingBufferFactoryBase::_calculate_ring_buffer_statistics(f32 & oMinPrc, f32 & oMaxPrc, f32 & ioGlobMinPrc, f32 & ioGlobMaxPrc, SListPar & ioPar,
                                                              const bool iResetMinMax, const RingbufferBase::SFillState & iFillState) const
{
  if (iResetMinMax)
  {
    ioPar.MinVal = iFillState.Total;
    ioPar.MaxVal = 0;
  }

  if (ioPar.MinVal > iFillState.Filled) ioPar.MinVal = iFillState.Filled;
  if (ioPar.MaxVal < iFillState.Filled) ioPar.MaxVal = iFillState.Filled;
  oMinPrc = 100.0f * (f32)ioPar.MinVal / (f32)iFillState.Total;
  oMaxPrc = 100.0f * (f32)ioPar.MaxVal / (f32)iFillState.Total;
  if (ioGlobMinPrc > oMinPrc) ioGlobMinPrc = oMinPrc;
  if (ioGlobMaxPrc < oMaxPrc) ioGlobMaxPrc = oMaxPrc;
}

template <>
void RingBufferFactory<u8>::print_status(const bool iResetMinMax /*= false*/) const
{
  u32 showDataCnt = 0;
  f32 globMinPrc = std::numeric_limits<f32>::max();
  f32 globMaxPrc = 0.0f;

  for (const auto & [_, list] : mMap)
  {
    SListPar & par = list.Par;
    if (!par.ShowStatistics) continue;
    if (showDataCnt++ == 0) _print_line(iResetMinMax);

    const RingbufferBase::SFillState fs = list.pRingBuffer->get_fill_state();

    f32 minPrc, maxPrc;
    _calculate_ring_buffer_statistics(minPrc, maxPrc, globMinPrc, globMaxPrc, par, iResetMinMax, fs);

    const char * const progBar = _show_progress_bar(fs.Percent);
    printf("RingbufferUInt8   Id: %2d, Name: %20s, FillLevel: %3.0f%% %s (%3.0f%%, %3.0f%% | %7u, %7u, %7u)\n",
      (i32)list.Id, list.pName, fs.Percent, progBar, minPrc, maxPrc, fs.Filled, fs.Free, fs.Free + fs.Filled);
  }
  if (showDataCnt > 0)
  {
    const char * const globProgBar = _show_progress_bar(globMaxPrc, globMinPrc);
    printf("RingbufferUInt8           Name: %20s, FillLevel:      %s (%3.0f%%, %3.0f%%)\n", "MIN/MAX", globProgBar, globMinPrc, globMaxPrc);
  }
}

template <>
void RingBufferFactory<i16>::print_status(const bool iResetMinMax /*= false*/) const
{
  u32 showDataCnt = 0;
  f32 globMinPrc = std::numeric_limits<f32>::max();
  f32 globMaxPrc = 0.0f;

  for (const auto & [_, list] : mMap)
  {
    SListPar & par = list.Par;
    if (!par.ShowStatistics) continue;
    if (showDataCnt++ == 0) _print_line(iResetMinMax);

    const RingbufferBase::SFillState fs = list.pRingBuffer->get_fill_state();

    f32 minPrc, maxPrc;
    _calculate_ring_buffer_statistics(minPrc, maxPrc, globMinPrc, globMaxPrc, par, iResetMinMax, fs);

    const char * const progBar = _show_progress_bar(fs.Percent);
    printf("RingbufferInt16   Id: %2d, Name: %20s, FillLevel: %3.0f%% %s (%3.0f%%, %3.0f%% | %7u, %7u, %7u)\n",
      (i32)list.Id, list.pName, fs.Percent, progBar, minPrc, maxPrc, fs.Filled, fs.Free, fs.Free + fs.Filled);
  }
  if (showDataCnt > 0)
  {
    const char * const globProgBar = _show_progress_bar(globMaxPrc, globMinPrc);
    printf("RingbufferInt16           Name: %20s, FillLevel:      %s (%3.0f%%, %3.0f%%)\n", "MIN/MAX", globProgBar, globMinPrc, globMaxPrc);
  }
}

template <>
void RingBufferFactory<ci16>::print_status(const bool iResetMinMax /*= false*/) const
{
  u32 showDataCnt = 0;
  f32 globMinPrc = std::numeric_limits<f32>::max();
  f32 globMaxPrc = 0.0f;

  for (const auto & [_, list] : mMap)
  {
    SListPar & par = list.Par;
    if (!par.ShowStatistics) continue;
    if (showDataCnt++ == 0) _print_line(iResetMinMax);

    const RingbufferBase::SFillState fs = list.pRingBuffer->get_fill_state();

    f32 minPrc, maxPrc;
    _calculate_ring_buffer_statistics(minPrc, maxPrc, globMinPrc, globMaxPrc, par, iResetMinMax, fs);

    const char * const progBar = _show_progress_bar(fs.Percent);
    printf("RingbufferCmplx16 Id: %2d, Name: %20s, FillLevel: %3.0f%% %s (%3.0f%%, %3.0f%% | %7u, %7u, %7u)\n",
           (i32)list.Id, list.pName, fs.Percent, progBar, minPrc, maxPrc, fs.Filled, fs.Free, fs.Free + fs.Filled);
  }
  if (showDataCnt > 0)
  {
    const char * const globProgBar = _show_progress_bar(globMaxPrc, globMinPrc);
    printf("RingbufferCmplx16         Name: %20s, FillLevel:      %s (%3.0f%%, %3.0f%%)\n", "MIN/MAX", globProgBar, globMinPrc, globMaxPrc);
  }
}

template <>
void RingBufferFactory<f32>::print_status(const bool iResetMinMax /*= false*/) const
{
  u32 showDataCnt = 0;
  f32 globMinPrc = std::numeric_limits<f32>::max();
  f32 globMaxPrc = 0.0f;

  for (const auto & [_, list] : mMap)
  {
    SListPar & par = list.Par;
    if (!par.ShowStatistics) continue;
    if (showDataCnt++ == 0) _print_line(iResetMinMax);

    const RingbufferBase::SFillState fs = list.pRingBuffer->get_fill_state();

    f32 minPrc, maxPrc;
    _calculate_ring_buffer_statistics(minPrc, maxPrc, globMinPrc, globMaxPrc, par, iResetMinMax, fs);

    const char * const progBar = _show_progress_bar(fs.Percent);
    printf("RingbufferFloat   Id: %2d, Name: %20s, FillLevel: %3.0f%% %s (%3.0f%%, %3.0f%% | %7u, %7u, %7u)\n",
      (i32)list.Id, list.pName, fs.Percent, progBar, minPrc, maxPrc, fs.Filled, fs.Free, fs.Free + fs.Filled);
  }
  if (showDataCnt > 0)
  {
    const char * const globProgBar = _show_progress_bar(globMaxPrc, globMinPrc);
    printf("RingbufferFloat           Name: %20s, FillLevel:      %s (%3.0f%%, %3.0f%%)\n", "MIN/MAX", globProgBar, globMinPrc, globMaxPrc);
  }
}

template <>
void RingBufferFactory<cf32>::print_status(const bool iResetMinMax /*= false*/) const
{
  u32 showDataCnt = 0;
  f32 globMinPrc = std::numeric_limits<f32>::max();
  f32 globMaxPrc = 0.0f;

  for (const auto & [_, list] : mMap)
  {
    SListPar & par = list.Par;
    if (!par.ShowStatistics) continue;
    if (showDataCnt++ == 0) _print_line(iResetMinMax);

    const RingbufferBase::SFillState fs = list.pRingBuffer->get_fill_state();

    f32 minPrc, maxPrc;
    _calculate_ring_buffer_statistics(minPrc, maxPrc, globMinPrc, globMaxPrc, par, iResetMinMax, fs);

    const char * const progBar = _show_progress_bar(fs.Percent);
    printf("RingbufferCmplx   Id: %2d, Name: %20s, FillLevel: %3.0f%% %s (%3.0f%%, %3.0f%% | %7u, %7u, %7u)\n",
      (i32)list.Id, list.pName, fs.Percent, progBar, minPrc, maxPrc, fs.Filled, fs.Free, fs.Free + fs.Filled);
  }
  if (showDataCnt > 0)
  {
    const char * const globProgBar = _show_progress_bar(globMaxPrc, globMinPrc);
    printf("RingbufferCmplx           Name: %20s, FillLevel:      %s (%3.0f%%, %3.0f%%)\n", "MIN/MAX", globProgBar, globMinPrc, globMaxPrc);
  }
}


// instantiation
RingBufferFactory<u8>   sRingBufferFactoryUInt8;
RingBufferFactory<i16>  sRingBufferFactoryInt16;
RingBufferFactory<ci16> sRingBufferFactoryCmplx16;
RingBufferFactory<f32>  sRingBufferFactoryFloat;
RingBufferFactory<cf32> sRingBufferFactoryCmplx;
