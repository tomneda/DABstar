/*
 * Copyright (c) 2025 by Thomas Neder (https://github.com/tomneda)
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
#include "ofdm-decoder-simd.h"
#include "phasetable.h"
#include "radio.h"
#include "simd_extensions.h"
#include "ofdm-decoder.h"
#include <vector>
#include <volk/volk.h>

// shortcut syntax to get better overview
#define LOOP_OVER_K   for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)


OfdmDecoder::OfdmDecoder(RadioInterface * ipMr, uint8_t iDabMode, RingBuffer<cmplx> * ipIqBuffer, RingBuffer<float> * ipCarrBuffer)
  : mpRadioInterface(ipMr)
  , mFreqInterleaver(iDabMode)
  , mpIqBuffer(ipIqBuffer)
  , mpCarrBuffer(ipCarrBuffer)
{
  mIqVector.resize(cK);
  mCarrVector.resize(cK);

  qInfo() << "Use VOLK machine:" << volk_get_machine();

  for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)
  {
    // We do not interchange the positive/negative frequencies to their right positions, the de-interleaving understands this.
    int16_t fftIdx = mFreqInterleaver.map_k_to_fft_bin(nomCarrIdx);
    int16_t realCarrRelIdx = fftIdx;

    if (fftIdx < 0)
    {
      realCarrRelIdx += cK / 2;
      fftIdx += cTu;
    }
    else // > 0 (there is no 0)
    {
      realCarrRelIdx += cK / 2 - 1;
    }
    assert(realCarrRelIdx >= 0);
    assert(realCarrRelIdx < cK);
    assert(fftIdx >= 0);
    assert(fftIdx < cTu);
    mMapNomToRealCarrIdx[nomCarrIdx] = realCarrRelIdx;
    mMapNomToFftIdx[nomCarrIdx] = fftIdx;
  }

  // create phase -> cmplx LUT
  for (int32_t phaseIdx = -cLutLen2; phaseIdx <= cLutLen2; ++phaseIdx)
  {
    const float phase = (float)phaseIdx / cLutFact;
    mLutPhase2Cmplx[phaseIdx + cLutLen2] = cmplx(std::cos(phase), std::sin(phase));
  }

  connect(this, &OfdmDecoder::signal_slot_show_iq, mpRadioInterface, &RadioInterface::slot_show_iq);
  connect(this, &OfdmDecoder::signal_show_lcd_data, mpRadioInterface, &RadioInterface::slot_show_lcd_data);

  reset();
}

void OfdmDecoder::reset()
{
  mSimdVecPhaseReference.fill_zeros();
  mSimdVecNomCarrier.fill_zeros();
  mSimdVecMeanNullPowerWithoutTII.fill_zeros();
  mSimdVecStdDevSqPhaseVec.fill_zeros();
  mSimdVecMeanLevel.fill_zeros();
  mSimdVecMeanPower.fill_zeros();
  mSimdVecMeanSigmaSq.fill_zeros();
  mSimdVecIntegAbsPhase.fill_zeros();

  mMeanPowerOvrAll = 1.0f;

  _reset_null_symbol_statistics();
}

void OfdmDecoder::store_null_symbol_with_tii(const std::vector<cmplx> & iFftBuffer) // with TII information
{
  if (mCarrierPlotType != ECarrierPlotType::NULL_TII_LIN &&
      mCarrierPlotType != ECarrierPlotType::NULL_TII_LOG)
  {
    return;
  }

  _eval_null_symbol_statistics(iFftBuffer);
}

void OfdmDecoder::store_null_symbol_without_tii(const std::vector<cmplx> & iFftBuffer) // with TII information
{
  if (mCarrierPlotType == ECarrierPlotType::NULL_NO_TII)
  {
    _eval_null_symbol_statistics(iFftBuffer);
  }

  constexpr float ALPHA = 0.1f;

  for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)
  {
    const int16_t fftIdx = mMapNomToFftIdx[nomCarrIdx];
    const float level = std::abs(iFftBuffer[fftIdx]);
    mean_filter(mSimdVecMeanNullPowerWithoutTII[nomCarrIdx], level * level, ALPHA);
  }
}

void OfdmDecoder::store_reference_symbol_0(const std::vector<cmplx> & iFftBuffer)
{
  // We are now in the frequency domain, and we keep the carriers as coming from the FFT as phase reference.
  mDcFft = iFftBuffer[0];

  for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)
  {
    const int16_t fftIdx = mMapNomToFftIdx[nomCarrIdx];
    mSimdVecPhaseReference[nomCarrIdx] = iFftBuffer[fftIdx];
  }
}

void OfdmDecoder::decode_symbol(const std::vector<cmplx> & iFftBuffer, const uint16_t iCurOfdmSymbIdx, const float iPhaseCorr, std::vector<int16_t> & oBits)
{
  // current runtime: avr: 66us, min: 26us (with no VOLK: 240us)
  // mTimeMeas.trigger_begin();

  mDcFftLast = mDcFft;
  mDcFft = iFftBuffer[0];

  // do frequency de-interleaving and transform FFT vector to contiguous field
  LOOP_OVER_K
  {
    const int16_t fftIdx = mMapNomToFftIdx[nomCarrIdx];
    mSimdVecNomCarrier[nomCarrIdx] = iFftBuffer[fftIdx];
  }

  // -------------------------------
  mSimdVecPhaseReferenceNormed.set_normalize_each_element(mSimdVecPhaseReference);
  mSimdVecFftBinRaw.set_multiply_conj_each_element(mSimdVecNomCarrier, mSimdVecPhaseReferenceNormed);  // PI/4-DQPSK demodulation

  // -------------------------------
  // mVolkFftBinPhaseCorrVec = mVolkFftBinRawVec * cmplx_from_phase(-mVolkIntegAbsPhaseVector);
  LOOP_OVER_K
  {
    const int32_t phaseIdx = (int32_t)std::lround(mSimdVecIntegAbsPhase[nomCarrIdx] * cLutFact);
    assert(phaseIdx >= -cLutLen2 && phaseIdx <= cLutLen2 );  // the limitation is done below already
    const cmplx phase = mLutPhase2Cmplx[cLutLen2 - phaseIdx]; // the phase is used inverse here
    mSimdVecFftBinPhaseCorr[nomCarrIdx] = mSimdVecFftBinRaw[nomCarrIdx] * phase;
  }

  // -------------------------------
  mSimdVecFftBinPhaseCorrArg.set_arg_each_element(mSimdVecFftBinPhaseCorr);

  // -------------------------------
  // in best case the values in mVolkFftBinAbsPhaseCorrVec should only rest onto the positive real axis
  mSimdVecFftBinWrapped.set_wrap_4QPSK_to_phase_zero_each_element(mSimdVecFftBinPhaseCorrArg);

  constexpr float ALPHA = 0.005f;

  // -------------------------------
  // mVolkIntegAbsPhaseVector = limit(mVolkIntegAbsPhaseVector + mVolkFftBinAbsPhaseCorr * 0.2f * ALPHA)
  // Integrate phase error to perform the phase correction in the next OFDM symbol.
  mSimdVecTemp1Float.set_multiply_vector_and_scalar_each_element(mSimdVecFftBinWrapped, 0.2f * ALPHA); // weighting
  mSimdVecIntegAbsPhase.modify_accumulate_each_element(mSimdVecTemp1Float); // integrator
  mSimdVecIntegAbsPhase.modify_limit_symmetrically_each_element(F_RAD_PER_DEG * cPhaseShiftLimit); // this is important to not overdrive mLutPhase2Cmplx!

  // -------------------------------
  mSimdVecTemp1Float.set_square_each_element(mSimdVecFftBinWrapped); // variance
  mSimdVecStdDevSqPhaseVec.modify_mean_filter_each_element(mSimdVecTemp1Float, ALPHA);

  // -------------------------------
  mSimdVecFftBinPower.set_squared_magnitude_each_element(mSimdVecFftBinPhaseCorr);
  mSimdVecFftBinLevel.set_sqrt_each_element(mSimdVecFftBinPower);

  // -------------------------------
  mSimdVecMeanLevel.modify_mean_filter_each_element(mSimdVecFftBinLevel, ALPHA);
  mSimdVecMeanPower.modify_mean_filter_each_element(mSimdVecFftBinPower, ALPHA);
  mMeanPowerOvrAll = mSimdVecFftBinPower.get_mean_filter_sum_of_elements(mMeanPowerOvrAll, ALPHA);

  // -------------------------------
  mSimdVecFftBinPhaseCorr.store_to_real_and_imag_each_element(mSimdVecFftBinPhaseCorrReal, mSimdVecFftBinPhaseCorrImag);
  // calculate the mean squared distance from the current point in 1st quadrant to the point where it should be
  mSimdVecSigmaSq.set_squared_distance_to_nearest_constellation_point_each_element(mSimdVecFftBinPhaseCorrReal, mSimdVecFftBinPhaseCorrImag, mSimdVecMeanLevel);
  mSimdVecMeanSigmaSq.modify_mean_filter_each_element(mSimdVecSigmaSq, ALPHA);

  // -------------------------------
  // 1/SNR + 2 = mVolkTemp2FloatVec = mVolkMeanNullPowerWithoutTII / (mVolkMeanPowerVector - mVolkMeanNullPowerWithoutTII) + 2
  mSimdVecTemp1Float.set_subtract_each_element(mSimdVecMeanPower, mSimdVecMeanNullPowerWithoutTII);
  if (mSimdVecTemp1Float.modify_check_negative_or_zero_values_and_fallback_each_element(0.1f))
  {
    qDebug() << "WARNING: (mSimdVecMeanPower - mSimdVecMeanNullPowerWithoutTII) <= 0!";
  }
  mSimdVecTemp2Float.set_divide_each_element(mSimdVecMeanNullPowerWithoutTII, mSimdVecTemp1Float);  // T2 = 1/SNR
  mSimdVecTemp2Float.set_add_scalar_each_element(2.0f); // T2 += 2

  // -------------------------------
  // w1 = mSimdVecWeightPerBin = (mVolkMeanLevelVector / mVolkMeanSigmaSqVector) / mVolkTemp2FloatVec (1/SNR + 2)
  mSimdVecWeightPerBin.set_divide_each_element(mSimdVecMeanLevel, mSimdVecMeanSigmaSq);    // w1 = meanLevelPerBinRef / meanSigmaSqPerBinRef;
  mSimdVecWeightPerBin.set_divide_each_element(mSimdVecWeightPerBin, mSimdVecTemp2Float);  // w1 /= T2


  if (mSoftBitType == ESoftBitType::SOFTDEC1)
  {
    // w1 = mSimdVecWeightPerBin *= std::abs(mVolkPhaseReference);
    mSimdVecTemp1Float.set_magnitude_each_element(mSimdVecPhaseReference);
    mSimdVecWeightPerBin.set_multiply_each_element(mSimdVecWeightPerBin, mSimdVecTemp1Float);
  }

  mSimdVecDecodingReal.set_multiply_each_element(mSimdVecFftBinPhaseCorrReal, mSimdVecWeightPerBin);
  mSimdVecDecodingImag.set_multiply_each_element(mSimdVecFftBinPhaseCorrImag, mSimdVecWeightPerBin);

  // use mMeanValue from last round to be conform with the non-SIMD variant
  const float w2 = (mSoftBitType == ESoftBitType::SOFTDEC1 ? -140 / mMeanValue : -100 / mMeanValue);

  // -------------------------------
  mSimdVecTemp1Float.set_squared_magnitude_of_elements(mSimdVecDecodingReal, mSimdVecDecodingImag);
  mSimdVecTemp1Float.set_sqrt_each_element(mSimdVecTemp1Float);
  mMeanValue = mSimdVecTemp1Float.get_sum_of_elements() / cK;

  // -------------------------------
  // apply weight w2 to be conform to the viterbi input range
  // const float w2 = (mSoftBitType == ESoftBitType::SOFTDEC1 ? -140 / mMeanValue : -100 / mMeanValue);
  mSimdVecDecodingReal.modify_multiply_scalar_each_element(w2);
  mSimdVecDecodingImag.modify_multiply_scalar_each_element(w2);
  // -------------------------------

  // extract coding bits
  LOOP_OVER_K
  {
    oBits[0  + nomCarrIdx] = (int16_t)(mSimdVecDecodingReal[nomCarrIdx]);
    oBits[cK + nomCarrIdx] = (int16_t)(mSimdVecDecodingImag[nomCarrIdx]);
  }

  // mTimeMeas.trigger_end();
  // if (iCurOfdmSymbIdx == 1) mTimeMeas.print_time_per_round();

  // part for displaying IQ scope and carrier bscope
  ++mShowCntIqScope;
  ++mShowCntStatistics;
  const bool showScopeData = (mShowCntIqScope > cL && iCurOfdmSymbIdx == mNextShownOfdmSymbIdx);
  const bool showStatisticData = (mShowCntStatistics > 5 * cL && iCurOfdmSymbIdx == mNextShownOfdmSymbIdx);

  if (showScopeData)
  {
    _display_iq_and_carr_vectors();

    // From time to time we show the constellation of the current symbol
    mpIqBuffer->put_data_into_ring_buffer(mIqVector.data(), cK);
    mpCarrBuffer->put_data_into_ring_buffer(mCarrVector.data(), cK);
    emit signal_slot_show_iq(cK, 1.0f /*mGain / TOP_VAL*/);
    mShowCntIqScope = 0;
  }

  if (showStatisticData)
  {
    const float noisePow = _compute_noise_Power();
    float snr = (mMeanPowerOvrAll - noisePow) / noisePow;
    if (snr <= 0.0f) snr = 0.1f;
    mLcdData.CurOfdmSymbolNo = iCurOfdmSymbIdx + 1; // as "idx" goes from 0...(L-1)
    mLcdData.ModQuality = 10.0f * std::log10(F_M_PI_4 * F_M_PI_4 * cK / mSimdVecStdDevSqPhaseVec.get_sum_of_elements());
    mLcdData.PhaseCorr = -conv_rad_to_deg(iPhaseCorr);
    mLcdData.SNR = 10.0f * std::log10(snr);
    mLcdData.TestData1 = _compute_frequency_offset(mSimdVecNomCarrier, mSimdVecPhaseReference);
    mLcdData.TestData2 = iPhaseCorr / F_2_M_PI * (float)cK;

    emit signal_show_lcd_data(&mLcdData);

    mShowCntStatistics = 0;
    mNextShownOfdmSymbIdx = (mNextShownOfdmSymbIdx + 1) % cL;
    if (mNextShownOfdmSymbIdx == 0) mNextShownOfdmSymbIdx = 1; // as iCurSymbolNo can never be zero here
  }

  // copy current FFT values as next OFDM reference
  memcpy(mSimdVecPhaseReference, mSimdVecNomCarrier, cK * sizeof(cmplx));
}

