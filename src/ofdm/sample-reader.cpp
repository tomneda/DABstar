/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013 .. 2017
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
#include  "sample-reader.h"
#include  "dabradio.h"
#include  <ctime>

static inline i16 value_for_bit_pos(i16 b)
{
  assert(b > 0);
  i16 res = 1;
  while (--b > 0)
  {
    res <<= 1;
  }
  return res;
}

SampleReader::SampleReader(const DabRadio * mr, IDeviceHandler * iTheRig, RingBuffer<cf32> * iSpectrumBuffer)
  : myRadioInterface(mr)
  , theRig(iTheRig)
  , spectrumBuffer(iSpectrumBuffer)
{
  dumpfilePointer.store(nullptr);
  dumpScale = value_for_bit_pos(theRig->bitDepth());
  running.store(true);

#ifndef HAVE_SSE_OR_AVX
  for (i32 i = 0; i < INPUT_RATE; i++)
  {
    oscillatorTable[i] = cf32((f32)cos(2.0 * M_PI * i / INPUT_RATE),
                              (f32)sin(2.0 * M_PI * i / INPUT_RATE));
  }
#endif

  connect(this, &SampleReader::signal_show_spectrum, mr, &DabRadio::slot_show_spectrum);
  connect(this, &SampleReader::signal_show_cir     , mr, &DabRadio::slot_show_cir);
}

void SampleReader::setRunning(bool b)
{
  running.store(b);

  // with (looped) file input the while loop will hang, so skip this, deleting the buffer does not make sense here either
  if (!theRig->isFileInput())
  {
    theRig->resetBuffer();

    // in the case resetBuffer() is not implemented, read out the remaining buffer
    i32 readSampleSize = 0;
    do
    {
      readSampleSize = theRig->getSamples(mSampleBuffer.data(), (i32)mSampleBuffer.size());
    }
    while(readSampleSize >= (i32)mSampleBuffer.size()); // repeat if full buffer could be read-in as there could be more
  }
}

f32 SampleReader::get_sLevel() const
{
  return sLevel;
}

cf32 SampleReader::getSample(i32 phaseOffset)
{
  getSamples(mSampleBuffer, 0, 1, phaseOffset, true); // show spectrum while scanning
  return mSampleBuffer[0];
}

