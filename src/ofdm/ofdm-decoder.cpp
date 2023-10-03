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
 *    This file is part of Qt-DAB
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
 *
 *	Once the bits are "in", interpretation and manipulation
 *	should reconstruct the data blocks.
 *	Ofdm_decoder is called for Block_0 and the FIC blocks,
 *	its invocation results in 2 * Tu bits
 */
#include "ofdm-decoder.h"
#include "phasetable.h"
#include "radio.h"
#include <vector>

/**
 *	\brief OfdmDecoder
 *	The class OfdmDecoder is
 *	taking the data from the ofdmProcessor class in, and
 *	will extract the Tu samples, do an FFT and extract the
 *	carriers and map them on (soft) bits
 */
OfdmDecoder::OfdmDecoder(RadioInterface * ipMr, uint8_t iDabMode, RingBuffer<cmplx> * ipIqBuffer, RingBuffer<float> * ipCarrBuffer) :
  mpRadioInterface(ipMr),
  mDabPar(DabParams(iDabMode).get_dab_par()),
  mFreqInterleaver(iDabMode),
  mFftHandler(mDabPar.T_u, false),
  mpIqBuffer(ipIqBuffer),
  mpCarrBuffer(ipCarrBuffer)
{
  connect(this, &OfdmDecoder::signal_slot_show_iq, mpRadioInterface, &RadioInterface::slot_show_iq);
  connect(this, &OfdmDecoder::signal_show_mod_quality, mpRadioInterface, &RadioInterface::slot_show_mod_quality_data);

  mMeanNullLevelWithTII.resize(mDabPar.T_u);
  mMeanNullPowerWithoutTII.resize(mDabPar.T_u);
  mPhaseReference.resize(mDabPar.T_u);
  mFftBuffer.resize(mDabPar.T_u);
  mIqVector.resize(mDabPar.K);
  mCarrVector.resize(mDabPar.K);
  mStdDevSqPhaseVector.resize(mDabPar.K);
  mIntegAbsPhaseVector.resize(mDabPar.K);
  mMeanPhaseVector.resize(mDabPar.K);
  mMeanPowerVector.resize(mDabPar.K);

  reset();
}

void OfdmDecoder::reset()
{
  std::fill(mStdDevSqPhaseVector.begin(), mStdDevSqPhaseVector.end(), 0.0f);
  std::fill(mIntegAbsPhaseVector.begin(), mIntegAbsPhaseVector.end(), 0.0f);
  std::fill(mMeanPhaseVector.begin(), mMeanPhaseVector.end(), 0.0f);
  std::fill(mMeanPowerVector.begin(), mMeanPowerVector.end(), 0.0f);
  std::fill(mMeanNullLevelWithTII.begin(), mMeanNullLevelWithTII.end(), 0.0f);
  std::fill(mMeanNullPowerWithoutTII.begin(), mMeanNullPowerWithoutTII.end(), 0.0f);

  mMeanPowerOvrAll = 1.0f;
  mAvgAbsNullLevelWithTIIMin = 0.0f;
  mAvgAbsNullLevelWithTIIGain = 0.0f;
}

void OfdmDecoder::store_null_symbol_with_tii(const std::vector<cmplx> & iV) // with TII information
{
  if (mCarrierPlotType != ECarrierPlotType::NULL_TII)
  {
    return;
  }

  memcpy(mFftBuffer.data(), &(iV[mDabPar.T_g]), mDabPar.T_u * sizeof(cmplx));

  mFftHandler.fft(mFftBuffer);

  constexpr float ALPHA = 0.20f;
  float max = -1e38f;
  float min =  1e38f;

  for (int32_t idx = -mDabPar.K / 2; idx < mDabPar.K / 2; ++idx)
  {
    int32_t fftIdx = idx;

    if (fftIdx < 0)
    {
      fftIdx += mDabPar.T_u; // negative spectrum part
    }
    else
    {
      ++fftIdx; // jump over bin at DC (0 Hz)
    }

    const float level = std::abs(mFftBuffer[fftIdx]);
    float & meanNullLevelWithTIIRef = mMeanNullLevelWithTII[fftIdx];
    mean_filter(meanNullLevelWithTIIRef, level, ALPHA);

    if (meanNullLevelWithTIIRef < min) min = meanNullLevelWithTIIRef;
    if (meanNullLevelWithTIIRef > max) max = meanNullLevelWithTIIRef;
  }

  assert(max  > min);
  mAvgAbsNullLevelWithTIIMin = min;
  mAvgAbsNullLevelWithTIIGain = 100.0f / (max - min);
}

