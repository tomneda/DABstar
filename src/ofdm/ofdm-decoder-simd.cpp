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
#include "phasetable.h"
#include "dabradio.h"
#include "simd_extensions.h"
#include "ofdm-decoder-simd.h"
#include <vector>
#include <volk/volk.h>

// shortcut syntax to get better overview
#define LOOP_OVER_K   for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)


OfdmDecoder::OfdmDecoder(DabRadio * ipMr, RingBuffer<cmplx> * ipIqBuffer, RingBuffer<float> * ipCarrBuffer)
  : mpRadioInterface(ipMr)
  , mFreqInterleaver()
  , mpIqBuffer(ipIqBuffer)
  , mpCarrBuffer(ipCarrBuffer)
{
  mIqVector.resize(cK);
  mCarrVector.resize(cK);

  qInfo() << "Using VOLK machine:" << volk_get_machine();

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

  connect(this, &OfdmDecoder::signal_slot_show_iq, mpRadioInterface, &DabRadio::slot_show_iq);
  connect(this, &OfdmDecoder::signal_show_lcd_data, mpRadioInterface, &DabRadio::slot_show_lcd_data);

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

void OfdmDecoder::store_null_symbol_with_tii(const TArrayTu & iFftBuffer)
{
  if (mCarrierPlotType != ECarrierPlotType::NULL_TII_LIN &&
      mCarrierPlotType != ECarrierPlotType::NULL_TII_LOG)
  {
    return;
  }

  _eval_null_symbol_statistics(iFftBuffer);
}

void OfdmDecoder::store_null_symbol_without_tii(const TArrayTu & iFftBuffer)
{
  if (mCarrierPlotType == ECarrierPlotType::NULL_NO_TII)
  {
    _eval_null_symbol_statistics(iFftBuffer);
  }

  for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)
  {
    const int16_t fftIdx = mMapNomToFftIdx[nomCarrIdx];
    mSimdVecTemp1Float[nomCarrIdx] = std::abs(iFftBuffer[fftIdx]); // level
  }

  constexpr float cAlpha = 0.1f;
  mSimdVecTemp1Float.set_square_each_element(mSimdVecTemp1Float); // noise power
  mSimdVecMeanNullPowerWithoutTII.modify_mean_filter_each_element(mSimdVecTemp1Float, cAlpha);
}

void OfdmDecoder::store_reference_symbol_0(const TArrayTu & iFftBuffer)
{
  // We are now in the frequency domain, and we keep the carriers as coming from the FFT as phase reference.
  mDcFft = iFftBuffer[0];

  for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)
  {
    const int16_t fftIdx = mMapNomToFftIdx[nomCarrIdx];
    mSimdVecPhaseReference[nomCarrIdx] = iFftBuffer[fftIdx];
  }
}

