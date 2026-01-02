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
 *    Once the bits are "in", interpretation and manipulation
 *    should reconstruct the data blocks.
 *    Ofdm_decoder is called for Block_0 and the FIC blocks,
 *    its invocation results in 2 * Tu bits
 */
#include "ofdm-decoder.h"
#include "phasetable.h"
#include "dabradio.h"
#include <vector>

OfdmDecoder::OfdmDecoder(DabRadio * ipMr, RingBuffer<cf32> * ipIqBuffer, RingBuffer<f32> * ipCarrBuffer) :
  mpRadioInterface(ipMr),
  mFreqInterleaver(),
  mpIqBuffer(ipIqBuffer),
  mpCarrBuffer(ipCarrBuffer)
{
  mMeanNullLevel.resize(cTu);
  mMeanNullPowerWithoutTII.resize(cTu);
  mPhaseReference.resize(cTu);
  mIqVector.resize(cK);
  mCarrVector.resize(cK);
  mStdDevSqPhaseVector.resize(cK);
  mIntegAbsPhaseVector.resize(cK);
  mMeanPhaseVector.resize(cK);
  mMeanPowerVector.resize(cK);
  mMeanSigmaSqVector.resize(cK);

  connect(this, &OfdmDecoder::signal_slot_show_iq, mpRadioInterface, &DabRadio::slot_show_iq);
  connect(this, &OfdmDecoder::signal_show_lcd_data, mpRadioInterface, &DabRadio::slot_show_lcd_data);

  reset();
}

OfdmDecoder::~OfdmDecoder()
{
}

cf32 OfdmDecoder::cmplx_from_phase2(f32 x)
{
  /*
   * Minimax polynomial for sin(x) and cos(x) on [-pi/4, pi/4]
   * Coefficients via Remez algorithm (Sollya)
   * Max |error| < 1.5e-4 for sinus, < 6e-4 for cosinus
   */
  const f32 x2 = x * x;
  const f32 s1 =  0.99903142452239990234375f;
  const f32 s2 = -0.16034401953220367431640625;
  const f32 sine = x * (x2 * s2 + s1);

  const f32 c1 =  0.9994032382965087890625f;
  const f32 c2 = -0.495580852031707763671875;
  const f32 c3 =  3.679168224334716796875e-2;
  const f32 cosine = c1 + x2 * (x2 * c3 + c2);

  return cf32(cosine, sine);
}

void OfdmDecoder::reset()
{
  std::fill(mStdDevSqPhaseVector.begin(), mStdDevSqPhaseVector.end(), 0.0f);
  std::fill(mIntegAbsPhaseVector.begin(), mIntegAbsPhaseVector.end(), 0.0f);
  std::fill(mMeanPhaseVector.begin(), mMeanPhaseVector.end(), 0.0f);
  std::fill(mMeanPowerVector.begin(), mMeanPowerVector.end(), 0.0f);
  std::fill(mMeanSigmaSqVector.begin(), mMeanSigmaSqVector.end(), 0.0f);
  std::fill(mMeanNullPowerWithoutTII.begin(), mMeanNullPowerWithoutTII.end(), 0.0f);

  mMeanPowerOvrAll = 1.0f;

  _reset_null_symbol_statistics();
}

void OfdmDecoder::store_null_symbol_with_tii(const TArrayTu & iV) // with TII information
{
  if (mCarrierPlotType != ECarrierPlotType::NULL_TII_LIN &&
      mCarrierPlotType != ECarrierPlotType::NULL_TII_LOG)
  {
    return;
  }

  _eval_null_symbol_statistics(iV);
}

