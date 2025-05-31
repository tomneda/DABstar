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

  for (i32 i = 0; i < INPUT_RATE; i++)
  {
    oscillatorTable[i] = cf32((f32)cos(2.0 * M_PI * i / INPUT_RATE),
                              (f32)sin(2.0 * M_PI * i / INPUT_RATE));
  }

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
    usleep(10);
  }

  if (!running.load()) throw 20; // stops the DAB processor

  iNoSamples = theRig->getSamples(buffer, iNoSamples);

  if (dumpfilePointer.load() != nullptr)
  {
    _dump_samples_to_file(buffer, iNoSamples);
  }

  //	OK, we have samples!!
  //	First: adjust frequency. We need Hz accuracy
  for (i32 i = 0; i < iNoSamples; i++)
  {
    currentPhase -= iFreqOffsetBBHz;

    //	Note that "phase" itself might be negative
    currentPhase = (currentPhase + INPUT_RATE) % INPUT_RATE;
    assert(currentPhase >= 0);  // could happen with currentPhase < -INPUT_RATE

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

    // we mix after the IQ/DC compensation as these effects are only related to the ADC properties
    buffer[i] = v * oscillatorTable[currentPhase];
  }

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

