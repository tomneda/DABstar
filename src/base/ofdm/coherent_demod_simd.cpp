/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * The coherent demodulator was taken over from the project welle.io (https://github.com/AlbrechtL/welle.io),
 * file src/backend/ofdm-decoder.cpp/.h, class CoherentDemodulator. Many thanks to the welle.io team for this
 * contribution! It was adapted to the programming style and the data structures of DABstar.
 *
 * The original copyright is acknowledged.
 */

#ifdef HAVE_SSE_OR_AVX

#include "coherent_demod_simd.h"
#include <algorithm>
#include <cmath>

// The constants and the algorithm are the same as in coherent_demod.cpp (see there for the explanations)
constexpr f32 cChannelAlpha = 0.15f;
constexpr f32 cNoiseAlpha = 0.2f;
constexpr i32 cNoiseSmoothing = 8;
constexpr f32 cTapThreshold = 64.0f;
constexpr i32 cTapMargin = 16;
constexpr f32 cMaxPowerChange = 4.0f;
constexpr f32 cMinCorrelation = 0.25f;
constexpr i32 cTrackingDistance = 256;
constexpr i32 cCoarseTrackingDistance = 32;
constexpr f32 cTimingGain = 0.5f;
constexpr f32 cRateGain = 0.03f;
constexpr f32 cDataNoiseAlpha = 0.05f;
constexpr f32 cMaxLogLikelihood = 15.0f;
constexpr f32 cMinProbability = 1e-15f;
constexpr f32 cMaxTimingRate = 0.001f;
constexpr f32 cLlrScale = 16.0f;

CoherentDemod::CoherentDemod(const FreqInterleaver & iFreqInterleaver, const TArrayTu & iRefTable)
{
  // FFTW_MEASURE overwrites the buffers while planning, so the plans have to be created before the buffers are used
  // (the same plans as in PhaseReference, so they are taken from the FFTW wisdom)
  mFftPlanFwd = fftwf_plan_dft_1d(cTu, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_FORWARD, FFTW_MEASURE);
  mFftPlanBwd = fftwf_plan_dft_1d(cTu, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_BACKWARD, FFTW_MEASURE);

  // carrier number k of the frequency index c = 0 .. cK-1 (lowest frequency first, no carrier at k = 0)
  auto carrier_of_freq_idx = [](const i32 iC) { return iC < cK / 2 ? iC - cK / 2 : iC - cK / 2 + 1; };

  for (i16 nomCarrIdx = 0; nomCarrIdx < cK; ++nomCarrIdx)
  {
    const i16 k = iFreqInterleaver.map_k_to_fft_bin(nomCarrIdx); // -cK/2 .. cK/2 without 0
    assert(k != 0 && k >= -cK / 2 && k <= cK / 2);
    const i16 fftBin = (k < 0 ? k + cTu : k);
    const i16 freqIdx = (k < 0 ? k + cK / 2 : k + cK / 2 - 1);
    assert(carrier_of_freq_idx(freqIdx) == k);
    mFftBin[nomCarrIdx] = fftBin;
    mFreqToNom[freqIdx] = nomCarrIdx;
    mCarrier[nomCarrIdx] = (f32)k;
    mPrs[nomCarrIdx] = iRefTable[fftBin];
  }

  // carrier pairs with a fixed distance (the gap at k = 0 makes the distance of the pairs across it one larger, they are not used)
  auto make_pairs = [&](const i32 iDistance, std::vector<i16> & oLow, std::vector<i16> & oHigh)
  {
    for (i32 c = 0; c + iDistance < cK; ++c)
    {
      if (carrier_of_freq_idx(c + iDistance) - carrier_of_freq_idx(c) == iDistance)
      {
        oLow.push_back(mFreqToNom[c]);
        oHigh.push_back(mFreqToNom[c + iDistance]);
      }
    }
  };
  make_pairs(cTrackingDistance, mPairLowFine, mPairHighFine);
  make_pairs(cCoarseTrackingDistance, mPairLowCoarse, mPairHighCoarse);

  mPairLow.fill_zeros();
  mPairHigh.fill_zeros();
  mHAvg.fill_zeros();
}

CoherentDemod::~CoherentDemod()
{
  fftwf_destroy_plan(mFftPlanBwd);
  fftwf_destroy_plan(mFftPlanFwd);
}

void CoherentDemod::_ifft()
{
  fftwf_execute(mFftPlanBwd);

  constexpr f32 cScale = 1.0f / (f32)cTu;
  for (auto & v : mFftOutBuffer)
  {
    v *= cScale;
  }
}

void CoherentDemod::_phasor(const f32 iPhase, const f32 iSlope, SimdVec<cf32> & oV)
{
  mPhase.set_multiply_vector_and_scalar_each_element(mCarrier, iSlope);
  mPhase.modify_add_scalar_each_element(iPhase);
  oV.set_unit_phasor_each_element(mPhase);
}

void CoherentDemod::reset()
{
  mFrames = 0;
  mTimingRate = 0.0f;
  mDataNoise = 1.0f;
}

void CoherentDemod::process_prs(const SimdVec<cf32> & iV)
{
  mHRaw.set_multiply_conj_each_element(iV, mPrs);
  mTmp1.set_squared_magnitude_each_element(mHRaw);
  const f32 rawPower = mTmp1.get_sum_of_elements();
  mTmp1.set_squared_magnitude_each_element(mHAvg);
  const f32 avgPower = mTmp1.get_sum_of_elements();

  if (mFrames > 0 && (rawPower > cMaxPowerChange * avgPower || cMaxPowerChange * rawPower < avgPower))
  {
    mFrames = 0;
  }

  if (mFrames == 0)
  {
    mHAvg.set_copy(mHRaw);
    mFrames = 1;
    _smooth_channel();
    return;
  }

  // Turn the average to the timing and the phase of this frame
  const f32 phasePerCarrier = _timing_rotation();
  _phasor(0.0f, phasePerCarrier, mPhasor);
  mHAvg.set_multiply_each_element(mHAvg, mPhasor);
  const cf32 common = mHRaw.get_conj_dot_product(mHAvg); // sum of mHRaw * conj(mHAvg)
  const cf32 commonRotation = norm_to_length_one(common);

  // A different channel hardly correlates with the average
  if (std::abs(common) < cMinCorrelation * std::sqrt(rawPower * avgPower))
  {
    mHAvg.set_copy(mHRaw);
    mFrames = 1;
    _smooth_channel();
    return;
  }

  // Equal weights for the first frames, then exponential (once per frame, a simple loop is enough)
  const f32 alpha = std::max(cChannelAlpha, 1.0f / (f32)(mFrames + 1));
  const f32 remaining = std::max(cChannelAlpha / (2.0f - cChannelAlpha), 1.0f / (f32)mFrames);
  const f32 noiseScale = 1.0f / (1.0f + remaining);
  const f32 beta = std::max(cNoiseAlpha, 1.0f / (f32)mFrames);
  for (i32 i = 0; i < cK; ++i)
  {
    cf32 & hAvg = mHAvg[i];
    hAvg *= commonRotation;
    const f32 e = std::norm(mHRaw[i] - hAvg) * noiseScale;
    if (mFrames == 1)
    {
      mNoiseVar[i] = e;
    }
    else
    {
      mean_filter(mNoiseVar[i], e, beta);
    }
    mean_filter(hAvg, mHRaw[i], alpha);
  }
  ++mFrames;

  _smooth_channel();
  _update_weights();
}

void CoherentDemod::_update_weights()
{
  // Noise power, averaged over neighbouring carriers (in frequency order). The soft bits of a carrier are weighted with 2 / noise power.
  mNoisePrefixSum[0] = 0.0;
  for (i32 c = 0; c < cK; ++c)
  {
    mNoisePrefixSum[c + 1] = mNoisePrefixSum[c] + (f64)mNoiseVar[mFreqToNom[c]];
  }

  mH2.set_squared_magnitude_each_element(mHSmooth);

  f32 signalSum = 0.0f;
  f32 noiseSum = 0.0f;
  mErrCount = 0;
  for (i32 c = 0; c < cK; ++c)
  {
    const i32 first = std::max(0, c - cNoiseSmoothing);
    const i32 last = std::min(cK - 1, c + cNoiseSmoothing);
    const f32 n = (f32)((mNoisePrefixSum[last + 1] - mNoisePrefixSum[first]) / (f64)(last - first + 1));
    const i16 i = mFreqToNom[c];
    const f32 w = (n > 0.0f ? 2.0f / n : 0.0f);
    const f32 h2 = mH2[i];
    mWeight[i] = w;
    if (h2 > 0.0f && w > 0.0f)
    {
      mErrScale[i] = w / (2.0f * h2);
      ++mErrCount;
    }
    else
    {
      mErrScale[i] = 0.0f;
    }
    signalSum += h2;
    noiseSum += n;
  }
  mSnr = (noiseSum > 0.0f ? signalSum / noiseSum : 0.0f);
  mInvRmsChannel = (signalSum > 0.0f ? 1.0f / std::sqrt(signalSum / (f32)cK) : 0.0f);
}

f32 CoherentDemod::_timing_rotation()
{
  // Cross-spectrum of the new and the averaged estimate, its IFFT has the maximum at the timing difference
  mCross.set_multiply_conj_each_element(mHRaw, mHAvg);

  mFftInBuffer.fill(cf32(0.0f, 0.0f));
  for (i32 i = 0; i < cK; ++i)
  {
    mFftInBuffer[mFftBin[i]] = mCross[i];
  }
  _ifft();

  const TArrayTu & time = mFftOutBuffer;
  i32 peak = 0;
  for (i32 n = 1; n < cTu; ++n)
  {
    if (std::norm(time[n]) > std::norm(time[peak]))
    {
      peak = n;
    }
  }
  const i32 lag = (peak < cTu / 2 ? peak : peak - cTu);
  constexpr f32 step = F_2_M_PI / (f32)cTu;

  // Fraction of a sample: ternary search for the maximum of the correlation within the main lobe
  auto correlation = [this](const f32 iSlope)
  {
    _phasor(0.0f, iSlope, mPhasor);
    return std::norm(mCross.get_conj_dot_product(mPhasor)); // sum of mCross * exp(-j iSlope k)
  };

  f32 low  = (f32)(-lag - 1) * step;
  f32 high = (f32)(-lag + 1) * step;
  for (i32 i = 0; i < 16; ++i)
  {
    const f32 m1 = low + (high - low) / 3.0f;
    const f32 m2 = high - (high - low) / 3.0f;
    if (correlation(m1) < correlation(m2))
    {
      low = m1;
    }
    else
    {
      high = m2;
    }
  }
  return (low + high) / 2.0f;
}

void CoherentDemod::_smooth_channel()
{
  // The impulse response is short compared to cTu: keep only its taps (once per frame, the FFT order is needed)
  mFftInBuffer.fill(cf32(0.0f, 0.0f));
  for (i32 i = 0; i < cK; ++i)
  {
    mFftInBuffer[mFftBin[i]] = mHAvg[i];
  }
  _ifft();

  const TArrayTu & time = mFftOutBuffer;
  for (i32 n = 0; n < cTu; ++n)
  {
    mTapPower[n] = std::norm(time[n]);
  }
  const i32 peak = (i32)(std::max_element(mTapPower.begin(), mTapPower.end()) - mTapPower.begin());
  mTapPowerSorted = mTapPower;
  std::nth_element(mTapPowerSorted.begin(), mTapPowerSorted.begin() + cTu / 2, mTapPowerSorted.end());
  const f32 threshold = cTapThreshold * mTapPowerSorted[cTu / 2];

  // Taps above the noise within +-cTu/4 of the strongest one
  i32 first = 0;
  i32 last = 0;
  for (i32 d = -cTu / 4; d < cTu / 4; ++d)
  {
    if (mTapPower[(peak + d + cTu) % cTu] > threshold)
    {
      first = std::min(first, d);
      last = std::max(last, d);
    }
  }
  first -= cTapMargin;
  last += cTapMargin;

  // Keep only the taps within the window (mFftInBuffer and mFftOutBuffer are different buffers)
  mFftInBuffer.fill(cf32(0.0f, 0.0f));
  mTapInWindow.fill(false);
  for (i32 d = first; d <= last; ++d)
  {
    const i32 n = (peak + d + cTu) % cTu;
    mFftInBuffer[n] = time[n];
    mTapInWindow[n] = true;
  }

  fftwf_execute(mFftPlanFwd);
  for (i32 i = 0; i < cK; ++i)
  {
    mHSmooth[i] = mFftOutBuffer[mFftBin[i]];
  }

  // Divide by the response of a flat channel to the same window (band edges and the centre carrier)
  mFftInBuffer.fill(cf32(0.0f, 0.0f));
  for (i32 i = 0; i < cK; ++i)
  {
    mFftInBuffer[mFftBin[i]] = cf32(1.0f, 0.0f);
  }
  _ifft();
  for (i32 n = 0; n < cTu; ++n)
  {
    mFftInBuffer[n] = (mTapInWindow[n] ? mFftOutBuffer[n] : cf32(0.0f, 0.0f));
  }
  fftwf_execute(mFftPlanFwd);
  for (i32 i = 0; i < cK; ++i)
  {
    const f32 flat = std::real(mFftOutBuffer[mFftBin[i]]);
    if (flat > 0.1f)
    {
      mHSmooth[i] /= flat;
    }
    mReference[i] = std::conj(mHSmooth[i] * mPrs[i]);
  }
}

f32 CoherentDemod::_slope(const std::vector<i16> & iLow, const std::vector<i16> & iHigh, const i32 iDistance)
{
  // Mean phase difference of the carriers iDistance apart, per carrier. The unused end of mPairLow/mPairHigh stays 0.
  const u32 n = (u32)iLow.size();
  for (u32 j = 0; j < n; ++j)
  {
    mPairLow[j] = mResidual[iLow[j]];
    mPairHigh[j] = mResidual[iHigh[j]];
  }
  return std::arg(mPairHigh.get_conj_dot_product(mPairLow, n)) / (f32)iDistance;
}

void CoherentDemod::demodulate(const SimdVec<cf32> & iV, const i32 iCurOfdmSymbIdx, const cf32 & iGuardCorrSum, const bool iCalcConstellation)
{
  if (!is_ready())
  {
    return; // the differential demodulation is used until the channel estimate is ready
  }

  if (iCurOfdmSymbIdx == 1)
  {
    // The previous symbol is the PRS, it is known
    mQ[0].fill(1.0f);
    mQ[1].fill_zeros();
    mQ[2].fill_zeros();
    mQ[3].fill_zeros();
    mCommonPhase = 0.0f;
    mTimingPhase = 0.0f;
  }

  // Remove the channel, the PRS and iCurOfdmSymbIdx * pi/4 with the predicted phase and timing drift (see coherent_demod.cpp)
  mCommonPhase += std::arg(iGuardCorrSum) * (f32)cTs / (f32)cTu;
  mTimingPhase += mTimingRate;
  _phasor(mCommonPhase + (f32)iCurOfdmSymbIdx * F_M_PI_4, mTimingPhase, mPhasor);
  mTmpCmplx.set_multiply_each_element(iV, mReference);
  mSymbol.set_multiply_conj_each_element(mTmpCmplx, mPhasor);

  // Remove the decided phase, weighted with the reliability
  mFolded.set_fold_to_positive_real_axis_each_element(mSymbol);
  mResidual.set_multiply_real_each_element(mFolded, mWeight);

  // Timing: carriers far apart are precise, carriers closer together resolve the ambiguity of the decisions
  const f32 coarse = _slope(mPairLowCoarse, mPairHighCoarse, cCoarseTrackingDistance);
  constexpr f32 step = F_M_PI_2 / (f32)cTrackingDistance;
  f32 measured = _slope(mPairLowFine, mPairHighFine, cTrackingDistance);
  measured += step * std::round((coarse - measured) / step);

  // The timing changes slowly, its measurement is noisy at low SNR
  const f32 dataSnr = mSnr / std::max(1.0f, mDataNoise);
  const f32 reliability = dataSnr / (dataSnr + 2.0f);
  const f32 a = reliability * cTimingGain * measured;
  _phasor(0.0f, a, mPhasor);
  const f32 phase = std::arg(mResidual.get_conj_dot_product(mPhasor)); // sum of mResidual * exp(-j a k)

  mTimingPhase += a;
  mCommonPhase += phase;
  mTimingRate += reliability * cRateGain * measured;
  limit_symmetrically(mTimingRate, cMaxTimingRate);

  // v = mSymbol * exp(-j (phase + a k))
  mV.set_multiply_conj_each_element(mSymbol, mPhasor);
  mV.modify_multiply_scalar_each_element(cmplx_from_phase(-phase));

  // Noise of the data symbols (distance to the nearest phases) relative to the noise of the PRS
  const f32 noiseFactor = std::max(1.0f, mDataNoise);
  mFolded.set_fold_to_positive_real_axis_each_element(mV);
  mFolded.store_to_real_and_imag_each_element(mRe, mIm);
  mRe.set_subtract_each_element(mRe, mH2);                  // the nearest phase is at |H|^2 on the positive real axis
  mTmp1.set_squared_magnitude_of_elements(mRe, mIm);
  const f32 relativeError = mTmp1.get_dot_product(mErrScale);

  // Log-likelihoods w Re(v), w Im(v) of the absolute phases, scaled down if necessary
  mV.store_to_real_and_imag_each_element(mRe, mIm);
  mW.set_multiply_vector_and_scalar_each_element(mWeight, 1.0f / noiseFactor);
  mX.set_multiply_each_element(mRe, mW);
  mY.set_multiply_each_element(mIm, mW);
  mTmp1.set_abs_each_element(mX);
  mTmp2.set_abs_each_element(mY);
  mTmp1.set_max_each_element(mTmp1, mTmp2);
  mTmp1.modify_clamp_each_element(cMaxLogLikelihood, 1e30f);
  mTmp2.set_reciprocal_each_element(mTmp1);
  mTmp2.modify_multiply_scalar_each_element(cMaxLogLikelihood); // min(1, cMaxLogLikelihood / largest)
  mX.modify_multiply_each_element(mTmp2);
  mY.modify_multiply_each_element(mTmp2);

  // Probabilities of the absolute phases 0, pi/2, pi, 3 pi/2
  mP[0].set_exp_each_element(mX);
  mP[1].set_exp_each_element(mY);
  mP[2].set_reciprocal_each_element(mP[0]);
  mP[3].set_reciprocal_each_element(mP[1]);
  mTmp1.set_add_each_element(mP[0], mP[1]);
  mTmp1.set_add_each_element(mTmp1, mP[2]);
  mTmp1.set_add_each_element(mTmp1, mP[3]);
  mTmp2.set_reciprocal_each_element(mTmp1);
  for (auto & p : mP)
  {
    p.modify_multiply_each_element(mTmp2);
    p.modify_clamp_each_element(cMinProbability, 1.0f);
  }

  if (iCalcConstellation)
  {
    _calc_constellation(); // needs mQ of the previous symbol
  }

  // Phase difference n * pi/2 + pi/4 to the previous symbol: pd[n] = sum_m q[m] p[(m + n) & 3]
  for (i32 n = 0; n < 4; ++n)
  {
    mPd[n].set_multiply_each_element(mQ[0], mP[n]);
    for (i32 m = 1; m < 4; ++m)
    {
      mPd[n].modify_add_product_each_element(mQ[m], mP[(m + n) & 3]);
    }
  }

  // Log-likelihood ratios (> 0: bit 0) scaled to the soft bits (> 0: bit 1).
  // pi/4 and 7 pi/4 have a positive real part, pi/4 and 3 pi/4 a positive imaginary part.
  constexpr f32 tiny = 1e-30f;
  mTmp1.set_add_each_element(mPd[0], mPd[3]);
  mTmp2.set_add_each_element(mPd[1], mPd[2]);
  mTmp1.modify_add_scalar_each_element(tiny);
  mTmp2.modify_add_scalar_each_element(tiny);
  mTmp1.set_divide_each_element(mTmp1, mTmp2);
  mSoftBitRe.set_ln_each_element(mTmp1);
  mSoftBitRe.modify_multiply_scalar_each_element(-cLlrScale);

  mTmp1.set_add_each_element(mPd[0], mPd[1]);
  mTmp2.set_add_each_element(mPd[2], mPd[3]);
  mTmp1.modify_add_scalar_each_element(tiny);
  mTmp2.modify_add_scalar_each_element(tiny);
  mTmp1.set_divide_each_element(mTmp1, mTmp2);
  mSoftBitIm.set_ln_each_element(mTmp1);
  mSoftBitIm.modify_multiply_scalar_each_element(-cLlrScale);

  for (i32 m = 0; m < 4; ++m)
  {
    mQ[m].set_copy(mP[m]);
  }

  if (mErrCount > 0)
  {
    mean_filter(mDataNoise, relativeError / (f32)mErrCount, cDataNoiseAlpha);
  }
}

void CoherentDemod::_calc_constellation()
{
  // exp(j (pi/4 - m pi/2)) for the most probable previous phase m
  static const cf32 toDifference[4] = { {  F_SQRT1_2,  F_SQRT1_2 }, {  F_SQRT1_2, -F_SQRT1_2 },
                                        { -F_SQRT1_2, -F_SQRT1_2 }, { -F_SQRT1_2,  F_SQRT1_2 } };

  for (i32 i = 0; i < cK; ++i)
  {
    i32 prevIdx = 0;
    for (i32 m = 1; m < 4; ++m)
    {
      if (mQ[m][i] > mQ[prevIdx][i]) prevIdx = m;
    }
    const cf32 d = mV[i] * toDifference[prevIdx]; // ~ |H|^2 times the phase difference
    const f32 h2 = mH2[i];
    if (h2 > 0.0f)
    {
      mConstCarrNormed[i] = d / h2;
      mConstMeanNormed[i] = d * (mInvRmsChannel / std::sqrt(h2)); // ~ |H| / RMS(|H|)
    }
    else
    {
      mConstCarrNormed[i] = mConstMeanNormed[i] = cf32(0.0f, 0.0f);
    }
  }
}

#endif // HAVE_SSE_OR_AVX
