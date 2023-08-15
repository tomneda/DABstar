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

PhaseReference::PhaseReference(const RadioInterface * const ipRadio, const processParams * const ipParam)
  : PhaseTable(ipParam->dabMode),
    mDabPar(DabParams(ipParam->dabMode).get_dab_par()),
    mFramesPerSecond(2048000 / mDabPar.T_F),
    mFftForward(mDabPar.T_u, false),
    mFftBackwards(mDabPar.T_u, true),
    mpResponse(ipParam->responseBuffer),
    mRefTable(mDabPar.T_u, { 0, 0 }),
    mCorrPeakValues(mDabPar.T_u / 2)
{
  for (int32_t i = 1; i <= mDabPar.K / 2; i++)
  {
    float Phi_k = get_Phi(i);
    mRefTable[i] = cmplx(cos(Phi_k), sin(Phi_k));
    Phi_k = get_Phi(-i);
    mRefTable[mDabPar.T_u - i] = cmplx(cos(Phi_k), sin(Phi_k));
  }

  // prepare a table for the coarse frequency synchronization
  // can be a static one, actually, we are only interested in
  // the ones with a null
  for (int32_t i = 1; i <= DIFFLENGTH; i++)
  {
    mPhaseDifferences[i - 1] = abs(arg(mRefTable[(mDabPar.T_u + i + 0) % mDabPar.T_u] * conj(mRefTable[(mDabPar.T_u + i + 1) % mDabPar.T_u])));
  }

  connect(this, &PhaseReference::show_correlation, ipRadio, &RadioInterface::showCorrelation);
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

int32_t PhaseReference::find_index(std::vector<cmplx> iV, float iThreshold) // copy of iV is intended
{
  mFftForward.fft(iV); // in-place FFT

  //	into the frequency domain, now correlate
  for (int32_t i = 0; i < mDabPar.T_u; i++)
  {
    iV[i] *= conj(mRefTable[i]);
  }

  //	and, again, back into the time domain
  mFftBackwards.fft(iV); // in-place IFFT

  /**
    *	We compute the average and the max signal values
    */
  float sum = 0;

  for (int32_t i = 0; i < mDabPar.T_u / 2; i++)
  {
    mCorrPeakValues[i] = jan_abs(iV[i]);
    sum += mCorrPeakValues[i];
  }

  sum /= (float)(mDabPar.T_u) / 2;
  QVector<int> indices;
  int32_t maxIndex = -1;
  float maxL = -1000;
  constexpr int16_t GAP_SEARCH_WIDTH = 10;
  constexpr int16_t EXTENDED_SEARCH_REGION = 250;
  const int16_t idxStart = mDabPar.T_g - EXTENDED_SEARCH_REGION;
  const int16_t idxStop  = mDabPar.T_g + EXTENDED_SEARCH_REGION;
  assert(idxStart >= 0);
  assert(idxStop <= (signed)mCorrPeakValues.size());

  for (int16_t i = idxStart; i < idxStop; i++)  // TODO: underflow with other DabModes != 1!
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
    return (int32_t)(-abs(maxL / sum) - 1);
  }

  if (mpResponse != nullptr)
  {
    ++mDisplayCounter;

    if (mDisplayCounter > mFramesPerSecond / 2)
    {
      mpResponse->putDataIntoBuffer(mCorrPeakValues.data(), (int32_t)mCorrPeakValues.size());
      emit show_correlation((int32_t)mCorrPeakValues.size(), mDabPar.T_g, indices);
      mDisplayCounter = 0;
    }
  }

  assert(maxIndex >= 0);
  return maxIndex;
}

//	an approach that works fine is to correlate the phasedifferences between subsequent carriers
int16_t PhaseReference::estimate_carrier_offset(std::vector<cmplx> iV) // copy of iV is intended
{
  mFftForward.fft(iV);

  const int16_t idxStart = mDabPar.T_u - SEARCHRANGE / 2;
  const int16_t idxStop  = mDabPar.T_u + SEARCHRANGE / 2;

  for (int16_t i = idxStart; i < idxStop + DIFFLENGTH; i++)
  {
    mComputedDiffs[i - idxStart] = abs(arg(iV[(i + 0) % mDabPar.T_u] * conj(iV[(i + 1) % mDabPar.T_u])));
  }

  float mmin = 1000;
  float mmax = 0;
  int16_t idxMin = IDX_NOT_FOUND;
  int16_t idxMax = IDX_NOT_FOUND;

  for (int16_t i = idxStart; i < idxStop; i++)
  {
    float sum = 0;
    float sum2 = 0;

    for (int16_t j = 1; j < DIFFLENGTH; j++)
    {
      if (mPhaseDifferences[j - 1] < 0.1)
      {
        sum += mComputedDiffs[i - idxStart + j];
      }
      if (mPhaseDifferences[j - 1] > M_PI - 0.1)
      {
        sum2 += mComputedDiffs[i - idxStart + j];
      }
    }

    if (sum < mmin)
    {
      mmin = sum;
      idxMin = i;
    }

    if (sum2 > mmax)
    {
      mmax = sum2;
      idxMax = i;
    }
  }

  if (idxMin != idxMax)
  {
    return IDX_NOT_FOUND;
  }

  return idxMin - mDabPar.T_u;
}

/*static*/ float PhaseReference::phase(const std::vector<cmplx> & iV, const int32_t iTs)
{
  cmplx sum = cmplx(0, 0);

  for (int i = 0; i < iTs; i++)
  {
    sum += iV[i];
  }

  return arg(sum);
}
