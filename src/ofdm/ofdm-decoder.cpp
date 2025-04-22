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
#include "dabradio.h"
#include <vector>

OfdmDecoder::OfdmDecoder(DabRadio * ipMr, RingBuffer<cmplx> * ipIqBuffer, RingBuffer<float> * ipCarrBuffer) :
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
  mMeanLevelVector.resize(cK);
  mMeanSigmaSqVector.resize(cK);

#ifdef USE_PHASE_CORR_LUT
  // create phase -> cmplx LUT
  for (int32_t phaseIdx = -cLutLen2; phaseIdx <= cLutLen2; ++phaseIdx)
  {
    const float phase = (float)phaseIdx / cLutFact;
    mLutPhase2Cmplx[phaseIdx + cLutLen2] = cmplx(std::cos(phase), std::sin(phase));
  }
#endif
  
  connect(this, &OfdmDecoder::signal_slot_show_iq, mpRadioInterface, &DabRadio::slot_show_iq);
  connect(this, &OfdmDecoder::signal_show_lcd_data, mpRadioInterface, &DabRadio::slot_show_lcd_data);

  reset();
}

OfdmDecoder::~OfdmDecoder()
{
}

#ifndef USE_PHASE_CORR_LUT
cmplx OfdmDecoder::cmplx_from_phase2(float d)
{
  const double M_4_PI = 1.273239544735162542821171882678754627704620361328125;
  const double PI4_A = 0.7853981554508209228515625;
  const double PI4_B = 0.794662735614792836713604629039764404296875e-8;
  const double PI4_C = 0.306161699786838294306516483068750264552437361480769e-16;
  const int N = 3; // order of argument reduction

  double s = abs(d);
  int q = (int)(s * M_4_PI);
  int r = q + (q & 1);
  s -= r * PI4_A;
  s -= r * PI4_B;
  s -= r * PI4_C;

  s = s * 0.125; // 2^-N
  s = s * s; // Evaluating Taylor series
  s = ((((s / 1814400. - 1.0 / 20160.0) * s + 1.0 / 360.0) * s - 1.0 / 12.0) * s + 1.0) * s;
  for (int i = 0; i < N; i++) // Applying double angle formula
  {
    s = (4.0 - s) * s;
  }
  s = s / 2.0;
  float sine = sqrt((2.0 - s) * s);
  float cosine = 1.0 - s;

  if (((q + 1) & 2) != 0)
  {
    s = cosine;
    cosine = sine;
    sine = s;
  }
  if (((q & 4) != 0) != (d < 0.0)) sine = -sine;
  if (((q + 2) & 4) != 0) cosine = -cosine;
  return cmplx(cosine, sine);
}
#endif

void OfdmDecoder::reset()
{
  std::fill(mStdDevSqPhaseVector.begin(), mStdDevSqPhaseVector.end(), 0.0f);
  std::fill(mIntegAbsPhaseVector.begin(), mIntegAbsPhaseVector.end(), 0.0f);
  std::fill(mMeanPhaseVector.begin(), mMeanPhaseVector.end(), 0.0f);
  std::fill(mMeanPowerVector.begin(), mMeanPowerVector.end(), 0.0f);
  std::fill(mMeanLevelVector.begin(), mMeanLevelVector.end(), 0.0f);
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

  constexpr float ALPHA = 0.1f;

  for (int32_t idx = -cK / 2; idx < cK / 2; ++idx)
  {
    // Consider FFT shift and skipping DC (0 Hz) bin.
    const int32_t fftIdx = fft_shift_skip_dc(idx, cTu);
    const float level = std::abs(iV[fftIdx]);
    mean_filter(mMeanNullPowerWithoutTII[fftIdx], level * level, ALPHA);
  }
}

void OfdmDecoder::store_reference_symbol_0(const TArrayTu & iBuffer)
{
  // We are now in the frequency domain, and we keep the carriers as coming from the FFT as phase reference.
  memcpy(mPhaseReference.data(), iBuffer.data(), cTu * sizeof(cmplx));
}

