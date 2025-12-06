/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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
#include  "phasereference.h"
#include  <QVector>
#include  "dabradio.h"
#include  <vector>
#ifdef HAVE_SSE_OR_AVX
  #include <volk/volk.h>
#endif

/**
  *	\class PhaseReference
  *	Implements the correlation that is used to identify
  *	the "first" element (following the cyclic prefix) of
  *	the first non-null block of a frame
  *	The class inherits from the phaseTable.
  */

PhaseReference::PhaseReference(const DabRadio * const ipRadio, const ProcessParams * const ipParam)
  : PhaseTable()
  , mFramesPerSecond(INPUT_RATE / cTF)
  , mpResponse(ipParam->responseBuffer)
  , mCorrPeakValues(cTu)
{
  mFftPlanFwd = fftwf_plan_dft_1d(cTu, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_FORWARD, FFTW_ESTIMATE);
  mFftPlanBwd = fftwf_plan_dft_1d(cTu, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_BACKWARD, FFTW_ESTIMATE);

  mMeanCorrPeakValues.resize(cTu);
  std::fill(mMeanCorrPeakValues.begin(), mMeanCorrPeakValues.end(), 0.0f);

  // Prepare a table for the coarse frequency synchronization.
  cRefArg.resize(cTu);
  CalculateRelativePhase(mRefTable.data(), mFftInBuffer);
  fftwf_execute(mFftPlanBwd);
  for (i32 i = 0; i < cTu; i++)
  {
    cRefArg[i] = std::conj(mFftOutBuffer[i]);
  }

  connect(this, &PhaseReference::signal_show_correlation, ipRadio, &DabRadio::slot_show_correlation);
}

PhaseReference::~PhaseReference()
{
  fftwf_destroy_plan(mFftPlanBwd);
  fftwf_destroy_plan(mFftPlanFwd);
}

/**
  *	\brief findIndex
  *	the vector v contains "cTu" samples that are believed to
  *	belong to the first non-null block of a DAB frame.
  *	We correlate the data in this vector with the predefined
  *	data, and if the maximum exceeds a threshold value,
  *	we believe that that indicates the first sample we were
  *	looking for.
  */

i32 PhaseReference::correlate_with_phase_ref_and_find_max_peak(const TArrayTn & iV, const f32 iThreshold)
{
  // memcpy() is considerable faster than std::copy on my i7-6700K (nearly twice as fast for size == 2048)
  memcpy(mFftInBuffer.data(), iV.data(), mFftInBuffer.size() * sizeof(cf32));

  fftwf_execute(mFftPlanFwd);

  //	into the frequency domain, now correlate
#ifdef HAVE_SSE_OR_AVX
  volk_32fc_x2_multiply_conjugate_32fc_a(mFftInBuffer.data(), mFftOutBuffer.data(), mRefTable.data(), cTu);
#else
  for (i32 i = 0; i < cTu; i++)
  {
    mFftInBuffer[i] = mFftOutBuffer[i] * conj(mRefTable[i]);
  }
#endif

  //	and, again, back into the time domain
  fftwf_execute(mFftPlanBwd);

  /**
    *	We compute the average and the max signal values
    */
  f32 sum = 0;

  for (i32 i = 0; i < cTu; i++)
  {
    const f32 absVal = std::abs(mFftOutBuffer[i]);
    mCorrPeakValues[i] = absVal;
    mMeanCorrPeakValues[i] += absVal;
    sum += absVal;
  }

  sum /= (f32)(cTu);
  QVector<i32> indices;
  i32 maxIndex = -1;
  f32 maxL = -1000;
  constexpr i16 GAP_SEARCH_WIDTH = 10;
  constexpr i16 EXTENDED_SEARCH_REGION = 250;
  const i16 idxStart = cTg - EXTENDED_SEARCH_REGION;
  const i16 idxStop  = cTg + EXTENDED_SEARCH_REGION;
  assert(idxStart >= 0);
  assert(idxStop <= (signed)mCorrPeakValues.size());

  for (i16 i = idxStart; i < idxStop; ++i)
  {
    if (mCorrPeakValues[i] / sum > iThreshold)
    {
      bool foundOne = true;

      for (i16 j = 1; (j < GAP_SEARCH_WIDTH) && (i + j < idxStop); j++)
      {
        if (mCorrPeakValues[i + j] > mCorrPeakValues[i])
        {
          foundOne = false;
          break;
        }
      }
      if (foundOne)
      {
        indices.push_back(i);

        if (mCorrPeakValues[i] > maxL)
        {
          maxL = mCorrPeakValues[i];
          maxIndex = i;
        }
        i += GAP_SEARCH_WIDTH;
      }
    }
  }

  if (maxL / sum < iThreshold)
  {
    return -1; // no (relevant) peak found
    //return (i32)(-abs(maxL / sum) - 1);
  }

  // display correlation spectrum
  if (mpResponse != nullptr)
  {
    ++mDisplayCounter;

    if (mDisplayCounter > mFramesPerSecond / 2)
    {
      for (i32 i = 0; i < cTu; i++)
      {
        mMeanCorrPeakValues[i] /= 6;
      }
      mpResponse->put_data_into_ring_buffer(mMeanCorrPeakValues.data(), (i32)mMeanCorrPeakValues.size());
      //mpResponse->put_data_into_ring_buffer(mCorrPeakValues.data(), (i32)mCorrPeakValues.size());
      emit signal_show_correlation(sum * iThreshold, indices);
      mDisplayCounter = 0;
    }
  }

  /* Tomneda: original Qt-DAB returns maximum found peak "maxIndex", but
   * I receive a channel here with up to 5 different transmitters where the nearest (earliest) does not have the maximum power.
   * The maximum peak is either the second or forth transmitter. So the maximum peak search had also decided for a quite delayed signal.
   * This leads to a OFDM symbol interference which was clearly been seen in the IQ scope or Modulation quality.
   * So, the theory and also the experiment says that the first found peak (above the threshold) should be used as timing marker.
   */
  if(mSyncOnStrongestPeak)
  {
    assert(maxIndex >= 0);
    return maxIndex;
  }
  else
  {
    assert(indices.size() > 0);
    return indices[0];
  }
}