void OfdmDecoder::store_null_symbol_without_tii(const TArrayTu & iV) // with TII information
{
  if (mCarrierPlotType == ECarrierPlotType::NULL_NO_TII)
  {
    _eval_null_symbol_statistics(iV);
  }

  constexpr f32 ALPHA = 0.1f;

  for (i32 idx = -cK / 2; idx < cK / 2; ++idx)
  {
    // Consider FFT shift and skipping DC (0 Hz) bin.
    const i32 fftIdx = fft_shift_skip_dc(idx, cTu);
    const f32 level = std::abs(iV[fftIdx]);
    mean_filter(mMeanNullPowerWithoutTII[fftIdx], level * level, ALPHA);
  }
}

void OfdmDecoder::store_reference_symbol_0(const TArrayTu & iBuffer)
{
  // We are now in the frequency domain, and we keep the carriers as coming from the FFT as phase reference.
  memcpy(mPhaseReference.data(), iBuffer.data(), cTu * sizeof(cf32));
}

void OfdmDecoder::decode_symbol(const TArrayTu & iV, const u16 iCurOfdmSymbIdx, const f32 iPhaseCorr, std::vector<i16> & oBits)
{
  // current runtime on i7-6700K:
  // phase correction via:
  //   LUT:                avr: 240us, min: 108us
  //   cmplx_from_phase    avr: 235us, min: 101us
  //   cmplx_from_phase2   avr: 283us, min: 144us
  f32 sum = 0.0f;
  f32 stdDevSqOvrAll = 0.0f;

  ++mShowCntIqScope;
  ++mShowCntStatistics;
  const bool showScopeData = (mShowCntIqScope > cL && iCurOfdmSymbIdx == mNextShownOfdmSymbIdx);
  const bool showStatisticData = (mShowCntStatistics > 5 * cL && iCurOfdmSymbIdx == mNextShownOfdmSymbIdx);

  for (i16 nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)
  {
    // We do not interchange the positive/negative frequencies to their right positions, the de-interleaving understands this.
    i16 fftIdx = mFreqInterleaver.map_k_to_fft_bin(nomCarrIdx);
    i16 realCarrRelIdx = fftIdx;

    if (fftIdx < 0)
    {
      realCarrRelIdx += cK / 2;
      fftIdx += cTu;
    }
    else // > 0 (there is no 0)
    {
      realCarrRelIdx += cK / 2 - 1;
    }

    const i16 dataVecCarrIdx = (mShowNomCarrier ? nomCarrIdx : realCarrRelIdx);

    constexpr f32 ALPHA = 0.005f;

    /*
     * Decoding is computing the phase difference between carriers with the same index in subsequent blocks.
     * The carrier of a block is the reference for the carrier on the same position in the next block.
     */
    const cf32 fftBinRaw = iV[fftIdx] * norm_to_length_one(conj(mPhaseReference[fftIdx])); // PI/4-DQPSK demodulation

    f32 & integAbsPhasePerBinRef = mIntegAbsPhaseVector[nomCarrIdx];

    const cf32 fftBin = fftBinRaw * cmplx_from_phase2(-integAbsPhasePerBinRef);
    //const cf32 fftBin = fftBinRaw * cmplx_from_phase(-integAbsPhasePerBinRef);

    // Get phase and absolute phase for each bin.
    const f32 fftBinPhase = std::arg(fftBin);
    const f32 fftBinAbsPhase = turn_phase_to_first_quadrant(fftBinPhase);

    // Integrate phase error to perform the phase correction in the next OFDM symbol.
    integAbsPhasePerBinRef += 0.2f * ALPHA * (fftBinAbsPhase - F_M_PI_4); // TODO: correction still necessary?
    limit_symmetrically(integAbsPhasePerBinRef, F_RAD_PER_DEG * 20.0f); // this is important to not overdrive mLutPhase2Cmplx!

    // Get standard deviation of absolute phase for each bin. The integrator above assures that F_M_PI_4 is the phase mean value.
    const f32 curStdDevDiff = (fftBinAbsPhase -  F_M_PI_4);
    const f32 curStdDevSq = curStdDevDiff * curStdDevDiff;
    f32 & stdDevSqRef =  mStdDevSqPhaseVector[nomCarrIdx];
    mean_filter(stdDevSqRef, curStdDevSq, ALPHA);
    stdDevSqOvrAll += stdDevSqRef;

    // Calculate the mean of power of each bin to equalize the IQ diagram (simply looks nicer) and use it for SNR calculation.
    const f32 fftBinPower = std::norm(fftBin);
    f32 & meanPowerPerBinRef = mMeanPowerVector[nomCarrIdx];
    mean_filter(meanPowerPerBinRef, fftBinPower, ALPHA);
    mean_filter(mMeanPowerOvrAll, fftBinPower, ALPHA / (f32)cK);

    // Collect data for "Log Likelihood Ratio"
    const f32 meanLevelPerBinRef = std::sqrt(meanPowerPerBinRef);
    const f32 meanLevelAtAxisPerBin = meanLevelPerBinRef * F_SQRT1_2;
    const f32 realLevelDistPerBin = (std::abs(real(fftBin)) - meanLevelAtAxisPerBin); // x-distance to reference point
    const f32 imagLevelDistPerBin = (std::abs(imag(fftBin)) - meanLevelAtAxisPerBin); // y-distance to reference point
    const f32 sigmaSqPerBin = realLevelDistPerBin * realLevelDistPerBin + imagLevelDistPerBin * imagLevelDistPerBin; // squared distance to reference point
    f32 & meanSigmaSqPerBinRef = mMeanSigmaSqVector[nomCarrIdx];
    mean_filter(meanSigmaSqPerBinRef, sigmaSqPerBin, ALPHA);

    f32 signalPower = meanPowerPerBinRef - mMeanNullPowerWithoutTII[fftIdx];
    if (signalPower <= 0.0f) signalPower = 0.1f;

    // Calculate a soft-bit weight. The viterbi decoder will limit the soft-bit values to +/-127.
    f32 w1 = std::sqrt(std::abs(fftBin) * std::abs(mPhaseReference[fftIdx])); // input level
    if(mSoftBitType == ESoftBitType::SOFTDEC3)
      w1 *= meanPowerPerBinRef;
    else if(mSoftBitType == ESoftBitType::SOFTDEC2)
      w1 *= std::sqrt(meanLevelPerBinRef) * meanLevelPerBinRef;
    else
      w1 *= meanLevelPerBinRef;
    w1 /= meanSigmaSqPerBinRef;
    w1 /= (mMeanNullPowerWithoutTII[fftIdx] / signalPower) +
          (mSoftBitType == ESoftBitType::SOFTDEC3 ? 2.0f : 1.0f); // 1/SNR + (1 or 2)
    const cf32 r1 = norm_to_length_one(fftBin) * w1;
    f32 w2 = -100 / mMeanValue;

    // split the real and the imaginary part and scale it
    oBits[0  + nomCarrIdx] = (i16)(real(r1) * w2);
    oBits[cK + nomCarrIdx] = (i16)(imag(r1) * w2);
    sum += std::abs(r1);

    if (showScopeData)
    {
      switch (mIqPlotType)
      {
      case EIqPlotType::PHASE_CORR_CARR_NORMED: mIqVector[nomCarrIdx] = fftBin / meanLevelPerBinRef; break;
      case EIqPlotType::PHASE_CORR_MEAN_NORMED: mIqVector[nomCarrIdx] = fftBin / std::sqrt(mMeanPowerOvrAll); break;
      case EIqPlotType::RAW_MEAN_NORMED:        mIqVector[nomCarrIdx] = fftBinRaw / std::sqrt(mMeanPowerOvrAll); break;
      case EIqPlotType::DC_OFFSET_FFT_100:      mIqVector[nomCarrIdx] = 100.0f / (f32)cTu * _interpolate_2d_plane(mPhaseReference[0], iV[0], (f32)nomCarrIdx / ((f32)mIqVector.size() - 1)); break;
      case EIqPlotType::DC_OFFSET_ADC_100:      mIqVector[nomCarrIdx] = 100.0f * mDcAdc; break;
      }

      switch (mCarrierPlotType)
      {
      case ECarrierPlotType::SB_WEIGHT:
        // Convert and limit the soft bit weigth to percent
        w2 = (std::abs(real(r1)) + std::abs(imag(r1))) / 2.0f * (-w2);
        limit_min_max(w2, 0.0f, F_VITERBI_SOFT_BIT_VALUE_MAX);
        mCarrVector[dataVecCarrIdx] = 100.0f / VITERBI_SOFT_BIT_VALUE_MAX * w2;
        break;
      case ECarrierPlotType::EVM_PER:         mCarrVector[dataVecCarrIdx] = 100.0f * std::sqrt(meanSigmaSqPerBinRef) / meanLevelPerBinRef; break;
      case ECarrierPlotType::EVM_DB:          mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(meanSigmaSqPerBinRef / meanPowerPerBinRef); break;
      case ECarrierPlotType::STD_DEV:         mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(std::sqrt(stdDevSqRef)); break;
      case ECarrierPlotType::PHASE_ERROR:     mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(integAbsPhasePerBinRef); break;
      case ECarrierPlotType::FOUR_QUAD_PHASE: mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(std::arg(fftBin)); break;
      case ECarrierPlotType::REL_POWER:       mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(meanPowerPerBinRef / mMeanPowerOvrAll); break;
      case ECarrierPlotType::SNR:             mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(meanPowerPerBinRef / mMeanNullPowerWithoutTII[fftIdx]); break;
      case ECarrierPlotType::NULL_TII_LIN:
      case ECarrierPlotType::NULL_TII_LOG:
      case ECarrierPlotType::NULL_NO_TII:     mCarrVector[dataVecCarrIdx] = mAbsNullLevelGain * (mMeanNullLevel[fftIdx] - mAbsNullLevelMin); break;
      case ECarrierPlotType::NULL_OVR_POW:    mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(mMeanNullPowerWithoutTII[fftIdx] / mMeanPowerOvrAll); break;
      }
    }
  } // for (nomCarrIdx...

  mMeanValue = sum / cK;

  if (iCurOfdmSymbIdx == 1)
  {
    const f32 FreqCorr = iPhaseCorr / F_2_M_PI * (f32)cCarrDiff;
    mean_filter(mMeanSigmaSqFreqCorr, FreqCorr * FreqCorr, 0.2f);
  }

  //    From time to time we show the constellation of the current symbol
  if (showScopeData)
  {
    mpIqBuffer->put_data_into_ring_buffer(mIqVector.data(), cK);
    mpCarrBuffer->put_data_into_ring_buffer(mCarrVector.data(), cK);
    emit signal_slot_show_iq(cK, 1.0f /*mGain / TOP_VAL*/);
    mShowCntIqScope = 0;
  }

  if (showStatisticData)
  {
    const f32 noisePow = _compute_noise_Power();
    f32 snr = (mMeanPowerOvrAll - noisePow) / noisePow;
    if (snr <= 0.0f) snr = 0.1f;
    mLcdData.CurOfdmSymbolNo = iCurOfdmSymbIdx + 1; // as "idx" goes from 0...(L-1)
    mLcdData.ModQuality = 10.0f * std::log10(F_M_PI_4 * F_M_PI_4 * cK / stdDevSqOvrAll);
    //mLcdData.PhaseCorr = -conv_rad_to_deg(iPhaseCorr);
    mLcdData.MeanSigmaSqFreqCorr = sqrt(mMeanSigmaSqFreqCorr);
    mLcdData.SNR = 10.0f * std::log10(snr);
    mLcdData.TestData1 = mMeanValue;
    mLcdData.TestData2 = iPhaseCorr / F_2_M_PI * (f32)cCarrDiff;

    emit signal_show_lcd_data(&mLcdData);

    mShowCntStatistics = 0;
    mNextShownOfdmSymbIdx = (mNextShownOfdmSymbIdx + 1) % cL;
    if (mNextShownOfdmSymbIdx == 0) mNextShownOfdmSymbIdx = 1; // as iCurSymbolNo can never be zero here
  }

  memcpy(mPhaseReference.data(), iV.data(), cTu * sizeof(cf32));
}

