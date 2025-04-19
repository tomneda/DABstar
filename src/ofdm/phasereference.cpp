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
#include  "radio.h"
#include  <vector>

/**
  *	\class PhaseReference
  *	Implements the correlation that is used to identify
  *	the "first" element (following the cyclic prefix) of
  *	the first non-null block of a frame
  *	The class inherits from the phaseTable.
  */

#define CORRELATION_LENGTH  48

PhaseReference::PhaseReference(const RadioInterface * const ipRadio, const ProcessParams * const ipParam)
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
  // We collect data of SEARCHRANGE/2 bins at the end of the FFT buffer and wrap to the begin and check SEARCHRANGE/2 elements further.
  // This is equal to check +/- SEARCHRANGE/2 bins (== (35) kHz in DabMode 1) around the DC
  mRefArg.resize(CORRELATION_LENGTH);
  for (int i = 0; i < CORRELATION_LENGTH; i++)
  {
    mRefArg[i] = arg(mRefTable[(cTu + i) % cTu] * conj(mRefTable[(cTu + i + 1) % cTu]));
  }
  mCorrelationVector.resize(SEARCHRANGE + CORRELATION_LENGTH);

  connect(this, &PhaseReference::signal_show_correlation, ipRadio, &RadioInterface::slot_show_correlation);
}

PhaseReference::~PhaseReference()
{
  fftwf_destroy_plan(mFftPlanBwd);
  fftwf_destroy_plan(mFftPlanFwd);
}

/**
  *	\brief findIndex
  *	the vector v contains "T_u" samples that are believed to
  *	belong to the first non-null block of a DAB frame.
  *	We correlate the data in this vector with the predefined
  *	data, and if the maximum exceeds a threshold value,
  *	we believe that that indicates the first sample we were
  *	looking for.
  */

int32_t PhaseReference::correlate_with_phase_ref_and_find_max_peak(const TArrayTn & iV, const float iThreshold)
{
  // memcpy() is considerable faster than std::copy on my i7-6700K (nearly twice as fast for size == 2048)
  memcpy(mFftInBuffer.data(), iV.data(), mFftInBuffer.size() * sizeof(cmplx));

  fftwf_execute(mFftPlanFwd);

  //	into the frequency domain, now correlate
  for (int32_t i = 0; i < cTu; i++)
  {
    mFftInBuffer[i] = mFftOutBuffer[i] * conj(mRefTable[i]);
  }

  //	and, again, back into the time domain
  fftwf_execute(mFftPlanBwd);

  /**
    *	We compute the average and the max signal values
    */
  float sum = 0;

  for (int32_t i = 0; i < cTu; i++)
  {
    const float absVal = std::abs(mFftOutBuffer[i]);
    mCorrPeakValues[i] = absVal;
    mMeanCorrPeakValues[i] += absVal;
    sum += absVal;
  }

  sum /= (float)(cTu);
  QVector<int> indices;
  int32_t maxIndex = -1;
  float maxL = -1000;
  constexpr int16_t GAP_SEARCH_WIDTH = 10;
  constexpr int16_t EXTENDED_SEARCH_REGION = 250;
  const int16_t idxStart = cTg - EXTENDED_SEARCH_REGION;
  const int16_t idxStop  = cTg + EXTENDED_SEARCH_REGION;
  assert(idxStart >= 0);
  assert(idxStop <= (signed)mCorrPeakValues.size());

  for (int16_t i = idxStart; i < idxStop; ++i)
  {
    if (mCorrPeakValues[i] / sum > iThreshold)
    {
      bool foundOne = true;

      for (int16_t j = 1; (j < GAP_SEARCH_WIDTH) && (i + j < idxStop); j++)
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
    //return (int32_t)(-abs(maxL / sum) - 1);
  }

  // display correlation spectrum
  if (mpResponse != nullptr)
  {
    ++mDisplayCounter;

    if (mDisplayCounter > mFramesPerSecond / 2)
    {
      for (int32_t i = 0; i < cTu; i++)
         mMeanCorrPeakValues[i] /= 6;
      mpResponse->put_data_into_ring_buffer(mMeanCorrPeakValues.data(), (int32_t)mMeanCorrPeakValues.size());
      //mpResponse->put_data_into_ring_buffer(mCorrPeakValues.data(), (int32_t)mCorrPeakValues.size());
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

//	an approach that works fine is to correlate the phase differences between subsequent carriers
int16_t PhaseReference::estimate_carrier_offset_from_sync_symbol_0(const TArrayTu & iV)
{
  //  The phase differences are computed once
  for (int16_t i = 0; i < SEARCHRANGE + CORRELATION_LENGTH; ++i)
  {
    const int16_t baseIndex = cTu - SEARCHRANGE / 2 + i;
    mCorrelationVector[i] = arg(iV[baseIndex % cTu] * conj(iV[(baseIndex + 1) % cTu]));
  }

  int16_t index = IDX_NOT_FOUND;
  float MMax = 0;

  for (int16_t i = 0; i < SEARCHRANGE; i++)
  {
    float sum = 0;
    for (int16_t j = 0; j < CORRELATION_LENGTH; ++j)
    {
      sum += abs(mRefArg[j] * mCorrelationVector[i + j]);
      if (sum > MMax)
      {
        MMax = sum;
        index = i;
      }
    }
  }

  //  Now map the index back to the right carrier
  return index - SEARCHRANGE / 2;
}

/*static*/
float PhaseReference::phase(const std::vector<cmplx> & iV, const int32_t iTs)
{
  cmplx sum = cmplx(0, 0);

  for (int i = 0; i < iTs; i++)
  {
    sum += iV[i];
  }

  return arg(sum);
}

void PhaseReference::set_sync_on_strongest_peak(bool sync)
{
  mSyncOnStrongestPeak = sync;
}