void OfdmDecoder::decode_symbol(const TArrayTu & iFftBuffer, const uint16_t iCurOfdmSymbIdx, const float iPhaseCorr, std::vector<int16_t> & oBits)
{
  // current runtime on i7-6700K: avr: 57us, min: 19us
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
  mSimdVecFftBinPhaseCorr.set_back_rotate_phase_each_element(mSimdVecFftBinRaw, mSimdVecIntegAbsPhase);

  // -------------------------------
  mSimdVecFftBinPhaseCorrArg.set_arg_each_element(mSimdVecFftBinPhaseCorr);

  // -------------------------------
  // in best case the values in mVolkFftBinAbsPhaseCorrVec should only rest onto the positive real axis
  mSimdVecFftBinWrapped.set_wrap_4QPSK_to_phase_zero_each_element(mSimdVecFftBinPhaseCorrArg);

  constexpr float cAlpha = 0.005f;

  // -------------------------------
  // Integrate phase error to perform the phase correction in the next OFDM symbol.
  mSimdVecTemp1Float.set_multiply_vector_and_scalar_each_element(mSimdVecFftBinWrapped, 0.2f * cAlpha); // weighting
  mSimdVecIntegAbsPhase.modify_accumulate_each_element(mSimdVecTemp1Float); // integrator
  mSimdVecIntegAbsPhase.modify_limit_symmetrically_each_element(F_RAD_PER_DEG * cPhaseShiftLimit); // this is important to not overdrive mLutPhase2Cmplx!

  // -------------------------------
  mSimdVecTemp1Float.set_square_each_element(mSimdVecFftBinWrapped); // variance
  mSimdVecStdDevSqPhaseVec.modify_mean_filter_each_element(mSimdVecTemp1Float, cAlpha);

  // -------------------------------
  mSimdVecFftBinPower.set_squared_magnitude_each_element(mSimdVecFftBinPhaseCorr);
  mSimdVecFftBinLevel.set_sqrt_each_element(mSimdVecFftBinPower);

  // -------------------------------
  mSimdVecMeanLevel.modify_mean_filter_each_element(mSimdVecFftBinLevel, cAlpha);
  mSimdVecMeanPower.modify_mean_filter_each_element(mSimdVecFftBinPower, cAlpha);
  mMeanPowerOvrAll = mSimdVecFftBinPower.get_mean_filter_sum_of_elements(mMeanPowerOvrAll, cAlpha);

  // -------------------------------
  mSimdVecFftBinPhaseCorr.store_to_real_and_imag_each_element(mSimdVecFftBinPhaseCorrReal, mSimdVecFftBinPhaseCorrImag);
  // calculate the mean squared distance from the current point in 1st quadrant to the point where it should be
  mSimdVecSigmaSq.set_squared_distance_to_nearest_constellation_point_each_element(mSimdVecFftBinPhaseCorrReal, mSimdVecFftBinPhaseCorrImag, mSimdVecMeanLevel);
  mSimdVecMeanSigmaSq.modify_mean_filter_each_element(mSimdVecSigmaSq, cAlpha);

  // -------------------------------
  mSimdVecMeanNettoPower.set_subtract_each_element(mSimdVecMeanPower, mSimdVecMeanNullPowerWithoutTII);
  mSimdVecMeanNettoPower.modify_check_negative_or_zero_values_and_fallback_each_element(0.1f);
  mSimdVecTemp2Float.set_divide_each_element(mSimdVecMeanNullPowerWithoutTII, mSimdVecMeanNettoPower);  // T2 = 1/SNR
  mSimdVecTemp2Float.modify_add_scalar_each_element(mSoftBitType == ESoftBitType::SOFTDEC1 ? 1.0f : 2.0f); // T2 += (1 or 2)

  // -------------------------------
  mSimdVecWeightPerBin.set_divide_each_element(mSimdVecMeanLevel, mSimdVecMeanSigmaSq);    // w1 = meanLevelPerBinRef / meanSigmaSqPerBinRef;
  mSimdVecWeightPerBin.set_divide_each_element(mSimdVecWeightPerBin, mSimdVecTemp2Float);  // w1 /= T2

  // -------------------------------
  if (mSoftBitType == ESoftBitType::SOFTDEC3)
  {
    mSimdVecTemp1Float.set_magnitude_each_element(mSimdVecPhaseReference);  // T1 = abs(phaseRef)
    mSimdVecWeightPerBin.modify_multiply_each_element(mSimdVecTemp1Float);  // w1 *= T1
    mSimdVecDecodingReal.set_multiply_each_element(mSimdVecFftBinPhaseCorrReal, mSimdVecWeightPerBin);  // real of r1 = fftBin * w1
    mSimdVecDecodingImag.set_multiply_each_element(mSimdVecFftBinPhaseCorrImag, mSimdVecWeightPerBin);  // imag of r1 = fftBin * w1
  }
  else // SOFTDEC1 and SOFTDEC2
  {
    mSimdVecTemp1Float.set_magnitude_each_element(mSimdVecPhaseReference); // T1 = abs(phaseRef)
    mSimdVecTemp1Float.modify_multiply_each_element(mSimdVecFftBinLevel);  // T1 *= abs(fftBin)
    mSimdVecTemp1Float.modify_sqrt_each_element();                         // T1 = sqrt(T1)
    mSimdVecWeightPerBin.modify_multiply_each_element(mSimdVecTemp1Float); // w1 *= T1
    mSimdVecTempCmplx.set_normalize_each_element(mSimdVecFftBinPhaseCorr); // T = normalize(fftBin)
    mSimdVecTempCmplx.store_to_real_and_imag_each_element(mSimdVecDecodingReal, mSimdVecDecodingImag);
    mSimdVecDecodingReal.modify_multiply_each_element(mSimdVecWeightPerBin);  // real *= w1
    mSimdVecDecodingImag.modify_multiply_each_element(mSimdVecWeightPerBin);  // imag *= w1
  }

  // use mMeanValue from last round to be conform with the non-SIMD variant
  // const float w2 = (mSoftBitType == ESoftBitType::SOFTDEC3 ? -140.0f : -100.0f) / mMeanValue;

  // -------------------------------
  mSimdVecTemp1Float.set_squared_magnitude_of_elements(mSimdVecDecodingReal, mSimdVecDecodingImag);
  mSimdVecTemp1Float.set_sqrt_each_element(mSimdVecTemp1Float);
  mMeanValue = mSimdVecTemp1Float.get_sum_of_elements() / cK;

  // -------------------------------
  // apply weight w2 to be conform to the viterbi input range
  const float w2 = (mSoftBitType == ESoftBitType::SOFTDEC3 ? -140.0f : -100.0f) / mMeanValue;
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

  // displaying IQ scope and carrier scope
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
    //mLcdData.TestData1 = _compute_frequency_offset(mSimdVecNomCarrier, mSimdVecPhaseReference);
    mLcdData.TestData1 = mMeanValue;
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

  for (int idx = -cK / 2; idx < cK / 2; idx += 1)
  {
    const int32_t index = fft_shift_skip_dc(idx, cTu); // this was with DC before in QT-DAB
    cmplx val = r[index] * conj(c[index]);
    val = turn_complex_phase_to_first_quadrant(val);
    theta += val;
  }
  return (arg(theta) - F_M_PI_4) / F_2_M_PI * (float)cTu / (float)cTs * (float)cCarrDiff;
  // return (arg(theta) - F_M_PI_4) / F_2_M_PI * (float)cCarrDiff;
#endif
  return 0;
}

