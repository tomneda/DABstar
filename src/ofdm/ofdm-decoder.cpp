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

  mMeanNullLevel.resize(mDabPar.T_u);
  mMeanNullPowerWithoutTII.resize(mDabPar.T_u);
  mPhaseReference.resize(mDabPar.T_u);
  mFftBuffer.resize(mDabPar.T_u);
  mIqVector.resize(mDabPar.K);
  mCarrVector.resize(mDabPar.K);
  mStdDevSqPhaseVector.resize(mDabPar.K);
  mIntegAbsPhaseVector.resize(mDabPar.K);
  mMeanPhaseVector.resize(mDabPar.K);
  mMeanPowerVector.resize(mDabPar.K);
  mMeanLevelVector.resize(mDabPar.K);
  mMeanSigmaSqVector.resize(mDabPar.K);

  reset();
}

void OfdmDecoder::reset()
{
  std::fill(mStdDevSqPhaseVector.begin(), mStdDevSqPhaseVector.end(), 0.0f);
  std::fill(mIntegAbsPhaseVector.begin(), mIntegAbsPhaseVector.end(), 0.0f);
  std::fill(mMeanPhaseVector.begin(), mMeanPhaseVector.end(), 0.0f);
  std::fill(mMeanPowerVector.begin(), mMeanPowerVector.end(), 0.0f);
  std::fill(mMeanLevelVector.begin(), mMeanLevelVector.end(), 0.0f);
  std::fill(mMeanSigmaSqVector.begin(), mMeanSigmaSqVector.end(), 0.0f);
  std::fill(mMeanNullPowerWithoutTII.begin(), mMeanNullPowerWithoutTII.end(), 0.0f);

  mMeanModQual = 0.0f;
  mMeanPowerOvrAll = 1.0f;
  mLlrScaling = 1.0f;

  _reset_null_symbol_statistics();
}

void OfdmDecoder::store_null_symbol_with_tii(const std::vector<cmplx> & iV) // with TII information
{
  if (mCarrierPlotType != ECarrierPlotType::NULL_TII)
  {
    return;
  }

  memcpy(mFftBuffer.data(), &(iV[mDabPar.T_g]), mDabPar.T_u * sizeof(cmplx));

  mFftHandler.fft(mFftBuffer);

  _eval_null_symbol_statistics();

}

void OfdmDecoder::store_null_symbol_without_tii(const std::vector<cmplx> & iV) // with TII information
{
  memcpy(mFftBuffer.data(), &(iV[mDabPar.T_g]), mDabPar.T_u * sizeof(cmplx));

  mFftHandler.fft(mFftBuffer);

  if (mCarrierPlotType == ECarrierPlotType::NULL_NO_TII)
  {
    _eval_null_symbol_statistics();
  }

  constexpr float ALPHA = 0.1f;

  for (int32_t idx = -mDabPar.K / 2; idx < mDabPar.K / 2; ++idx)
  {
    // Consider FFT shift and skipping DC (0 Hz) bin.
    const int32_t fftIdx = fft_shift_skip_dc(idx, mDabPar.T_u);
    const float level = std::abs(mFftBuffer[fftIdx]);
    mean_filter(mMeanNullPowerWithoutTII[fftIdx], level * level, ALPHA);
  }
}

void OfdmDecoder::store_reference_symbol_0(std::vector<cmplx> buffer) // copy is intended as used as fft buffer
{
  mFftHandler.fft(buffer);

  // We are now in the frequency domain, and we keep the carriers as coming from the FFT as phase reference.
  memcpy(mPhaseReference.data(), buffer.data(), mDabPar.T_u * sizeof(cmplx));
}