void OfdmDecoder::store_null_symbol_without_tii(const std::vector<cmplx> & iV) // with TII information
{
  memcpy(mFftBuffer.data(), &(iV[mDabPar.T_g]), mDabPar.T_u * sizeof(cmplx));

  mFftHandler.fft(mFftBuffer);

  constexpr float ALPHA = 0.1f;

  for (int32_t idx = 0; idx < mDabPar.T_u; ++idx)
  {
    const float level = std::abs(mFftBuffer[idx]);
    mean_filter(mMeanNullPowerWithoutTII[idx], level * level, ALPHA);
  }
}

/**
 */
void OfdmDecoder::store_reference_symbol_0(std::vector<cmplx> buffer) // copy is intended as used as fft buffer
{
  mFftHandler.fft(buffer);
  /**
   *	we are now in the frequency domain, and we keep the carriers
   *	as coming from the FFT as phase reference.
   */

  memcpy(mPhaseReference.data(), buffer.data(), mDabPar.T_u * sizeof(cmplx));
}

/**
 *	for the other blocks of data, the first step is to go from
 *	time to frequency domain, to get the carriers.
 *	we distinguish between FIC blocks and other blocks,
 *	only to spare a test. The mapping code is the same
 */

void OfdmDecoder::decode_symbol(const std::vector<cmplx> & iV, uint16_t iCurOfdmSymbIdx, float iPhaseCorr, std::vector<int16_t> & oBits)
{
  memcpy(mFftBuffer.data(), &(iV[mDabPar.T_g]), mDabPar.T_u * sizeof(cmplx));

  mFftHandler.fft(mFftBuffer);

  //const cmplx rotator = std::exp(cmplx(0.0f, -iPhaseCorr)); // fine correction of phase which can't be done in the time domain

  ++mShowCntIqScope;
  ++mShowCntStatistics;
  const bool showScopeData = (mShowCntIqScope > mDabPar.L && iCurOfdmSymbIdx == mNextShownOfdmSymbIdx);
  const bool showStatisticData = (mShowCntStatistics > 5 * mDabPar.L && iCurOfdmSymbIdx == mNextShownOfdmSymbIdx);

  for (int16_t nomCarrIdx = 0; nomCarrIdx < mDabPar.K; ++nomCarrIdx)
  {
    // We do not interchange the positive/negative frequencies to their right positions, the de-interleaving understands this.
    int16_t fftIdx = mFreqInterleaver.map_k_to_fft_bin(nomCarrIdx);
    int16_t realCarrRelIdx = fftIdx;

    if (fftIdx < 0)
    {
      realCarrRelIdx += mDabPar.K / 2;
      fftIdx += mDabPar.T_u;
    }
    else // > 0 (there is no 0)
    {
      realCarrRelIdx += mDabPar.K / 2 - 1;
    }

    const int16_t dataVecCarrIdx = (mShowNomCarrier ? nomCarrIdx : realCarrRelIdx);

    constexpr float ALPHA = 0.005f;

    /*
     * Decoding is computing the phase difference between carriers with the same index in subsequent blocks.
     * The carrier of a block is the reference for the carrier on the same position in the next block.
     */
    cmplx fftBin = mFftBuffer[fftIdx] * norm_to_length_one(conj(mPhaseReference[fftIdx])); // PI/4-DQPSK demodulation

    float & integAbsPhasePerBinRef = mIntegAbsPhaseVector[nomCarrIdx];

    const cmplx fftBinRaw = fftBin;
    //fftBin *= rotator; // Fine correction of phase which can't be done in the time domain (not more necessary as it is done below other way).
    fftBin *= cmplx_from_phase(-integAbsPhasePerBinRef);

    // Get mean of absolute phase for each bin.
    const float fftBinPhase = std::arg(fftBin);
    const float fftBinAbsPhase = turn_phase_to_first_quadrant(fftBinPhase);

    // Integrate phase error to perform the phase correction in the next OFDM symbol.
    integAbsPhasePerBinRef += ALPHA * (fftBinAbsPhase - F_M_PI_4);
    limit_symmetrically(integAbsPhasePerBinRef, F_RAD_PER_DEG * 20.0f);

    // Get standard deviation of absolute phase for each bin. The integrator above assures that F_M_PI_4 is the phase mean value.
    const float curStdDevDiff = (fftBinAbsPhase -  F_M_PI_4);
    const float curStdDevSq = curStdDevDiff * curStdDevDiff;
    float & stdDevSqRef =  mStdDevSqPhaseVector[nomCarrIdx];
    mean_filter(stdDevSqRef, curStdDevSq, ALPHA);
    const float avgStdDev = std::sqrt(stdDevSqRef); // This value should be only between 0 (no noise) and M_PI_4 (heavy noise etc.).

    // Finally calculate (and limit) a soft bit weight from the standard deviation for each bin.
    float weight = 127.0f * ((float)M_PI_4 - avgStdDev) / F_M_PI_4;
    limit_min_max(weight, 2.0f, 127.0f);  // at least 2 as viterbi shows problems with only 1

    // Calculate the mean of power of each bin to equalize the IQ diagram (simply looks nicer) and use it for SNR calculation.
    // Simplification: The IQ plot would need only the level, not power, so the root of the power is used (below)..
    const float fftBinAbsLevel = std::abs(fftBin);
    const float fftBinPower = fftBinAbsLevel * fftBinAbsLevel;

    float & meanPowerPerBinRef = mMeanPowerVector[nomCarrIdx];
    mean_filter(meanPowerPerBinRef, fftBinPower, ALPHA);
    mean_filter(mMeanPowerOvrAll, fftBinPower, ALPHA / mDabPar.K);

    if (showScopeData)
    {
      switch (mIqPlotType)
      {
      case EIqPlotType::RAW_MEAN_NORMED:        mIqVector[nomCarrIdx] = fftBinRaw / sqrt(mMeanPowerOvrAll); break;
      case EIqPlotType::PHASE_CORR_MEAN_NORMED: mIqVector[nomCarrIdx] = fftBin / sqrt(mMeanPowerOvrAll); break;
      case EIqPlotType::PHASE_CORR_CARR_NORMED: mIqVector[nomCarrIdx] = fftBin / sqrt(meanPowerPerBinRef); break;
      }

      switch (mCarrierPlotType)
      {
      case ECarrierPlotType::FOUR_QUAD_PHASE: mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(std::arg(fftBin)); break;
      case ECarrierPlotType::MOD_QUAL:        mCarrVector[dataVecCarrIdx] = 100.0f / 127.0f * weight;  break;
      case ECarrierPlotType::STD_DEV:         mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(avgStdDev); break;
      case ECarrierPlotType::REL_POWER:       mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(meanPowerPerBinRef / mMeanPowerOvrAll); break;
      case ECarrierPlotType::PHASE_ERROR:     mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(integAbsPhasePerBinRef); break;
      case ECarrierPlotType::NULL_TII:        mCarrVector[dataVecCarrIdx] = mAvgAbsNullLevelWithTIIGain * (mMeanNullLevelWithTII[fftIdx] - mAvgAbsNullLevelWithTIIMin); break;
      case ECarrierPlotType::SNR:             mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(meanPowerPerBinRef / mMeanNullPowerWithoutTII[fftIdx]); break;
      }
    }

    if (!mUseOldSoftBitGen)
    {
      oBits[0         + nomCarrIdx] = (int16_t)(real(fftBin) < 0.0f ? weight : -weight);
      oBits[mDabPar.K + nomCarrIdx] = (int16_t)(imag(fftBin) < 0.0f ? weight : -weight);
    }
    else // old style of soft bit generation
    {
      cmplx r1 = mFftBuffer[fftIdx] * conj(mPhaseReference[fftIdx]);
      const float ab1 = abs(r1);
      // split the real and the imaginary part and scale it we make the bits into softbits in the range -127 .. 127 (+/- 255?)
      oBits[0         + nomCarrIdx] = (int16_t)(-(real(r1) * 255.0f) / ab1);
      oBits[mDabPar.K + nomCarrIdx] = (int16_t)(-(imag(r1) * 255.0f) / ab1);
    }
  } // for (nomCarrIdx...


  //	From time to time we show the constellation of the current symbol
  if (showScopeData)
  {
    mpIqBuffer->putDataIntoBuffer(mIqVector.data(), mDabPar.K);
    mpCarrBuffer->putDataIntoBuffer(mCarrVector.data(), mDabPar.K);
    emit signal_slot_show_iq(mDabPar.K, 1.0f /*mGain / TOP_VAL*/);
    mShowCntIqScope = 0;
  }

  if (showStatisticData)
  {
    mQD.CurOfdmSymbolNo = iCurOfdmSymbIdx + 1; // as "idx" goes from 0...(L-1)
    mQD.StdDeviation = _compute_mod_quality(mIqVector);
    mQD.TimeOffset = _compute_time_offset(mFftBuffer, mPhaseReference);
    mQD.FreqOffset = _compute_frequency_offset(mFftBuffer, mPhaseReference);
    mQD.PhaseCorr = -conv_rad_to_deg(iPhaseCorr);
    mQD.SNR = 10.0f * std::log10(mMeanPowerOvrAll / _compute_noise_Power());
    emit signal_show_mod_quality(&mQD);
    mShowCntStatistics = 0;
    mNextShownOfdmSymbIdx = (mNextShownOfdmSymbIdx + 1) % mDabPar.L;
    if (mNextShownOfdmSymbIdx == 0) mNextShownOfdmSymbIdx = 1; // as iCurSymbolNo can never be zero here
  }

  memcpy(mPhaseReference.data(), mFftBuffer.data(), mDabPar.T_u * sizeof(cmplx));
}