float OfdmDecoder::_compute_noise_Power() const
{
  return mSimdVecMeanNullPowerWithoutTII.get_sum_of_elements() / (float)cK;
}

void OfdmDecoder::_eval_null_symbol_statistics(const TArrayTu & iFftBuffer)
{
  float max = -1e38f;
  float min =  1e38f;
  if (mCarrierPlotType == ECarrierPlotType::NULL_TII_LOG)
  {
    constexpr float cAlpha = 0.20f;
    for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)
    {
      const int16_t fftIdx = mMapNomToFftIdx[nomCarrIdx];
      const float level = log10_times_20(std::abs(iFftBuffer[fftIdx]));
      float & meanNullLevelRef = mSimdVecMeanNullLevel[nomCarrIdx];
      mean_filter(meanNullLevelRef, level, cAlpha);

      if (meanNullLevelRef < min) min = meanNullLevelRef;
    }
    mean_filter(mAbsNullLevelMin, min, cAlpha);
    mAbsNullLevelGain = 1;
  }
  else
  {
    constexpr float cAlpha = 0.20f;
    for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)
    {
      const int16_t fftIdx = mMapNomToFftIdx[nomCarrIdx];
      const float level = std::abs(iFftBuffer[fftIdx]);
      float & meanNullLevelRef = mSimdVecMeanNullLevel[nomCarrIdx];
      mean_filter(meanNullLevelRef, level, cAlpha);

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