void SampleReader::getSamples(TArrayTn & oV, const i32 iStartIdx, i32 iNoSamples, const i32 iFreqOffsetBBHz, bool iShowSpec)
{
  assert((signed)oV.size() >= iStartIdx + iNoSamples);

  cf32 * const buffer = oV.data() + iStartIdx;

  while (running.load() && theRig->Samples() < iNoSamples)
  {
    usleep(1000);
  }

  if (!running.load()) throw 20; // stops the DAB processor

  iNoSamples = theRig->getSamples(buffer, iNoSamples);

  if (dumpfilePointer.load() != nullptr)
  {
    _dump_samples_to_file(buffer, iNoSamples);
  }

  // OK, we have samples!!
#ifdef HAVE_SSE_OR_AVX
  f32 SingleFloat;
  u32 index;

  // Perform DC removal if wanted
  if (dcRemovalActive)
  {
    constexpr f32 ALPHA = 1.0f / INPUT_RATE;
    volk_32fc_deinterleave_32f_x2_u(mVolkFloat1, mVolkFloat2, buffer, iNoSamples);
    volk_32f_accumulator_s32f_a(&SingleFloat, mVolkFloat1, iNoSamples);
    mean_filter(meanI, SingleFloat / iNoSamples, ALPHA * iNoSamples);
    volk_32f_accumulator_s32f_a(&SingleFloat, mVolkFloat2, iNoSamples);
    mean_filter(meanQ, SingleFloat / iNoSamples, ALPHA * iNoSamples);
    volk_32f_s32f_add_32f_a(mVolkFloat1, mVolkFloat1, -meanI, iNoSamples); // v_i - meanI
    volk_32f_s32f_add_32f_a(mVolkFloat2, mVolkFloat2, -meanQ, iNoSamples); // v_q - meanQ
#ifdef USE_IQ_COMPENSATION
    volk_32f_x2_multiply_32f_a(mVolkFloat3, mVolkFloat1, mVolkFloat1, iNoSamples); // x_i * x_i
    volk_32f_accumulator_s32f_a(&SingleFloat, mVolkFloat3, iNoSamples);
    mean_filter(meanII, SingleFloat / iNoSamples, ALPHA * iNoSamples);
    volk_32f_x2_multiply_32f_a(mVolkFloat3, mVolkFloat1, mVolkFloat2, iNoSamples); // x_i * x_q
    volk_32f_accumulator_s32f_a(&SingleFloat, mVolkFloat3, iNoSamples);
    mean_filter(meanIQ, SingleFloat / iNoSamples, ALPHA * iNoSamples);
    const f32 phi = meanIQ / meanII;
    volk_32f_s32f_multiply_32f_a(mVolkFloat3, mVolkFloat1, -phi, iNoSamples); // -phi * x_i
    volk_32f_x2_add_32f_a(mVolkFloat3, mVolkFloat3, mVolkFloat2, iNoSamples); // x_q_corr
    volk_32f_x2_multiply_32f_a(mVolkFloat4, mVolkFloat3, mVolkFloat3, iNoSamples); // x_q_corr * x_q_corr
    volk_32f_accumulator_s32f_a(&SingleFloat, mVolkFloat4, iNoSamples);
    mean_filter(meanQQ, SingleFloat / iNoSamples, ALPHA * iNoSamples);
    const f32 gainQ = std::sqrt(meanII / meanQQ);
    volk_32f_s32f_multiply_32f_a(mVolkFloat2, mVolkFloat3, gainQ, iNoSamples); // x_q_corr * gainQ
#endif
    volk_32f_x2_interleave_32fc_u(buffer, mVolkFloat1, mVolkFloat2, iNoSamples);
  }
  // The mixing has no effect on the absolute level detection, so do it beforehand
  volk_32fc_magnitude_32f_u(mVolkFloat1, buffer, iNoSamples); // v_abs = std::abs(v);
  volk_32f_index_max_32u_a(&index, mVolkFloat1, iNoSamples); // index of peak
  if (mVolkFloat1[index] > peakLevel) peakLevel = mVolkFloat1[index]; // if (v_abs > peakLevel) peakLevel = v_abs;
  volk_32f_accumulator_s32f_a(&SingleFloat, mVolkFloat1, iNoSamples);
  mean_filter(sLevel, SingleFloat / iNoSamples, 0.00001f * iNoSamples);


  // use the non-frequency corrected sample data for CIR analyzer
  if (cirBuffer != nullptr)
  {
    for (i32 i = 0; i < iNoSamples; i++)
    {
      if(mWholeFrameIndex < CIR_BUFF_SIZE)
      {
        mWholeFrameBuff[mWholeFrameIndex++] = buffer[i];
      }
      mWholeFrameCount += 1;
      if ((mWholeFrameCount >= (2048*96*6)) && (mWholeFrameIndex >= CIR_BUFF_SIZE)) // 6 frames
      {
        cirBuffer->put_data_into_ring_buffer(mWholeFrameBuff, CIR_BUFF_SIZE);
        emit signal_show_cir(CIR_BUFF_SIZE);
        mWholeFrameIndex = 0;
        mWholeFrameCount = 0;
      }
    }
  }
  // use the non-frequency corrected sample data for the spectrum as it could jump widely with +/-35 kHz with weak signals
  if (specBuffIdx < SPEC_BUFF_SIZE)
  {
    i32 count = SPEC_BUFF_SIZE - specBuffIdx;
    if (count > iNoSamples) count = iNoSamples;
    memcpy(&specBuff[specBuffIdx], buffer, count * sizeof(cf32));
    specBuffIdx += count;
  }

  // adjust frequency. We need Hz accuracy
  const cf32 phase_inc = cmplx_from_phase(F_2_M_PI * -iFreqOffsetBBHz / INPUT_RATE);
  volk_32fc_s32fc_x2_rotator2_32fc_u(buffer, buffer, &phase_inc, &phase, iNoSamples);

#else
  for (i32 i = 0; i < iNoSamples; i++)
  {
    cf32 v = buffer[i];
    // Perform DC removal if wanted
    if (dcRemovalActive)
    {
      const f32 v_i = real(v);
      const f32 v_q = imag(v);
      constexpr f32 ALPHA = 1.0f / INPUT_RATE / 1.00f /*s*/;
      mean_filter(meanI, v_i, ALPHA);
      mean_filter(meanQ, v_q, ALPHA);
#ifdef USE_IQ_COMPENSATION
      const f32 x_i = v_i - meanI;
      const f32 x_q = v_q - meanQ;
      mean_filter(meanII, x_i * x_i, ALPHA);
      mean_filter(meanIQ, x_i * x_q, ALPHA);
      const f32 phi = meanIQ / meanII;
      const f32 x_q_corr = x_q - phi * x_i;
      mean_filter(meanQQ, x_q_corr * x_q_corr, ALPHA);
      const f32 gainQ = std::sqrt(meanII / meanQQ);
      v = cf32(x_i, x_q_corr * gainQ);
#else
      v = cf32(v_i - meanI, v_q - meanQ);
#endif
    }

    // The mixing has no effect on the absolute level detection, so do it beforehand
    const f32 v_abs = std::abs(v);
    if (v_abs > peakLevel) peakLevel = v_abs;
    mean_filter(sLevel, v_abs, 0.00001f);

    // use the non-frequency corrected sample data for CIR analyzer
    if (cirBuffer != nullptr)
    {
      if(mWholeFrameIndex < CIR_BUFF_SIZE)
      {
        mWholeFrameBuff[mWholeFrameIndex++] = v;
      }
      mWholeFrameCount += 1;
      if ((mWholeFrameCount >= (2048*96*6)) && (mWholeFrameIndex >= CIR_BUFF_SIZE)) // 6 frames
      {
        cirBuffer->put_data_into_ring_buffer(mWholeFrameBuff, CIR_BUFF_SIZE);
        emit signal_show_cir(CIR_BUFF_SIZE);
        mWholeFrameIndex = 0;
        mWholeFrameCount = 0;
      }
    }

    // use the non-frequency corrected sample data for the spectrum as it could jump widely with +/-35 kHz with weak signals
    if (specBuffIdx < SPEC_BUFF_SIZE)
    {
      specBuff[specBuffIdx] = v;
      ++specBuffIdx;
    }

    // adjust frequency. We need Hz accuracy
    // Note that "phase" itself might be negative
    currentPhase -= iFreqOffsetBBHz;
    currentPhase = (currentPhase + INPUT_RATE) % INPUT_RATE;
    assert(currentPhase >= 0);  // could happen with currentPhase < -INPUT_RATE

    // we mix after the IQ/DC compensation as these effects are only related to the ADC properties
    buffer[i] = v * oscillatorTable[currentPhase];
  }
#endif

  sampleCount += iNoSamples;

  if (sampleCount > INPUT_RATE / 4 && iShowSpec && specBuffIdx >= SPEC_BUFF_SIZE)
  {
    if (spectrumBuffer != nullptr)
    {
      spectrumBuffer->put_data_into_ring_buffer(specBuff.data(), SPEC_BUFF_SIZE);
      emit signal_show_spectrum(SPEC_BUFF_SIZE);
    }
    specBuffIdx = 0;
    sampleCount = 0;
  }
}