float OfdmDecoder::_compute_mod_quality(const std::vector<cmplx> & v) const
{
  /*
   * Since we do not equalize, we have a kind of "fake" reference point.
   * The key parameter here is the phase offset, so we compute the std.
   * deviation of the phases rather than the computation from the Modulation
   * Error Ratio as specified in Tr 101 290
   */

  constexpr cmplx rotator = cmplx(1.0f, -1.0f); // this is the reference phase shift to get the phase zero degree
  float squareVal = 0;

  for (int i = 0; i < mDabPar.K; i++)
  {
    float x1 = arg(cmplx(abs(real(v[i])), abs(imag(v[i]))) * rotator); // map to top right section and shift phase to zero (nominal)
    squareVal += x1 * x1;
  }

  return sqrtf(squareVal / (float)mDabPar.K) / (float)M_PI * 180.0f; // in degree
}

/*
 * While DAB symbols do not carry pilots, it is known that
 * arg (carrier [i, j] * conj (carrier [i + 1, j])
 * should be K * M_PI / 4,  (k in {1, 3, 5, 7}) so basically
 * carriers in decoded symbols can be used as if they were pilots
 * so, with that in mind we experiment with formula 5.39
 * and 5.40 from "OFDM Baseband Receiver Design for Wireless
 * Communications (Chiueh and Tsai)"
 */
