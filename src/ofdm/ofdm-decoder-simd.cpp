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
// #define VOLK_MALLOC(type_, size_)    (type_ *)volk_malloc((size_) * sizeof(type_), mVolkAlignment)
#define LOOP_OVER_K                  for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)


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
    mVolkMapNomToRealCarrIdx[nomCarrIdx] = realCarrRelIdx;
    mVolkMapNomToFftIdx[nomCarrIdx] = fftIdx;
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
  mVolkPhaseReference.fill_zeros();
  mVolkNomCarrierVec.fill_zeros();
  mVolkMeanNullPowerWithoutTII.fill_zeros();
  mVolkStdDevSqPhaseVector.fill_zeros();
  mVolkMeanLevelVector.fill_zeros();
  mVolkMeanPowerVector.fill_zeros();
  mVolkMeanSigmaSqVector.fill_zeros();
  mVolkIntegAbsPhaseVector.fill_zeros();

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
    const int16_t fftIdx = mVolkMapNomToFftIdx[nomCarrIdx];
    const float level = std::abs(iFftBuffer[fftIdx]);
    mean_filter(mVolkMeanNullPowerWithoutTII[nomCarrIdx], level * level, ALPHA);
  }
}

void OfdmDecoder::store_reference_symbol_0(const std::vector<cmplx> & iFftBuffer)
{
  // We are now in the frequency domain, and we keep the carriers as coming from the FFT as phase reference.
  mDcFft = iFftBuffer[0];

  for (int16_t nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)
  {
    const int16_t fftIdx = mVolkMapNomToFftIdx[nomCarrIdx];
    mVolkPhaseReference[nomCarrIdx] = iFftBuffer[fftIdx];
  }
}