void OfdmDecoder::decode_symbol(const std::vector<cmplx> & iV, uint16_t iCurOfdmSymbIdx, float iPhaseCorr, std::vector<int16_t> & oBits)
{
  memcpy(mFftBuffer.data(), &(iV[mDabPar.T_g]), mDabPar.T_u * sizeof(cmplx));

  mFftHandler.fft(mFftBuffer);

  //const cmplx rotator = std::exp(cmplx(0.0f, -iPhaseCorr)); // fine correction of phase which can't be done in the time domain

  ++mShowCntIqScope;
  ++mShowCntStatistics;
  const bool showScopeData = (mShowCntIqScope > mDabPar.L && iCurOfdmSymbIdx == mNextShownOfdmSymbIdx);
  const bool showStatisticData = (mShowCntStatistics > 5 * mDabPar.L && iCurOfdmSymbIdx == mNextShownOfdmSymbIdx);
  float llrMax = 0;

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
    const cmplx fftBinRaw = mFftBuffer[fftIdx] * norm_to_length_one(conj(mPhaseReference[fftIdx])); // PI/4-DQPSK demodulation
    //const cmplx fftBinRaw = mFftBuffer[fftIdx] * /*norm_to_length_one*/(conj(mPhaseReference[fftIdx])); // PI/4-DQPSK demodulation

    float & integAbsPhasePerBinRef = mIntegAbsPhaseVector[nomCarrIdx];

    //fftBin *= rotator; // Fine correction of phase which can't be done in the time domain (not more necessary as it is done below other way).
    const cmplx fftBin = fftBinRaw * cmplx_from_phase(-integAbsPhasePerBinRef);

    // Get phase and absolute phase for each bin.
    const float fftBinPhase = std::arg(fftBin);
    const float fftBinAbsPhase = turn_phase_to_first_quadrant(fftBinPhase);

    // Integrate phase error to perform the phase correction in the next OFDM symbol.
    integAbsPhasePerBinRef += 0.01f * ALPHA * (fftBinAbsPhase - F_M_PI_4); // TODO: correction still necessary?
    limit_symmetrically(integAbsPhasePerBinRef, F_RAD_PER_DEG * 20.0f);

    // Get standard deviation of absolute phase for each bin. The integrator above assures that F_M_PI_4 is the phase mean value.
    const float curStdDevDiff = (fftBinAbsPhase -  F_M_PI_4);
    const float curStdDevSq = curStdDevDiff * curStdDevDiff;
    float & stdDevSqRef =  mStdDevSqPhaseVector[nomCarrIdx];
    mean_filter(stdDevSqRef, curStdDevSq, ALPHA);

    // Calculate the mean of power of each bin to equalize the IQ diagram (simply looks nicer) and use it for SNR calculation.
    // Simplification: The IQ plot would need only the level, not power, so the root of the power is used (below)..
    const float fftBinAbsLevel = std::abs(fftBin);
    const float fftBinPower = fftBinAbsLevel * fftBinAbsLevel;

    float & meanLevelPerBinRef = mMeanLevelVector[nomCarrIdx];
    float & meanPowerPerBinRef = mMeanPowerVector[nomCarrIdx];
    mean_filter(meanLevelPerBinRef, fftBinAbsLevel, ALPHA);
    mean_filter(meanPowerPerBinRef, fftBinPower, ALPHA);
    mean_filter(mMeanPowerOvrAll, fftBinPower, ALPHA / (float)mDabPar.K);

    // Collect data for "Log Likelihood Ratio"
    const float meanLevelAtAxisPerBin = meanLevelPerBinRef * F_SQRT1_2;
    const float realLevelDistPerBin = (std::abs(real(fftBin)) - meanLevelAtAxisPerBin); // x-distance to reference point
    const float imagLevelDistPerBin = (std::abs(imag(fftBin)) - meanLevelAtAxisPerBin); // y-distance to reference point
    const float sigmaSqPerBin = realLevelDistPerBin * realLevelDistPerBin + imagLevelDistPerBin * imagLevelDistPerBin; // squared distance to reference point
    float & meanSigmaSqPerBinRef = mMeanSigmaSqVector[nomCarrIdx];
    mean_filter(meanSigmaSqPerBinRef, sigmaSqPerBin, ALPHA);
    float llrBitReal = -mLlrScaling * 2 * meanLevelPerBinRef / meanSigmaSqPerBinRef * real(fftBin);
    float llrBitImag = -mLlrScaling * 2 * meanLevelPerBinRef / meanSigmaSqPerBinRef * imag(fftBin);
    const float maxAbs = std::max(std::abs(llrBitReal), std::abs(llrBitImag));
    if (maxAbs > llrMax) llrMax = maxAbs; // for best soft bit scaling
    limit_symmetrically(llrBitReal, F_VITERBI_SOFT_BIT_VALUE_MAX);
    limit_symmetrically(llrBitImag, F_VITERBI_SOFT_BIT_VALUE_MAX);
    const float weightRrl = (std::abs(llrBitReal) + std::abs(llrBitImag)) / 2.0f;
    mean_filter(mMeanModQual, weightRrl, ALPHA / (float)mDabPar.K);

    // Calculate (and limit) a soft-bit weight
    float weight = 0.0f;
    float qtDabWeight = 0.0f;

    /*
     * The viterbi decoder will limit the soft-bit values to +/-127, but the "255" were so in Qt-DAB. The limitation below is only
     * for the carrier plot.
     */
    switch (mSoftBitType)
    {
    case ESoftBitType::LLR:         weight = weightRrl; break;
    case ESoftBitType::AVER:        weight = F_VITERBI_SOFT_BIT_VALUE_MAX * (F_M_PI_4 - std::sqrt(stdDevSqRef)) / F_M_PI_4; break;
    case ESoftBitType::FAST:        weight = F_VITERBI_SOFT_BIT_VALUE_MAX * (F_M_PI_4 - std::abs(curStdDevDiff)) / F_M_PI_4; break;
    case ESoftBitType::HARD:        weight = F_VITERBI_SOFT_BIT_VALUE_MAX; break;
    case ESoftBitType::MAX_DIST_IQ:
    case ESoftBitType::EUCL_DIST:   qtDabWeight = F_VITERBI_SOFT_BIT_VALUE_MAX; break;
    case ESoftBitType::EUCL_DIST_2: qtDabWeight = 2 * F_VITERBI_SOFT_BIT_VALUE_MAX + 1; break; // F_VITERBI_SOFT_BIT_VALUE_MAX is (intentionally?) overflown here
    }

    if (mSoftBitType == ESoftBitType::LLR)
    {
      oBits[0         + nomCarrIdx] = (int16_t)(llrBitReal);
      oBits[mDabPar.K + nomCarrIdx] = (int16_t)(llrBitImag);
    }
    else if (qtDabWeight != 0.0f)
    {
      // Original Qt-DAB decoding is with qtDabWeight == 255.0 and performs imo better than with 127.0 (F_VITERBI_SOFT_BIT_VALUE_MAX)
      const cmplx v = mFftBuffer[fftIdx] * conj(mPhaseReference[fftIdx]);
      const float vLen = (mSoftBitType == ESoftBitType::MAX_DIST_IQ ? std::max(std::abs(real(v)), std::abs(imag(v))) : std::abs(v));
      oBits[0         + nomCarrIdx] = (int16_t)(-(real(v) * qtDabWeight) / vLen);
      oBits[mDabPar.K + nomCarrIdx] = (int16_t)(-(imag(v) * qtDabWeight) / vLen);
      weight = std::abs(std::min(real(v), imag(v)) * qtDabWeight / vLen); // for the carrier scope: have to decide for real or imag part, take the min of both
      limit_min_max(weight, 0.0f, F_VITERBI_SOFT_BIT_VALUE_MAX);
    }
    else
    {
      limit_min_max(weight, 2.0f, F_VITERBI_SOFT_BIT_VALUE_MAX);  // at least 2 as viterbi shows problems with only 1
      oBits[0         + nomCarrIdx] = (int16_t)(real(fftBin) < 0.0f ? weight : -weight);
      oBits[mDabPar.K + nomCarrIdx] = (int16_t)(imag(fftBin) < 0.0f ? weight : -weight);
    }

    if (showScopeData)
    {
      switch (mIqPlotType)
      {
      case EIqPlotType::PHASE_CORR_CARR_NORMED: mIqVector[nomCarrIdx] = fftBin / sqrt(meanPowerPerBinRef); break;
      case EIqPlotType::PHASE_CORR_MEAN_NORMED: mIqVector[nomCarrIdx] = fftBin / sqrt(mMeanPowerOvrAll); break;
      case EIqPlotType::RAW_MEAN_NORMED:        mIqVector[nomCarrIdx] = fftBinRaw / sqrt(mMeanPowerOvrAll); break;
      case EIqPlotType::DC_OFFSET_FFT_10:       mIqVector[nomCarrIdx] =  10.0f / (float)mDabPar.T_u * _interpolate_2d_plane(mPhaseReference[0], mFftBuffer[0], (float)nomCarrIdx / ((float)mIqVector.size() - 1)); break;
      case EIqPlotType::DC_OFFSET_FFT_100:      mIqVector[nomCarrIdx] = 100.0f / (float)mDabPar.T_u * _interpolate_2d_plane(mPhaseReference[0], mFftBuffer[0], (float)nomCarrIdx / ((float)mIqVector.size() - 1)); break;
      case EIqPlotType::DC_OFFSET_ADC_10:       mIqVector[nomCarrIdx] =  10.0f * mDcAdc; break;
      case EIqPlotType::DC_OFFSET_ADC_100:      mIqVector[nomCarrIdx] = 100.0f * mDcAdc; break;
      }

      switch (mCarrierPlotType)
      {
      case ECarrierPlotType::SB_WEIGHT:       mCarrVector[dataVecCarrIdx] = 100.0f / VITERBI_SOFT_BIT_VALUE_MAX * weight;  break;
      case ECarrierPlotType::EVM_DB:          mCarrVector[dataVecCarrIdx] = 20.0f * std::log10(std::sqrt(meanSigmaSqPerBinRef) / meanLevelPerBinRef); break;
      case ECarrierPlotType::EVM_PER:         mCarrVector[dataVecCarrIdx] = 100.0f * std::sqrt(meanSigmaSqPerBinRef) / meanLevelPerBinRef; break;
      case ECarrierPlotType::STD_DEV:         mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(std::sqrt(stdDevSqRef)); break;
      case ECarrierPlotType::PHASE_ERROR:     mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(integAbsPhasePerBinRef); break;
      case ECarrierPlotType::FOUR_QUAD_PHASE: mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(std::arg(fftBin)); break;
      case ECarrierPlotType::REL_POWER:       mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(meanPowerPerBinRef / mMeanPowerOvrAll); break;
      case ECarrierPlotType::SNR:             mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(meanPowerPerBinRef / mMeanNullPowerWithoutTII[fftIdx]); break;
      case ECarrierPlotType::NULL_TII:
      case ECarrierPlotType::NULL_NO_TII:     mCarrVector[dataVecCarrIdx] = mAbsNullLevelGain * (mMeanNullLevel[fftIdx] - mAbsNullLevelMin); break;
      case ECarrierPlotType::NULL_OVR_POW:    mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(mMeanNullPowerWithoutTII[fftIdx] / mMeanPowerOvrAll); break;
      }
    }
  } // for (nomCarrIdx...

  // perform scaling for LLR softbits to use the full range soft-bit range on viterbi input
  {
    mLlrScaling += 0.0001f * (F_VITERBI_SOFT_BIT_VALUE_MAX - llrMax);
    limit_min_max(mLlrScaling, 1.0f, 100.0f); // limit value to ensure minimum decoding ability (TODO: is top value 100 useful?)
    //qDebug("LLR scaling: %2.2f, (max: %3.0f, diff: %4.0f)", mLlrScaling, llrMax, F_VITERBI_SOFT_BIT_VALUE_MAX - llrMax);
  }

  //	From time to time we show the constellation of the current symbol
  if (showScopeData)
  {
    mpIqBuffer->put_data_into_ring_buffer(mIqVector.data(), mDabPar.K);
    mpCarrBuffer->put_data_into_ring_buffer(mCarrVector.data(), mDabPar.K);
    emit signal_slot_show_iq(mDabPar.K, 1.0f /*mGain / TOP_VAL*/);
    mShowCntIqScope = 0;
  }

  if (showStatisticData)
  {
    const float noisePow = _compute_noise_Power();
    mQD.CurOfdmSymbolNo = iCurOfdmSymbIdx + 1; // as "idx" goes from 0...(L-1)
    mQD.ModQuality = 100.0f / F_VITERBI_SOFT_BIT_VALUE_MAX * mMeanModQual;
    mQD.TimeOffset = _compute_time_offset(mFftBuffer, mPhaseReference);
    mQD.FreqOffset = _compute_frequency_offset(mFftBuffer, mPhaseReference);
    mQD.PhaseCorr = -conv_rad_to_deg(iPhaseCorr);
    mQD.SNR = 10.0f * std::log10(mMeanPowerOvrAll / noisePow);
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
    float x1 = arg(cmplx(std::abs(real(v[i])), std::abs(imag(v[i]))) * rotator); // map to top right section and shift phase to zero (nominal)
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

    s = cmplx(std::abs(real(s)), std::abs(imag(s)));
    const cmplx leftTerm = s * conj(cmplx(std::abs(s) / sqrtf(2), std::abs(s) / sqrtf(2)));

    s = r[index_2] * conj(v[index_2]);
    s = cmplx(std::abs(real(s)), std::abs(imag(s)));
    const cmplx rightTerm = s * conj(cmplx(std::abs(s) / sqrtf(2), std::abs(s) / sqrtf(2)));

    sum += conj(leftTerm) * rightTerm;
  }

  return arg(sum);
}