void OfdmDecoder::decode_symbol(const TArrayTu & iV, const uint16_t iCurOfdmSymbIdx, const float iPhaseCorr, std::vector<int16_t> & oBits)
{
  // current runtime on i7-6700K:
  // phase correction via:
  //   LUT:                avr: 240us, min: 108us
  //   cmplx_from_phase    avr: 235us, min: 101us
  //   cmplx_from_phase2   avr: 283us, min: 144us
  float sum = 0.0f;
  float stdDevSqOvrAll = 0.0f;

  ++mShowCntIqScope;
  ++mShowCntStatistics;
  const bool showScopeData = (mShowCntIqScope > cL && iCurOfdmSymbIdx == mNextShownOfdmSymbIdx);
  const bool showStatisticData = (mShowCntStatistics > 5 * cL && iCurOfdmSymbIdx == mNextShownOfdmSymbIdx);

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

    const int16_t dataVecCarrIdx = (mShowNomCarrier ? nomCarrIdx : realCarrRelIdx);

    constexpr float ALPHA = 0.005f;

    /*
     * Decoding is computing the phase difference between carriers with the same index in subsequent blocks.
     * The carrier of a block is the reference for the carrier on the same position in the next block.
     */
    const cmplx fftBinRaw = iV[fftIdx] * norm_to_length_one(conj(mPhaseReference[fftIdx])); // PI/4-DQPSK demodulation

    float & integAbsPhasePerBinRef = mIntegAbsPhaseVector[nomCarrIdx];

    //fftBin *= rotator; // Fine correction of phase which can't be done in the time domain (not more necessary as it is done below other way).

#ifdef USE_PHASE_CORR_LUT
    // do phase correction via LUT
    const int32_t phaseIdx = (int32_t)std::lround(integAbsPhasePerBinRef * cLutFact);
    assert(phaseIdx >= -cLutLen2 && phaseIdx <= cLutLen2 );  // the limitation is done below already
    const cmplx phase = mLutPhase2Cmplx[cLutLen2 - phaseIdx]; // the phase is used inverse here
    const cmplx fftBin = fftBinRaw * phase;
#else
    const cmplx fftBin = fftBinRaw * cmplx_from_phase2(-integAbsPhasePerBinRef);
    // const cmplx fftBin = fftBinRaw * cmplx_from_phase(-integAbsPhasePerBinRef);
#endif

    // Get phase and absolute phase for each bin.
    const float fftBinPhase = std::arg(fftBin);
    const float fftBinAbsPhase = turn_phase_to_first_quadrant(fftBinPhase);

    // Integrate phase error to perform the phase correction in the next OFDM symbol.
    integAbsPhasePerBinRef += 0.2f * ALPHA * (fftBinAbsPhase - F_M_PI_4); // TODO: correction still necessary?
    limit_symmetrically(integAbsPhasePerBinRef, F_RAD_PER_DEG * 20.0f); // this is important to not overdrive mLutPhase2Cmplx!

    // Get standard deviation of absolute phase for each bin. The integrator above assures that F_M_PI_4 is the phase mean value.
    const float curStdDevDiff = (fftBinAbsPhase -  F_M_PI_4);
    const float curStdDevSq = curStdDevDiff * curStdDevDiff;
    float & stdDevSqRef =  mStdDevSqPhaseVector[nomCarrIdx];
    mean_filter(stdDevSqRef, curStdDevSq, ALPHA);
    stdDevSqOvrAll += stdDevSqRef;

    // Calculate the mean of power of each bin to equalize the IQ diagram (simply looks nicer) and use it for SNR calculation.
    // Simplification: The IQ plot would need only the level, not power, so the root of the power is used (below)..
    const float fftBinAbsLevel = std::abs(fftBin);
    const float fftBinPower = fftBinAbsLevel * fftBinAbsLevel;

    float & meanLevelPerBinRef = mMeanLevelVector[nomCarrIdx];
    float & meanPowerPerBinRef = mMeanPowerVector[nomCarrIdx];
    mean_filter(meanLevelPerBinRef, fftBinAbsLevel, ALPHA);
    mean_filter(meanPowerPerBinRef, fftBinPower, ALPHA);
    mean_filter(mMeanPowerOvrAll, fftBinPower, ALPHA / (float)cK);

    // Collect data for "Log Likelihood Ratio"
    const float meanLevelAtAxisPerBin = meanLevelPerBinRef * F_SQRT1_2;
    const float realLevelDistPerBin = (std::abs(real(fftBin)) - meanLevelAtAxisPerBin); // x-distance to reference point
    const float imagLevelDistPerBin = (std::abs(imag(fftBin)) - meanLevelAtAxisPerBin); // y-distance to reference point
    const float sigmaSqPerBin = realLevelDistPerBin * realLevelDistPerBin + imagLevelDistPerBin * imagLevelDistPerBin; // squared distance to reference point
    float & meanSigmaSqPerBinRef = mMeanSigmaSqVector[nomCarrIdx];
    mean_filter(meanSigmaSqPerBinRef, sigmaSqPerBin, ALPHA);

    float signalPower = meanPowerPerBinRef - mMeanNullPowerWithoutTII[fftIdx];
    if (signalPower <= 0.0f) signalPower = 0.1f;

    // Calculate a soft-bit weight. The viterbi decoder will limit the soft-bit values to +/-127.
    float w1 = meanLevelPerBinRef / meanSigmaSqPerBinRef;
    w1 /= (mMeanNullPowerWithoutTII[fftIdx] / signalPower) +
          (mSoftBitType == ESoftBitType::SOFTDEC1 ? 1.0f : 2.0f); // 1/SNR + (1 or 2)

    cmplx r1;
    float w2;

    if (mSoftBitType == ESoftBitType::SOFTDEC3)
    {
      w1 *= std::abs(mPhaseReference[fftIdx]);
      r1 = fftBin * w1; // input power over current and last OFDM symbol
      w2 = -140 / mMeanValue;
    }
    else // SOFTDEC1 and SOFTDEC2, log likelihood ratio
    {
      w1 *= std::sqrt(std::abs(fftBin) * std::abs(mPhaseReference[fftIdx])); // input level
      r1 = norm_to_length_one(fftBin) * w1;
      w2 = -100 / mMeanValue;
    }

    // split the real and the imaginary part and scale it
    oBits[0  + nomCarrIdx] = (int16_t)(real(r1) * w2);
    oBits[cK + nomCarrIdx] = (int16_t)(imag(r1) * w2);
    sum += std::abs(r1);

    if (showScopeData)
    {
      switch (mIqPlotType)
      {
      case EIqPlotType::PHASE_CORR_CARR_NORMED: mIqVector[nomCarrIdx] = fftBin / std::sqrt(meanPowerPerBinRef); break;
      case EIqPlotType::PHASE_CORR_MEAN_NORMED: mIqVector[nomCarrIdx] = fftBin / std::sqrt(mMeanPowerOvrAll); break;
      case EIqPlotType::RAW_MEAN_NORMED:        mIqVector[nomCarrIdx] = fftBinRaw / std::sqrt(mMeanPowerOvrAll); break;
      case EIqPlotType::DC_OFFSET_FFT_10:       mIqVector[nomCarrIdx] =  10.0f / (float)cTu * _interpolate_2d_plane(mPhaseReference[0], iV[0], (float)nomCarrIdx / ((float)mIqVector.size() - 1)); break;
      case EIqPlotType::DC_OFFSET_FFT_100:      mIqVector[nomCarrIdx] = 100.0f / (float)cTu * _interpolate_2d_plane(mPhaseReference[0], iV[0], (float)nomCarrIdx / ((float)mIqVector.size() - 1)); break;
      case EIqPlotType::DC_OFFSET_ADC_10:       mIqVector[nomCarrIdx] =  10.0f * mDcAdc; break;
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
      case ECarrierPlotType::EVM_DB:          mCarrVector[dataVecCarrIdx] = 20.0f * std::log10(std::sqrt(meanSigmaSqPerBinRef) / meanLevelPerBinRef); break;
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

  //	From time to time we show the constellation of the current symbol
  if (showScopeData)
  {
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
    // mLcdData.TestData1 = _compute_frequency_offset(iV, mPhaseReference);
    mLcdData.TestData1 = mMeanValue;
    mLcdData.TestData2 = iPhaseCorr / F_2_M_PI * (float)cCarrDiff;;

    emit signal_show_lcd_data(&mLcdData);

    mShowCntStatistics = 0;
    mNextShownOfdmSymbIdx = (mNextShownOfdmSymbIdx + 1) % cL;
    if (mNextShownOfdmSymbIdx == 0) mNextShownOfdmSymbIdx = 1; // as iCurSymbolNo can never be zero here
  }

  memcpy(mPhaseReference.data(), iV.data(), cTu * sizeof(cmplx));
}

float OfdmDecoder::_compute_frequency_offset(const std::vector<cmplx> & r, const std::vector<cmplx> & c) const
{
  cmplx theta = cmplx(0, 0);

  for (int idx = -cK / 2; idx < cK / 2; idx += 1)
  {
    const int32_t index = fft_shift_skip_dc(idx, cTu); // this was with DC before in QT-DAB
    cmplx val = r[index] * conj(c[index]);
    val = turn_complex_phase_to_first_quadrant(val);
    theta += val;
  }
  return (arg(theta) - F_M_PI_4) / F_2_M_PI * (float)cTu / (float)cTs * (float)cCarrDiff;
}

float OfdmDecoder::_compute_noise_Power() const
{
  float sumNoise = 0.0f;
  for (int32_t idx = -cK / 2; idx < cK / 2; ++idx)
  {
    // Consider FFT shift and skipping DC (0 Hz) bin.
    const int32_t fftIdx = fft_shift_skip_dc(idx, cTu);
    sumNoise += mMeanNullPowerWithoutTII[fftIdx]; // the component already squared
  }
  return sumNoise / (float)cK;
}

void OfdmDecoder::_eval_null_symbol_statistics(const TArrayTu & iV)
{
  float max = -1e38f;
  float min =  1e38f;

  if (mCarrierPlotType == ECarrierPlotType::NULL_TII_LOG)
  {
    constexpr float ALPHA = 0.20f;
    for (int32_t idx = -cK / 2; idx < cK / 2; ++idx)
    {
      // Consider FFT shift and skipping DC (0 Hz) bin.
      const int32_t fftIdx = fft_shift_skip_dc(idx, cTu);
      const float level = log10_times_20(std::abs(iV[fftIdx]));
      float & meanNullLevelRef = mMeanNullLevel[fftIdx];
      mean_filter(meanNullLevelRef, level, ALPHA);

      if (meanNullLevelRef < min) min = meanNullLevelRef;
    }
    mean_filter(mAbsNullLevelMin, min, ALPHA);
    mAbsNullLevelGain = 1;
  }
  else
  {
    constexpr float ALPHA = 0.20f;
    for (int32_t idx = -cK / 2; idx < cK / 2; ++idx)
    {
      // Consider FFT shift and skipping DC (0 Hz) bin.
      const int32_t fftIdx = fft_shift_skip_dc(idx, cTu);
      const float level = std::abs(iV[fftIdx]);
      float & meanNullLevelRef = mMeanNullLevel[fftIdx];
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