float OfdmDecoder::_compute_time_offset(const std::vector<cmplx> & r, const std::vector<cmplx> & v) const
{
  cmplx sum = cmplx(0, 0);

  for (int i = -mDabPar.K / 2; i < mDabPar.K / 2; i += 6)
  {
    int index_1 = i < 0 ? i + mDabPar.T_u : i;
    int index_2 = (i + 1) < 0 ? (i + 1) + mDabPar.T_u : (i + 1);

    cmplx s = r[index_1] * conj(v[index_2]);

    s = cmplx(abs(real(s)), abs(imag(s)));
    const cmplx leftTerm = s * conj(cmplx(fabs(s) / sqrtf(2), fabs(s) / sqrtf(2)));

    s = r[index_2] * conj(v[index_2]);
    s = cmplx(abs(real(s)), abs(imag(s)));
    const cmplx rightTerm = s * conj(cmplx(fabs(s) / sqrtf(2), fabs(s) / sqrtf(2)));

    sum += conj(leftTerm) * rightTerm;
  }

  return arg(sum);
}

float OfdmDecoder::_compute_frequency_offset(const std::vector<cmplx> & r, const std::vector<cmplx> & c) const
{
  cmplx theta = cmplx(0, 0);

  for (int i = -mDabPar.K / 2; i < mDabPar.K / 2; i += 6)
  {
    int index = i < 0 ? i + mDabPar.T_u : i;
    cmplx val = r[index] * conj(c[index]);
    val = cmplx(abs(real(val)), abs(imag(val)));
    theta += val;
  }
  theta *= cmplx(1, -1);

  return arg(theta) / (2.0f * (float)M_PI) * 2048000.0f / (float)mDabPar.T_u; // TODO: hard coded 2048000?
}

float OfdmDecoder::_compute_clock_offset(const cmplx * r, const cmplx * v) const
{
  float offsa = 0;
  int offsb = 0;

  for (int i = -mDabPar.K / 2; i < mDabPar.K / 2; i += 6)
  {
    int index = i < 0 ? (i + mDabPar.T_u) : i;
    int index_2 = i + mDabPar.K / 2;
    cmplx a1 = cmplx(abs(real(r[index])), abs(imag(r[index])));
    cmplx a2 = cmplx(abs(real(v[index])), abs(imag(v[index])));
    float s = abs(arg(a1 * conj(a2)));
    offsa += (float)index * s;
    offsb += index_2 * index_2;
  }

  return offsa / (2.0f * (float)M_PI * (float)mDabPar.T_s / (float)mDabPar.T_u * (float)offsb);
}

float OfdmDecoder::_compute_noise_Power() const
{
  float sumNoise = 0.0f;
  for (const auto & v : mMeanNullPowerWithoutTII)
  {
    sumNoise += v; // the component already squared
  }
  return sumNoise / (float)mMeanNullPowerWithoutTII.size();
}

void OfdmDecoder::slot_select_carrier_plot_type(ECarrierPlotType iPlotType)
{
  mCarrierPlotType = iPlotType;
}

void OfdmDecoder::slot_select_iq_plot_type(EIqPlotType iPlotType)
{
  mIqPlotType = iPlotType;
}

void OfdmDecoder::slot_show_nominal_carrier(bool iShowNominalCarrier)
{
  mShowNomCarrier = iShowNominalCarrier;
}

void OfdmDecoder::slot_use_old_soft_bit_gen(bool iUseOldSoftBitGen)
{
  mUseOldSoftBitGen = iUseOldSoftBitGen;
}