void OfdmDecoder::decode_symbol(const std::vector<cmplx> & iFftBuffer, const uint16_t iCurOfdmSymbIdx, const float iPhaseCorr, std::vector<int16_t> & oBits)
{
  // current runtime: 75us (with no VOLK: 240us)
  // mTimeMeas.trigger_begin();
  // we have 1536 carriers, this is 2^10 + 2^9 (1024 + 512), if there is a need to split it to 2^n
  // assert(volk_is_aligned(mVolkPhaseReference)); // example how to test correct alignment

  mDcFftLast = mDcFft;
  mDcFft = iFftBuffer[0];

  // do frequency de-interleaving and transform FFT vector to contiguous field
  LOOP_OVER_K
  {
    const int16_t fftIdx = mVolkMapNomToFftIdx[nomCarrIdx];
    mVolkNomCarrierVec[nomCarrIdx] = iFftBuffer[fftIdx];
  }

  // -------------------------------
  // mVolkFftBinRawVec = mVolkNomCarrierVec * conj(norm(mVolkPhaseReference))
  simd_normalize(mVolkPhaseReferenceNormedVec, mVolkPhaseReference, cK); // normalize phase reference (only phase information is needed)
  volk_32fc_x2_multiply_conjugate_32fc_a(mVolkFftBinRawVec, mVolkNomCarrierVec, mVolkPhaseReferenceNormedVec, cK);  // PI/4-DQPSK demodulation
  // -------------------------------


  // -------------------------------
  // mVolkFftBinRawVecPhaseCorr = mVolkFftBinRawVec * cmplx_from_phase(-mVolkIntegAbsPhaseVector);
  LOOP_OVER_K
  {
    const int32_t phaseIdx = (int32_t)std::lround(mVolkIntegAbsPhaseVector[nomCarrIdx] * cLutFact);
    assert(phaseIdx >= -cLutLen2 && phaseIdx <= cLutLen2 );  // the limitation is done below already
    const cmplx phase = mLutPhase2Cmplx[cLutLen2 - phaseIdx]; // the phase is used inverse here
    mVolkFftBinRawVecPhaseCorr[nomCarrIdx] = mVolkFftBinRawVec[nomCarrIdx] * phase;
  }
  // -------------------------------


  // -------------------------------
  mVolkFftBinRawVecPhaseCorrAbsSq.make_norm(mVolkFftBinRawVecPhaseCorr);
  mVolkFftBinRawVecPhaseCorrAbs.make_sqrt(mVolkFftBinRawVecPhaseCorrAbsSq);
  // -------------------------------


  // -------------------------------
  // mVolkFftBinRawVecPhaseCorrArg = arg_per_bin(mVolkFftBinRawVecPhaseCorr);
  mVolkFftBinRawVecPhaseCorrArg.make_arg(mVolkFftBinRawVecPhaseCorr);
  // -------------------------------

  // -------------------------------
  // mVolkFftBinAbsPhaseCorr = turn_phase_to_first_quadrant(mVolkFftBinRawVecPhaseCorrArg) - PI/4;
  mVolkFftBinAbsPhaseCorr.wrap_4QPSK_to_phase_zero(mVolkFftBinRawVecPhaseCorrArg);

  constexpr float ALPHA = 0.005f;

  // -------------------------------
  // mVolkIntegAbsPhaseVector = limit(mVolkIntegAbsPhaseVector + mVolkFftBinAbsPhaseCorr * 0.2f * ALPHA)
  // Integrate phase error to perform the phase correction in the next OFDM symbol.
  // volk_32f_s32f_multiply_32f_a(mVolkTemp1FloatVec, mVolkFftBinAbsPhaseCorr, 0.2f * ALPHA, cK); // weighting
  mVolkTemp1FloatVec.multiply_vector_and_scalar(mVolkFftBinAbsPhaseCorr, 0.2f * ALPHA); // weighting
  mVolkIntegAbsPhaseVector.accumulate_vector(mVolkTemp1FloatVec); // integrator
  LOOP_OVER_K limit_symmetrically(mVolkIntegAbsPhaseVector[nomCarrIdx], F_RAD_PER_DEG * cPhaseShiftLimit); // this is important to not overdrive mLutPhase2Cmplx!
  // -------------------------------


  // -------------------------------
  // mVolkStdDevSqPhaseVector = mean_per_bin(mVolkFftBinAbsPhaseCorr^2)
  mVolkTemp1FloatVec.make_square(mVolkFftBinAbsPhaseCorr); // variance
  mVolkStdDevSqPhaseVector.mean_filter(mVolkTemp1FloatVec, ALPHA);
  // _volk_mean_filter(mVolkStdDevSqPhaseVector, mVolkTemp1FloatVec, ALPHA);
  // -------------------------------


  // -------------------------------
  // stdDevSqOvrAll = sum_over_bins(mVolkStdDevSqPhaseVector)
  volk_32f_accumulator_s32f_a(mVolkTemp2FloatVec, mVolkStdDevSqPhaseVector, cK);  // this sums all vector elements to the first element
  const float stdDevSqOvrAll = mVolkTemp2FloatVec[0];
  // -------------------------------


  // -------------------------------
  // mVolkMeanLevelVector = mean_per_bin  (mVolkFftBinRawVecPhaseCorrAbs)
  // mVolkMeanPowerVector = mean_per_bin  (mVolkFftBinRawVecPhaseCorrAbsSq)
  // mMeanPowerOvrAll     = mean_over_bins(mVolkFftBinRawVecPhaseCorrAbsSq)
  mVolkMeanLevelVector.mean_filter(mVolkFftBinRawVecPhaseCorrAbs, ALPHA);
  mVolkMeanPowerVector.mean_filter(mVolkFftBinRawVecPhaseCorrAbsSq, ALPHA);
  _volk_mean_filter_sum(mMeanPowerOvrAll, mVolkFftBinRawVecPhaseCorrAbsSq, ALPHA / (float)cK);
  // -------------------------------


  // -------------------------------
  // calculate the mean squared distance from the current point in 1st quadrant to the point where it should be
  // mVolkMeanSigmaSqVector = mean_per_bin(min_distance(mVolkFftBinRawVecPhaseCorr, (1+1j)/sqrt(2)*mVolkMeanLevelVector)^2)
  volk_32fc_deinterleave_32f_x2_a(mVolkFftBinRawVecPhaseCorrReal, mVolkFftBinRawVecPhaseCorrImag, mVolkFftBinRawVecPhaseCorr, cK);
  simd_abs(mVolkTemp1FloatVec, mVolkFftBinRawVecPhaseCorrReal, cK);
  simd_abs(mVolkTemp2FloatVec, mVolkFftBinRawVecPhaseCorrImag, cK);

  LOOP_OVER_K
  {
    // Collect data for "Log Likelihood Ratio"
    const float meanLevelAtAxisPerBin = mVolkMeanLevelVector[nomCarrIdx] * F_SQRT1_2;
    const cmplx meanLevelAtAxisPerBinCplx = cmplx(meanLevelAtAxisPerBin, meanLevelAtAxisPerBin);
    const cmplx meanLevelAtAxisPerBinAbsCplx = cmplx(mVolkTemp1FloatVec[nomCarrIdx], mVolkTemp2FloatVec[nomCarrIdx]);

    volk_32fc_x2_square_dist_32f_a(mVolkTemp1FloatVec, &meanLevelAtAxisPerBinAbsCplx, &meanLevelAtAxisPerBinCplx, 1);
    const float sigmaSqPerBin = mVolkTemp1FloatVec[0];

    mean_filter(mVolkMeanSigmaSqVector[nomCarrIdx], sigmaSqPerBin, ALPHA);
  }
  // -------------------------------


  // -------------------------------
  // 1/SNR + 2 = mVolkTemp2FloatVec = mVolkMeanNullPowerWithoutTII / (mVolkMeanPowerVector - mVolkMeanNullPowerWithoutTII) + 2
  volk_32f_x2_subtract_32f_a(mVolkTemp1FloatVec, mVolkMeanPowerVector, mVolkMeanNullPowerWithoutTII, cK);
  LOOP_OVER_K if (mVolkTemp1FloatVec[nomCarrIdx] <= 0.0f) mVolkTemp1FloatVec[nomCarrIdx] = 0.1f;
  volk_32f_x2_divide_32f_a(mVolkTemp2FloatVec, mVolkMeanNullPowerWithoutTII, mVolkTemp1FloatVec, cK);
  volk_32f_s32f_add_32f_a(mVolkTemp2FloatVec, mVolkTemp2FloatVec, 2.0f, cK);
  // -------------------------------


  // -------------------------------
  // w1 = mVolkWeightPerBin = (mVolkMeanLevelVector / mVolkMeanSigmaSqVector) / mVolkTemp2FloatVec (1/SNR + 2)
  volk_32f_x2_divide_32f_a(mVolkWeightPerBin, mVolkMeanLevelVector, mVolkMeanSigmaSqVector, cK);  // r1 = meanLevelPerBinRef / meanSigmaSqPerBinRef;
  volk_32f_x2_divide_32f_a(mVolkWeightPerBin, mVolkWeightPerBin, mVolkTemp2FloatVec, cK);  // r1 /= T2
  // -------------------------------


  switch (mSoftBitType)
  {
  case ESoftBitType::SOFTDEC1:
    // w1 = mVolkWeightPerBin *= std::abs(mVolkPhaseReference);
    volk_32fc_magnitude_32f_a(mVolkTemp1FloatVec, mVolkPhaseReference, cK); // TODO: maybe we have this already from the former round?
    volk_32f_x2_multiply_32f_a(mVolkWeightPerBin, mVolkWeightPerBin, mVolkTemp1FloatVec, cK);
    // r1 = mVolkFftBinRawVecPhaseCorrReal * mVolkWeightPerBin
    volk_32f_x2_multiply_32f_a(mVolkViterbiFloatVecReal, mVolkFftBinRawVecPhaseCorrReal, mVolkWeightPerBin, cK);
    volk_32f_x2_multiply_32f_a(mVolkViterbiFloatVecImag, mVolkFftBinRawVecPhaseCorrImag, mVolkWeightPerBin, cK);
    break;
  case ESoftBitType::SOFTDEC2: // log likelihood ratio
    // r1 = mVolkFftBinRawVecPhaseCorrReal * mVolkWeightPerBin
    volk_32f_x2_multiply_32f_a(mVolkViterbiFloatVecReal, mVolkFftBinRawVecPhaseCorrReal, mVolkWeightPerBin, cK);
    volk_32f_x2_multiply_32f_a(mVolkViterbiFloatVecImag, mVolkFftBinRawVecPhaseCorrImag, mVolkWeightPerBin, cK);
    break;
  default: // ESoftBitType::SOFTDEC3
    // w1 *= std::sqrt(std::abs(fftBin) * std::abs(mPhaseReference[fftIdx])); // input level
    // mVolkWeightPerBin *= mVolkFftBinRawVecPhaseCorrAbs;
    // volk_32fc_deinterleave_32f_x2_a(mVolkViterbiFloatVecReal, mVolkViterbiFloatVecImag, mVolkFftBinRawVecPhaseCorr, cK);
    // mVolkPhaseReferenceNormedVec
    ;
  }

  // -------------------------------
  // apply weight w2 to be conform to the viterbi input range
  const float w2 = (mSoftBitType == ESoftBitType::SOFTDEC1 ? -140 / mMeanValue : -100 / mMeanValue);
  volk_32f_s32f_multiply_32f_a(mVolkViterbiFloatVecReal, mVolkViterbiFloatVecReal, w2, cK);
  volk_32f_s32f_multiply_32f_a(mVolkViterbiFloatVecImag, mVolkViterbiFloatVecImag, w2, cK);
  // -------------------------------

  // extract coding bits
  LOOP_OVER_K
  {
    oBits[0  + nomCarrIdx] = (int16_t)(mVolkViterbiFloatVecReal[nomCarrIdx]);
    oBits[cK + nomCarrIdx] = (int16_t)(mVolkViterbiFloatVecImag[nomCarrIdx]);
  }

  // -------------------------------
  // mMeanValue = sum_over_bins() / mDabPar.K;
  volk_32f_x2_multiply_32f_a(mVolkTemp1FloatVec, mVolkFftBinRawVecPhaseCorrAbs, mVolkWeightPerBin, cK);
  volk_32f_accumulator_s32f_a(mVolkTemp2FloatVec, mVolkTemp1FloatVec, cK);  // this sums all vector elements to the first element
  mMeanValue = mVolkTemp2FloatVec[0] /*sum*/ / cK;
  // -------------------------------

  // mTimeMeas.trigger_end();
  // if (iCurOfdmSymbIdx == 1) mTimeMeas.print_time_per_round();

  // part for displaying IQ scope and carrier scope
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
    mLcdData.ModQuality = 10.0f * std::log10(F_M_PI_4 * F_M_PI_4 * cK / stdDevSqOvrAll);
    mLcdData.PhaseCorr = -conv_rad_to_deg(iPhaseCorr);
    mLcdData.SNR = 10.0f * std::log10(snr);
    mLcdData.TestData1 = _compute_frequency_offset(mVolkNomCarrierVec, mVolkPhaseReference);
    mLcdData.TestData2 = 0;

    emit signal_show_lcd_data(&mLcdData);

    mShowCntStatistics = 0;
    mNextShownOfdmSymbIdx = (mNextShownOfdmSymbIdx + 1) % cL;
    if (mNextShownOfdmSymbIdx == 0) mNextShownOfdmSymbIdx = 1; // as iCurSymbolNo can never be zero here
  }

  // copy current FFT values as next OFDM reference
  memcpy(mVolkPhaseReference, mVolkNomCarrierVec, cK * sizeof(cmplx));
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
  volk_32f_accumulator_s32f_a(mVolkTemp1FloatVec, mVolkMeanNullPowerWithoutTII, cK); // this sums all vector elements to the first element
  return mVolkTemp1FloatVec[0] / (float)cK;
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
      const int16_t fftIdx = mVolkMapNomToFftIdx[nomCarrIdx];
      const float level = log10_times_20(std::abs(iFftBuffer[fftIdx]));
      float & meanNullLevelRef = mVolkMeanNullLevel[nomCarrIdx];
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
      const int16_t fftIdx = mVolkMapNomToFftIdx[nomCarrIdx];
      const float level = std::abs(iFftBuffer[fftIdx]);
      float & meanNullLevelRef = mVolkMeanNullLevel[nomCarrIdx];
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
  mVolkMeanNullLevel.fill_zeros();
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
    case EIqPlotType::PHASE_CORR_CARR_NORMED: mIqVector[nomCarrIdx] = mVolkFftBinRawVecPhaseCorr[nomCarrIdx] / std::sqrt(mVolkMeanPowerVector[nomCarrIdx]); break;
    case EIqPlotType::PHASE_CORR_MEAN_NORMED: mIqVector[nomCarrIdx] = mVolkFftBinRawVecPhaseCorr[nomCarrIdx] / std::sqrt(mMeanPowerOvrAll); break;
    case EIqPlotType::RAW_MEAN_NORMED:        mIqVector[nomCarrIdx] = mVolkFftBinRawVec[nomCarrIdx] / std::sqrt(mMeanPowerOvrAll); break;
    case EIqPlotType::DC_OFFSET_FFT_10:       mIqVector[nomCarrIdx] =  10.0f / (float)cTu * _interpolate_2d_plane(mDcFftLast, mDcFft, (float)nomCarrIdx / ((float)mIqVector.size() - 1)); break;
    case EIqPlotType::DC_OFFSET_FFT_100:      mIqVector[nomCarrIdx] = 100.0f / (float)cTu * _interpolate_2d_plane(mDcFftLast, mDcFft, (float)nomCarrIdx / ((float)mIqVector.size() - 1)); break;
    case EIqPlotType::DC_OFFSET_ADC_10:       mIqVector[nomCarrIdx] =  10.0f * mDcAdc; break;
    case EIqPlotType::DC_OFFSET_ADC_100:      mIqVector[nomCarrIdx] = 100.0f * mDcAdc; break;
    }

    const int16_t realCarrRelIdx = mVolkMapNomToRealCarrIdx[nomCarrIdx];
    const int16_t dataVecCarrIdx = (mShowNomCarrier ? nomCarrIdx : realCarrRelIdx);

    switch (mCarrierPlotType)
    {
    case ECarrierPlotType::SB_WEIGHT:
    {
      // Convert and limit the soft bit weight to percent
      // float weight2 = (std::abs(real(mVolk_r1[nomCarrIdx])) + std::abs(imag(mVolk_r1[nomCarrIdx]))) / 2.0f * (-iWeight);
      float val = (std::abs(mVolkViterbiFloatVecReal[nomCarrIdx]) + std::abs(mVolkViterbiFloatVecImag[nomCarrIdx])) / 2.0f;
      if (val > F_VITERBI_SOFT_BIT_VALUE_MAX) val = F_VITERBI_SOFT_BIT_VALUE_MAX; // limit graphics like the Viterbi itself does
      mCarrVector[dataVecCarrIdx] = 100.0f / VITERBI_SOFT_BIT_VALUE_MAX * val;  // show it in percent of the maximum Viterbi input
      break;
    }
    case ECarrierPlotType::EVM_PER:         mCarrVector[dataVecCarrIdx] = 100.0f * std::sqrt(mVolkMeanSigmaSqVector[nomCarrIdx]) / mVolkMeanLevelVector[nomCarrIdx]; break;
    case ECarrierPlotType::EVM_DB:          mCarrVector[dataVecCarrIdx] = 20.0f * std::log10(std::sqrt(mVolkMeanSigmaSqVector[nomCarrIdx]) / mVolkMeanLevelVector[nomCarrIdx]); break;
    case ECarrierPlotType::STD_DEV:         mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(std::sqrt(mVolkStdDevSqPhaseVector[nomCarrIdx])); break;
    case ECarrierPlotType::PHASE_ERROR:     mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(mVolkIntegAbsPhaseVector[nomCarrIdx]); break;
    case ECarrierPlotType::FOUR_QUAD_PHASE: mCarrVector[dataVecCarrIdx] = conv_rad_to_deg(std::arg(mVolkFftBinRawVecPhaseCorr[nomCarrIdx])); break;
    case ECarrierPlotType::REL_POWER:       mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(mVolkMeanPowerVector[nomCarrIdx] / mMeanPowerOvrAll); break;
    case ECarrierPlotType::SNR:             mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(mVolkMeanPowerVector[nomCarrIdx] / mVolkMeanNullPowerWithoutTII[nomCarrIdx]); break;
    case ECarrierPlotType::NULL_TII_LIN:
    case ECarrierPlotType::NULL_TII_LOG:
    case ECarrierPlotType::NULL_NO_TII:     mCarrVector[dataVecCarrIdx] = mAbsNullLevelGain * (mVolkMeanNullLevel[nomCarrIdx] - mAbsNullLevelMin); break;
    case ECarrierPlotType::NULL_OVR_POW:    mCarrVector[dataVecCarrIdx] = 10.0f * std::log10(mVolkMeanNullPowerWithoutTII[nomCarrIdx] / mMeanPowerOvrAll); break;
    }
  } // for (nomCarrIdx...
}

void OfdmDecoder::_volk_mean_filter_sum(float & ioValSum, const float * iValVec, const float iAlpha) const
{
  // ioVal += iAlpha * (iVal - ioVal);
  volk_32f_s32f_add_32f_a(mVolkTemp1FloatVec, iValVec, -ioValSum, cK);               // temp1 = (iVal - ioVal)
  volk_32f_s32f_multiply_32f_a(mVolkTemp2FloatVec, mVolkTemp1FloatVec, iAlpha, cK);  // temp2 = alpha * temp1
  volk_32f_accumulator_s32f_a(mVolkTemp1FloatVec, mVolkTemp2FloatVec, cK);           // this sums all vector elements to the first element
  ioValSum += mVolkTemp1FloatVec[0]; // use mVolkTemp1FloatVec as it is ensured correct aligned
};