float OfdmDecoder::_compute_frequency_offset(const std::vector<cmplx> & r, const std::vector<cmplx> & c) const
{
  cmplx theta = cmplx(0, 0);

  for (int idx = -mDabPar.K / 2; idx < mDabPar.K / 2; idx += 6)
  {
    const int32_t index = fft_shift_skip_dc(idx, mDabPar.T_u); // this was with DC before in QT-DAB
    cmplx val = r[index] * conj(c[index]);
    val = cmplx(std::abs(real(val)), std::abs(imag(val))); // TODO tomneda: is this correct?
    theta += val;
  }
  theta *= cmplx(1, -1);

  return std::arg(theta) / F_2_M_PI * (float)mDabPar.CarrDiff;
}

float OfdmDecoder::_compute_clock_offset(const cmplx * r, const cmplx * v) const
{
  float offsa = 0;
  int offsb = 0;

  for (int i = -mDabPar.K / 2; i < mDabPar.K / 2; i += 6)
  {
    int index = i < 0 ? (i + mDabPar.T_u) : i; // TODO tomneda: i+1 to skip DC?
    int index_2 = i + mDabPar.K / 2;
    cmplx a1 = cmplx(std::abs(real(r[index])), std::abs(imag(r[index])));
    cmplx a2 = cmplx(std::abs(real(v[index])), std::abs(imag(v[index])));
    float s = std::abs(arg(a1 * conj(a2)));
    offsa += (float)index * s;
    offsb += index_2 * index_2;
  }

  return offsa / (2.0f * (float)M_PI * (float)mDabPar.T_s / (float)mDabPar.T_u * (float)offsb);
}