void SampleReader::_dump_samples_to_file(const cf32 * const ipV, const i32 iNoSamples)
{
  const f32 scaleFactor = (INT16_MAX >> 2) / sLevel; // scale to 14 bit mean dynamic range

  for (i32 i = 0; i < iNoSamples; i++)
  {
    dumpBuffer[2 * dumpIndex + 0] = (i16)(real(ipV[i]) * scaleFactor);
    dumpBuffer[2 * dumpIndex + 1] = (i16)(imag(ipV[i]) * scaleFactor);

    if (++dumpIndex >= DUMP_SIZE / 2)
    {
      sf_writef_short(dumpfilePointer.load(), dumpBuffer.data(), dumpIndex);
      dumpIndex = 0;
    }
  }
}

void SampleReader::startDumping(SNDFILE * f)
{
  dumpIndex = 0;
  dumpfilePointer.store(f);
}

void SampleReader::stop_dumping()
{
  dumpfilePointer.store(nullptr);
}

f32 SampleReader::get_linear_peak_level_and_clear()
{
  const f32 peakLevelTemp = peakLevel;
  peakLevel = -1.0e6;
  return peakLevelTemp;
}

void SampleReader::set_dc_removal(bool iRemoveDC)
{
  meanI = meanQ = 0.0f;
  dcRemovalActive = iRemoveDC;
}

void SampleReader::set_cir_buffer(RingBuffer<cf32> * iCirBuffer)
{
  cirBuffer = iCirBuffer;
}