//  The "best" approach for computing the coarse frequency offset is to
//  look at the spectrum of symbol 0 and relate that with the spectrum as
//  it should be, i.e. the refTable.
//  However, since there might be a pretty large phase offset between the
//  incoming data and the reference table data, we correlate the phase
//  differences between the subsequent carriers rather than the values in
//  the segments themselves. It seems to work pretty well.
//  The phase differences are computed once
i32 PhaseReference::estimate_carrier_offset_from_sync_symbol_0(const TArrayTu & iV)
{
  i16 index = IDX_NOT_FOUND;
  f32 max = 0;
  f32 avg = 0;

  // Step 1: Get complex difference between consecutive bins
  CalculateRelativePhase(iV.data(), mFftInBuffer);

  // Step 2: Get IFFT so we can do correlation in frequency domain via product in time domain
  fftwf_execute(mFftPlanBwd);

  // Step 3: Conjugate product in time domain
  //         NOTE: cRefArg is already the conjugate
  for (i32 i = 0; i < cTu; i++)
  {
    mFftInBuffer[i] = mFftOutBuffer[i] * cRefArg[i];
  }

  // Step 4: Get FFT to get correlation in frequency domain
  fftwf_execute(mFftPlanFwd);

  // Step 5: Find the peak in our maximum coarse frequency error window
  //fprintf(stderr, "Sum = ");
  for (i32 i = -SEARCHRANGE/2; i <= SEARCHRANGE/2; i++)
  {
    const f32 value = std::abs(mFftOutBuffer[(cTu + i) % cTu]);
    if (value > max)
    {
      max = value;
      index = i;
    }
    avg += value;
    //fprintf(stderr, "%.0f ", value/10000000);
  }
  //fprintf(stderr, "\n");
  avg /= SEARCHRANGE;

  // Step 6: Return if peak is too small
  if(max < avg * 5)
    return IDX_NOT_FOUND;

  // Step 7: Determine the coarse frequency offset
  // We get the frequency offset in terms of FFT bins which we convert to normalised Hz
  // Interpolate peak between neighbouring fft bins based on magnitude for more accurate estimate
  f32 peak[3];
  f32 peak_sum = 0.0f;
  for(int i = 0; i<3; i++)
  {
    peak[i] = std::abs(mFftOutBuffer[(cTu + index + i - 1) % cTu]);
    peak_sum += peak[i];
  }
  const f32 offset = index + (peak[2] - peak[0]) / peak_sum;
  //fprintf(stderr, "max=%.0f, average=%.0f, offset=%f\n", max/10000000, avg/10000000, offset);
  return (int32_t)(offset * cCarrDiff);
}

/*static*/
f32 PhaseReference::phase(const std::vector<cf32> & iV, const i32 iTs)
{
  cf32 sum = cf32(0, 0);

  for (i32 i = 0; i < iTs; i++)
  {
    sum += iV[i];
  }

  return arg(sum);
}

void PhaseReference::set_sync_on_strongest_peak(bool sync)
{
  mSyncOnStrongestPeak = sync;
}

void PhaseReference::CalculateRelativePhase(const cf32 *fft_in, TArrayTu & arg_out)
{
  for (i32 i = 0; i < cTu-1; i++)
  {
    arg_out[i] = std::conj(fft_in[i]) * fft_in[i+1];
  }
  arg_out[cTu-1] = {0,0};
}