float OfdmDecoder::_compute_noise_Power() const
{
  float sumNoise = 0.0f;
  for (int32_t idx = -mDabPar.K / 2; idx < mDabPar.K / 2; ++idx)
  {
    // Consider FFT shift and skipping DC (0 Hz) bin.
    const int32_t fftIdx = fft_shift_skip_dc(idx, mDabPar.T_u);
    sumNoise += mMeanNullPowerWithoutTII[fftIdx]; // the component already squared
  }
  return sumNoise / (float)mDabPar.K;
}

void OfdmDecoder::_eval_null_symbol_statistics()
{
  constexpr float ALPHA = 0.20f;
  float max = -1e38f;
  float min =  1e38f;

  for (int32_t idx = -mDabPar.K / 2; idx < mDabPar.K / 2; ++idx)
  {
    // Consider FFT shift and skipping DC (0 Hz) bin.
    const int32_t fftIdx = fft_shift_skip_dc(idx, mDabPar.T_u);
    const float level = std::abs(mFftBuffer[fftIdx]);
    float & meanNullLevelRef = mMeanNullLevel[fftIdx];
    mean_filter(meanNullLevelRef, level, ALPHA);

    if (meanNullLevelRef < min) min = meanNullLevelRef;
    if (meanNullLevelRef > max) max = meanNullLevelRef;
  }

  assert(max > min);
  mAbsNullLevelMin = min;
  mAbsNullLevelGain = 100.0f / (max - min);
}