f32 OfdmDecoder::_compute_noise_Power() const
{
  f32 sumNoise = 0.0f;
  for (i32 idx = -cK / 2; idx < cK / 2; ++idx)
  {
    // Consider FFT shift and skipping DC (0 Hz) bin.
    const i32 fftIdx = fft_shift_skip_dc(idx, cTu);
    sumNoise += mMeanNullPowerWithoutTII[fftIdx]; // the component already squared
  }
  return sumNoise / (f32)cK;
}

void OfdmDecoder::_eval_null_symbol_statistics(const TArrayTu & iV)
{
  f32 max = -1e38f;
  f32 min =  1e38f;

  if (mCarrierPlotType == ECarrierPlotType::NULL_TII_LOG)
  {
    constexpr f32 ALPHA = 0.20f;
    for (i32 idx = -cK / 2; idx < cK / 2; ++idx)
    {
      // Consider FFT shift and skipping DC (0 Hz) bin.
      const i32 fftIdx = fft_shift_skip_dc(idx, cTu);
      const f32 level = log10_times_20(std::abs(iV[fftIdx]));
      f32 & meanNullLevelRef = mMeanNullLevel[fftIdx];
      mean_filter(meanNullLevelRef, level, ALPHA);

      if (meanNullLevelRef < min) min = meanNullLevelRef;
    }
    mean_filter(mAbsNullLevelMin, min, ALPHA);
    mAbsNullLevelGain = 1;
  }
  else
  {
    constexpr f32 ALPHA = 0.20f;
    for (i32 idx = -cK / 2; idx < cK / 2; ++idx)
    {
      // Consider FFT shift and skipping DC (0 Hz) bin.
      const i32 fftIdx = fft_shift_skip_dc(idx, cTu);
      const f32 level = std::abs(iV[fftIdx]);
      f32 & meanNullLevelRef = mMeanNullLevel[fftIdx];
      mean_filter(meanNullLevelRef, level, ALPHA);

      if (meanNullLevelRef < min) min = meanNullLevelRef;
      if (meanNullLevelRef > max) max = meanNullLevelRef;
    }
    assert(max > min);
    mAbsNullLevelMin = min;
    mAbsNullLevelGain = 100.0f / (max - min);
  }
}

void OfdmDecoder::_reset_null_symbol_statistics()
{
  std::fill(mMeanNullLevel.begin(), mMeanNullLevel.end(), 0.0f);
  mAbsNullLevelMin = 0.0f;
  mAbsNullLevelGain = 0.0f;
}

void OfdmDecoder::set_select_carrier_plot_type(ECarrierPlotType iPlotType)
{
  if (iPlotType == ECarrierPlotType::NULL_TII_LIN ||
      iPlotType == ECarrierPlotType::NULL_TII_LOG ||
      iPlotType == ECarrierPlotType::NULL_NO_TII)
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

cf32 OfdmDecoder::_interpolate_2d_plane(const cf32 & iStart, const cf32 & iEnd, f32 iPar)
{
  assert(iPar >= 0.0f && iPar <= 1.0f);
  const f32 a_r = real(iStart);
  const f32 a_i = imag(iStart);
  const f32 b_r = real(iEnd);
  const f32 b_i = imag(iEnd);
  const f32 c_r = (b_r - a_r) * iPar + a_r;
  const f32 c_i = (b_i - a_i) * iPar + a_i;
  return { c_r, c_i };
}