float OfdmDecoder::_compute_frequency_offset(const cmplx * const & r, const cmplx * const & v) const
{
  // TODO: make this correct!
#if 0
  cmplx theta = cmplx(0, 0);

  for (int idx = -mDabPar.K / 2; idx < mDabPar.K / 2; idx += 1)
  {
    const int32_t index = fft_shift_skip_dc(idx, mDabPar.T_u); // this was with DC before in QT-DAB
    cmplx val = r[index] * conj(c[index]);
    val = turn_complex_phase_to_first_quadrant(val);
    theta += val;
  }
  return (arg(theta) - F_M_PI_4) / F_2_M_PI * (float)mDabPar.T_u / (float)mDabPar.T_s * (float)mDabPar.CarrDiff;
  // return (arg(theta) - F_M_PI_4) / F_2_M_PI * (float)mDabPar.CarrDiff;
#endif
  return 0;
}

float OfdmDecoder::_compute_noise_Power() const
{
  volk_32f_accumulator_s32f_a(mSimdVecTemp1Float, mSimdVecMeanNullPowerWithoutTII, cK); // this sums all vector elements to the first element
  return mSimdVecTemp1Float[0] / (float)cK;
}

void OfdmDecoder::_eval_null_symbol_statistics(const std::vector<cmplx> & iFftBuffer)
{
  float max = -1e38f;
  float min =  1e38f;
  if (mCarrierPlotType == ECarrierPlotType::NULL_TII_LOG)
  {
    constexpr float ALPHA = 0.20f;
    for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)
    {
      const int16_t fftIdx = mMapNomToFftIdx[nomCarrIdx];
      const float level = log10_times_20(std::abs(iFftBuffer[fftIdx]));
      float & meanNullLevelRef = mSimdVecMeanNullLevel[nomCarrIdx];
      mean_filter(meanNullLevelRef, level, ALPHA);

      if (meanNullLevelRef < min) min = meanNullLevelRef;
    }
    mean_filter(mAbsNullLevelMin, min, ALPHA);
    mAbsNullLevelGain = 1;
  }
  else
  {
    constexpr float ALPHA = 0.20f;
    for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)
    {
      const int16_t fftIdx = mMapNomToFftIdx[nomCarrIdx];
      const float level = std::abs(iFftBuffer[fftIdx]);
      float & meanNullLevelRef = mSimdVecMeanNullLevel[nomCarrIdx];
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
  mSimdVecMeanNullLevel.fill_zeros();
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

void OfdmDecoder::_display_iq_and_carr_vectors()
{
  for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)
  {
    switch (mIqPlotType)
    {
    case EIqPlotType::PHASE_CORR_CARR_NORMED: mIqVector[nomCarrIdx] = mSimdVecFftBinPhaseCorr[nomCarrIdx] / std::sqrt(mSimdVecMeanPower[nomCarrIdx]); break;
    case EIqPlotType::PHASE_CORR_MEAN_NORMED: mIqVector[nomCarrIdx] = mSimdVecFftBinPhaseCorr[nomCarrIdx] / std::sqrt(mMeanPowerOvrAll); break;
    case EIqPlotType::RAW_MEAN_NORMED:        mIqVector[nomCarrIdx] = mSimdVecFftBinRaw[nomCarrIdx] / std::sqrt(mMeanPowerOvrAll); break;
    case EIqPlotType::DC_OFFSET_FFT_10:       mIqVector[nomCarrIdx] =  10.0f / (float)cTu * _interpolate_2d_plane(mDcFftLast, mDcFft, (float)nomCarrIdx / ((float)mIqVector.size() - 1)); break;
    case EIqPlotType::DC_OFFSET_FFT_100:      mIqVector[nomCarrIdx] = 100.0f / (float)cTu * _interpolate_2d_plane(mDcFftLast, mDcFft, (float)nomCarrIdx / ((float)mIqVector.size() - 1)); break;
    case EIqPlotType::DC_OFFSET_ADC_10:       mIqVector[nomCarrIdx] =  10.0f * mDcAdc; break;
    case EIqPlotType::DC_OFFSET_ADC_100:      mIqVector[nomCarrIdx] = 100.0f * mDcAdc; break;
    }

    const int16_t realCarrRelIdx = mMapNomToRealCarrIdx[nomCarrIdx];
    const int16_t dataVecCarrIdx = (mShowNomCarrier ? nomCarrIdx : realCarrRelIdx);

    switch (mCarrierPlotType)
    {
    case ECarrierPlotType::SB_WEIGHT:
    {
      // Convert and limit the soft bit weight to percent
      // float weight2 = (std::abs(real(mVolk_r1[nomCarrIdx])) + std::abs(imag(mVolk_r1[nomCarrIdx]))) / 2.0f * (-iWeight);
      float val = (std::abs(mSimdVecDecodingReal[nomCarrIdx]) + std::abs(mSimdVecDecodingImag[nomCarrIdx])) / 2.0f;
      if (val > F_VITERBI_SOFT_BIT_VALUE_MAX) val = F_VITERBI_SOFT_BIT_VALUE_MAX; // limit graphics like the Viterbi itself does
      mCarrVector[dataVecCarrIdx] = 100.0f / VITERBI_SOFT_BIT_VALUE_MAX * val;  // show it in percent of the maximum Viterbi input
      break;
    }
    case ECarrierPlotType::EVM_PER:         mCarrVector[dataVecCarrIdx] = 100.0f * std::sqrt(mSimdVecMeanSigmaSq[nomCarrIdx]) / mSimdVecMeanLevel[nomCarrIdx]; break;
    case ECarrierPlotType::EVM_DB:          mCarrVector[dataVecCarrIdx] = 20.0f * std::log10(std::sqrt(mSimdVecMeanSigmaSq[nomCarrIdx]) / mSimdVecMeanLevel[nomCarrIdx]); break;
    case ECarrierPlotType::STD_DEV:         mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(std::sqrt(mSimdVecStdDevSqPhaseVec[nomCarrIdx])); break;
    case ECarrierPlotType::PHASE_ERROR:     mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(mSimdVecIntegAbsPhase[nomCarrIdx]); break;
    case ECarrierPlotType::FOUR_QUAD_PHASE: mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(std::arg(mSimdVecFftBinPhaseCorr[nomCarrIdx])); break;
    case ECarrierPlotType::REL_POWER:       mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(mSimdVecMeanPower[nomCarrIdx] / mMeanPowerOvrAll); break;
    case ECarrierPlotType::SNR:             mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(mSimdVecMeanPower[nomCarrIdx] / mSimdVecMeanNullPowerWithoutTII[nomCarrIdx]); break;
    case ECarrierPlotType::NULL_TII_LIN:
    case ECarrierPlotType::NULL_TII_LOG:
    case ECarrierPlotType::NULL_NO_TII:     mCarrVector[dataVecCarrIdx] = mAbsNullLevelGain * (mSimdVecMeanNullLevel[nomCarrIdx] - mAbsNullLevelMin); break;
    case ECarrierPlotType::NULL_OVR_POW:    mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(mSimdVecMeanNullPowerWithoutTII[nomCarrIdx] / mMeanPowerOvrAll); break;
    }
  } // for (nomCarrIdx...
}