void OfdmDecoder::_reset_null_symbol_statistics()
{
  std::fill(mMeanNullLevel.begin(), mMeanNullLevel.end(), 0.0f);
  mAbsNullLevelMin = 0.0f;
  mAbsNullLevelGain = 0.0f;
}

void OfdmDecoder::set_select_carrier_plot_type(ECarrierPlotType iPlotType)
{
  if (iPlotType == ECarrierPlotType::NULL_TII || iPlotType == ECarrierPlotType::NULL_NO_TII)
  {
    _reset_null_symbol_statistics();
  }
  mCarrierPlotType = iPlotType;
}

void OfdmDecoder::set_select_iq_plot_type(EIqPlotType iPlotType)
{
  mIqPlotType = iPlotType;
}

void OfdmDecoder::set_soft_bit_gen_type(ESoftBitType iSoftBitType)
{
  mSoftBitType = iSoftBitType;
}

void OfdmDecoder::set_show_nominal_carrier(bool iShowNominalCarrier)
{
  mShowNomCarrier = iShowNominalCarrier;
}

cmplx OfdmDecoder::_interpolate_2d_plane(const cmplx & iStart, const cmplx & iEnd, float iPar)
{
  assert(iPar >= 0.0f && iPar <= 1.0f);
  const float a_r = real(iStart);
  const float a_i = imag(iStart);
  const float b_r = real(iEnd);
  const float b_i = imag(iEnd);
  const float c_r = (b_r - a_r) * iPar + a_r;
  const float c_i = (b_i - a_i) * iPar + a_i;
  return { c_r, c_i };
}
