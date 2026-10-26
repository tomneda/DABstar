/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * The coherent demodulator EMethod::FRAME_AVERAGE was taken over from the project welle.io (https://github.com/AlbrechtL/welle.io),
 * file src/backend/ofdm-decoder.cpp/.h, class CoherentDemodulator. Many thanks to the welle.io team for this
 * contribution! It was adapted to the programming style and the data structures of DABstar.
 *
 * The coherent demodulator EMethod::DECISION_DIRECTED was taken over from the project OnAir,
 * file core/src/dab/dab_rx.cpp, DabReceiver::Impl::channelSoft(). It was adapted to the symbol-wise processing,
 * the programming style and the data structures of DABstar.
 *
 * The original copyright is acknowledged.
 */

#include "coherent_demod.h"
#include <algorithm>
#include <cmath>

// ---- EMethod::FRAME_AVERAGE
// Weight of the newest PRS in the average of the channel estimate. The first frames are averaged with equal weights.
constexpr f32 cChannelAlpha = 0.15f;
// Weight of the newest PRS in the average of the noise power
constexpr f32 cNoiseAlpha = 0.2f;
// The noise power of a carrier is averaged over this many neighbours on each side
constexpr i32 cNoiseSmoothing = 8;
// Taps of the impulse response that are kept: above this many times the median tap power (noise), and margin taps
// before and after. A lower threshold lets single noise taps far from the paths widen the window
// (7D recording at 3 dB SNR: 15 % less audio with 16 than with 64).
constexpr f32 cTapThreshold = 64.0f;
constexpr i32 cTapMargin = 16;
// A frame whose PRS power differs from the average by more than this factor, or whose correlation with the average
// is below cMinCorrelation, starts a new average (new channel, gain changed)
constexpr f32 cMaxPowerChange = 4.0f;
constexpr f32 cMinCorrelation = 0.25f;
// Distances of the carriers whose phase differences give the timing drift from symbol to symbol (precise, and without ambiguity)
constexpr i32 cTrackingDistance = 256;
constexpr i32 cCoarseTrackingDistance = 32;
// Weight of a timing measurement in the timing and in the timing rate, at high SNR. The measurement is noisy at low
// SNR: the weights are multiplied by SNR / (SNR + 2) (linear SNR, e.g. 0.5 at 3 dB).
constexpr f32 cTimingGain = 0.5f;
constexpr f32 cRateGain = 0.03f;
// Weight of a symbol in the average of the noise of the data symbols
constexpr f32 cDataNoiseAlpha = 0.05f;
// Limit of the log-likelihoods of the phases of a carrier
constexpr f32 cMaxLogLikelihood = 15.0f;
constexpr f32 cMinProbability = 1e-15f;
// Largest timing rate: 0.33 samples per symbol (130 ppm)
constexpr f32 cMaxTimingRate = 0.001f;
// The log-likelihood ratios are scaled with this factor to the range of the soft bits
constexpr f32 cLlrScale = 16.0f;

// ---- EMethod::DECISION_DIRECTED
// Taps of the impulse response of the PRS that are kept: above this many times the median tap power (noise),
// widened by cDdTapWidening taps on each side
constexpr f32 cDdTapThreshold = 12.0f;
constexpr i32 cDdTapWidening = 2;
// A frame whose channel estimate changed more than this (relative power of the change from symbol to symbol) makes
// the decisions unreliable: the next frame goes back to the differential demodulation
constexpr f64 cDdMaxChannelChange = 0.05;
// Weight of a frame in the average of the mean |LLR|
constexpr f32 cDdLlrRefAlpha = 0.2f;
// Mean |soft bit| of a symbol with the average |LLR|
constexpr f32 cDdSoftBitMean = 40.0f / 127.0f * F_VITERBI_SOFT_BIT_VALUE_MAX;

CoherentDemod::CoherentDemod()
  : PhaseTable()
{
  mFftPlanFwd = fftwf_plan_dft_1d(cTu, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_FORWARD, FFTW_ESTIMATE);
  mFftPlanBwd = fftwf_plan_dft_1d(cTu, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_BACKWARD, FFTW_ESTIMATE);

  i32 c = 0;
  for (i32 k = -cK / 2; k <= cK / 2; ++k)
  {
    if (k == 0) continue;
    mCarrier[c] = k;
    mFftBin[c] = (k + cTu) % cTu;
    mPrs[c] = PhaseTable::mRefTable[mFftBin[c]];
    ++c;
  }
  assert(c == cK);

  // log(1 + exp(-d)) in the middle of the steps of 1/8 up to 8, then nothing
  for (i32 i = 0; i < (i32)mMaxStarTable.size() - 1; ++i)
  {
    mMaxStarTable[i] = std::log1p(std::exp(-((f32)i + 0.5f) / 8.0f));
  }
  mMaxStarTable.back() = 0.0f;
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

void CoherentDemod::_rotation(const f32 iPhase, const f32 iSlope, TArrayK & oV) const
{
  const cf32 step = cmplx_from_phase(-iSlope);
  cf32 r = cmplx_from_phase(-(iPhase + iSlope * (f32)mCarrier[0]));
  oV[0] = r;
  for (i32 c = 1; c < cK; ++c)
  {
    for (i32 k = mCarrier[c - 1]; k < mCarrier[c]; ++k)
    {
      r *= step;
    }
    oV[c] = r;
  }
}

void CoherentDemod::set_method(const EMethod iMethod)
{
  mMethod = iMethod;
  reset();
}

void CoherentDemod::reset()
{
  mFrames = 0;
  mTimingRate = 0.0f;
  mDataNoise = 1.0f;

  mTapMaskValid = false;
  mChannelSlow = false;
  mNextOfdmSymbIdx = 0;
  mLlrRef = 0.0f;
}

bool CoherentDemod::is_ready() const
{
  switch (mMethod)
  {
  case EMethod::FRAME_AVERAGE:     return mFrames >= 2; // the channel estimate and the noise are known
  case EMethod::DECISION_DIRECTED: return mTapMaskValid && mChannelSlow;
  }
  return false;
}

void CoherentDemod::process_prs(const TArrayTu & iV)
{
  switch (mMethod)
  {
  case EMethod::FRAME_AVERAGE:     _process_prs_frame_average(iV); break;
  case EMethod::DECISION_DIRECTED: _process_prs_decision_directed(iV); break;
  }
}

void CoherentDemod::demodulate(const TArrayTu & iV, const i32 iCurOfdmSymbIdx, const cf32 & iGuardCorrSum)
{
  switch (mMethod)
  {
  case EMethod::FRAME_AVERAGE:     _demodulate_frame_average(iV, iCurOfdmSymbIdx, iGuardCorrSum); break;
  case EMethod::DECISION_DIRECTED: _demodulate_decision_directed(iV, iCurOfdmSymbIdx); break;
  }
}

void CoherentDemod::_process_prs_frame_average(const TArrayTu & iV)
{
  f32 rawPower = 0.0f;
  f32 avgPower = 0.0f;
  for (i32 c = 0; c < cK; ++c)
  {
    mHRaw[c] = iV[mFftBin[c]] * std::conj(mPrs[c]);
    rawPower += std::norm(mHRaw[c]);
    avgPower += std::norm(mHAvg[c]);
  }

  if (mFrames > 0 && (rawPower > cMaxPowerChange * avgPower || cMaxPowerChange * rawPower < avgPower))
  {
    mFrames = 0;
  }

  if (mFrames == 0)
  {
    mHAvg = mHRaw;
    mFrames = 1;
    _smooth_channel();
    return;
  }

  // Turn the average to the timing and the phase of this frame: a different FFT window position gives a phase that
  // grows linearly with the carrier number, the oscillator a common phase
  const f32 phasePerCarrier = _timing_rotation();
  cf32 common(0.0f, 0.0f);
  for (i32 c = 0; c < cK; ++c)
  {
    mHAvg[c] *= cmplx_from_phase(phasePerCarrier * (f32)mCarrier[c]);
    common += mHRaw[c] * std::conj(mHAvg[c]);
  }

  const cf32 commonRotation = norm_to_length_one(common);

  // A different channel (other ensemble, other antenna) hardly correlates with the average; at 0 dB SNR the correlation is still about 0.7
  if (std::abs(common) < cMinCorrelation * std::sqrt(rawPower * avgPower))
  {
    mHAvg = mHRaw;
    mFrames = 1;
    _smooth_channel();
    return;
  }

  // Equal weights for the first frames, then exponential
  const f32 alpha = std::max(cChannelAlpha, 1.0f / (f32)(mFrames + 1));
  // The difference between the new and the averaged estimate contains the noise of both: the average of n frames
  // still has 1/n of the noise power, the exponential average alpha / (2 - alpha)
  const f32 remaining = std::max(cChannelAlpha / (2.0f - cChannelAlpha), 1.0f / (f32)mFrames);
  const f32 noiseScale = 1.0f / (1.0f + remaining);
  const f32 beta = std::max(cNoiseAlpha, 1.0f / (f32)mFrames);
  for (i32 c = 0; c < cK; ++c)
  {
    mHAvg[c] *= commonRotation;
    const f32 e = std::norm(mHRaw[c] - mHAvg[c]) * noiseScale;
    if (mFrames == 1)
    {
      mNoiseVar[c] = e;
    }
    else
    {
      mean_filter(mNoiseVar[c], e, beta);
    }
    mean_filter(mHAvg[c], mHRaw[c], alpha);
  }
  ++mFrames;

  _smooth_channel();

  // Noise power, averaged over neighbouring carriers. The soft bits of a carrier are weighted with 2 / noise power.
  f32 signalSum = 0.0f;
  f32 noiseSum = 0.0f;
  for (i32 c = 0; c < cK; ++c)
  {
    const i32 first = std::max(0, c - cNoiseSmoothing);
    const i32 last = std::min(cK - 1, c + cNoiseSmoothing);
    f32 sum = 0.0f;
    for (i32 j = first; j <= last; ++j)
    {
      sum += mNoiseVar[j];
    }
    const f32 n = sum / (f32)(last - first + 1);
    mWeight[c] = (n > 0.0f ? 2.0f / n : 0.0f);
    signalSum += std::norm(mHSmooth[c]);
    noiseSum += n;
  }
  mSnr = (noiseSum > 0.0f ? signalSum / noiseSum : 0.0f);
}

f32 CoherentDemod::_timing_rotation()
{
  // Cross-spectrum of the new and the averaged estimate. A timing difference of d samples makes it turn by
  // a = 2 pi d / cTu per carrier. The phase differences of neighbouring carriers would be biased by echoes whose
  // strength changes a little from frame to frame; the maximum of the cross-correlation is not.
  for (i32 c = 0; c < cK; ++c)
  {
    mCross[c] = mHRaw[c] * std::conj(mHAvg[c]);
  }

  mFftInBuffer.fill(cf32(0.0f, 0.0f));
  for (i32 c = 0; c < cK; ++c)
  {
    mFftInBuffer[mFftBin[c]] = mCross[c];
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
  // The IFFT has its maximum at n = -a cTu / (2 pi)
  const i32 lag = (peak < cTu / 2 ? peak : peak - cTu);
  constexpr f32 step = F_2_M_PI / (f32)cTu;

  // Fraction of a sample: ternary search for the maximum of the correlation within the main lobe
  auto correlation = [this](const f32 iSlope)
  {
    _rotation(0.0f, iSlope, mRotations);
    cf32 sum(0.0f, 0.0f);
    for (i32 c = 0; c < cK; ++c)
    {
      sum += mCross[c] * mRotations[c];
    }
    return std::norm(sum);
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
  // The impulse response is short compared to cTu: keep only its taps, which removes most of the noise of the estimate
  mFftInBuffer.fill(cf32(0.0f, 0.0f));
  for (i32 c = 0; c < cK; ++c)
  {
    mFftInBuffer[mFftBin[c]] = mHAvg[c];
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
  for (i32 c = 0; c < cK; ++c)
  {
    mHSmooth[c] = mFftOutBuffer[mFftBin[c]];
  }

  // The window also smooths the edges of the band and the unused centre carrier into their empty neighbours.
  // Divide by the response of a flat channel to the same window.
  mFftInBuffer.fill(cf32(0.0f, 0.0f));
  for (i32 c = 0; c < cK; ++c)
  {
    mFftInBuffer[mFftBin[c]] = cf32(1.0f, 0.0f);
  }
  _ifft();
  for (i32 n = 0; n < cTu; ++n)
  {
    mFftInBuffer[n] = (mTapInWindow[n] ? mFftOutBuffer[n] : cf32(0.0f, 0.0f));
  }
  fftwf_execute(mFftPlanFwd);
  for (i32 c = 0; c < cK; ++c)
  {
    const f32 flat = std::real(mFftOutBuffer[mFftBin[c]]);
    if (flat > 0.1f)
    {
      mHSmooth[c] /= flat;
    }
    mReference[c] = std::conj(mHSmooth[c] * mPrs[c]);
  }
}

void CoherentDemod::_demodulate_frame_average(const TArrayTu & iV, const i32 iCurOfdmSymbIdx, const cf32 & iGuardCorrSum)
{
  if (!is_ready())
  {
    return; // the differential demodulation is used until the channel estimate is ready
  }

  if (iCurOfdmSymbIdx == 1)
  {
    // The previous symbol is the PRS, it is known
    for (auto & p : mPrevious)
    {
      p = { 1.0f, 0.0f, 0.0f, 0.0f };
    }
    mCommonPhase = 0.0f;
    mTimingPhase = 0.0f;
  }

  /*
   * Symbol iCurOfdmSymbIdx has the phases of the PRS plus iCurOfdmSymbIdx * pi/4 plus a multiple of pi/2. Remove the
   * channel, the PRS and iCurOfdmSymbIdx * pi/4, then the four possible phases are 0, pi/2, pi, 3 pi/2.
   *
   * The channel estimate is from the PRS. Until the end of the frame a residual frequency offset turns all carriers
   * (common phase), and the error of the sample clock moves the symbols in time (a phase growing linearly with the
   * carrier number). Both are predicted, then the nearest phases are decided and what is left is measured.
   *
   * The decisions can't tell a phase rate from the same rate plus pi/2 per symbol (about 200 Hz), but then every phase
   * difference would be wrong. So the phase rate comes from the correlation of the guard intervals of this frame,
   * which has no such ambiguity. The timing rate (sample clock) is the same in every frame and is averaged.
   */
  mCommonPhase += std::arg(iGuardCorrSum) * (f32)cTs / (f32)cTu;
  mTimingPhase += mTimingRate;
  _rotation(mCommonPhase + (f32)iCurOfdmSymbIdx * F_M_PI_4, mTimingPhase, mRotations);

  for (i32 c = 0; c < cK; ++c)
  {
    mSymbol[c] = iV[mFftBin[c]] * mReference[c] * mRotations[c];
    // Remove the decided phase, weighted with the reliability
    const cf32 v = mSymbol[c];
    cf32 r;
    if (std::abs(v.real()) >= std::abs(v.imag()))
    {
      r = (v.real() > 0.0f ? v : -v);
    }
    else
    {
      r = (v.imag() > 0.0f ? cf32(v.imag(), -v.real()) : cf32(-v.imag(), v.real()));
    }
    mResidual[c] = r * mWeight[c];
  }

  // Timing: phase differences of carriers far apart are precise, but the decisions fold every phase into +-pi/4,
  // so a slope of pi/2 over the distance looks like none. Carriers closer together resolve this.
  auto slope = [this](const i32 iDistance)
  {
    cf32 sum(0.0f, 0.0f);
    for (i32 c = 0; c + iDistance < cK; ++c)
    {
      const i32 c2 = c + iDistance;
      if (mCarrier[c2] - mCarrier[c] == iDistance)
      {
        sum += mResidual[c2] * std::conj(mResidual[c]);
      }
    }
    return std::arg(sum) / (f32)iDistance;
  };

  const f32 coarse = slope(cCoarseTrackingDistance);
  constexpr f32 step = F_M_PI_2 / (f32)cTrackingDistance;
  f32 measured = slope(cTrackingDistance);
  measured += step * std::round((coarse - measured) / step);

  // The timing changes slowly, its measurement is noisy at low SNR: follow it slowly, otherwise the noise adds up from symbol to symbol
  const f32 dataSnr = mSnr / std::max(1.0f, mDataNoise);
  const f32 reliability = dataSnr / (dataSnr + 2.0f);
  const f32 a = reliability * cTimingGain * measured;
  _rotation(0.0f, a, mRotations);
  cf32 common(0.0f, 0.0f);
  for (i32 c = 0; c < cK; ++c)
  {
    common += mResidual[c] * mRotations[c];
  }
  const f32 phase = std::arg(common);

  mTimingPhase += a;
  mCommonPhase += phase;
  mTimingRate += reliability * cRateGain * measured;
  limit_symmetrically(mTimingRate, cMaxTimingRate);

  _rotation(phase, a, mRotations);

  // The noise of the PRS doesn't contain the interference between symbols (echoes longer than the guard interval):
  // the PRS follows the almost empty null symbol. Measure the noise of the data symbols as the distance to the nearest
  // phases, relative to the noise of the PRS, and use the larger of both.
  const f32 noiseFactor = std::max(1.0f, mDataNoise);
  f32 relativeError = 0.0f;
  i32 counted = 0;

  // exp(j (pi/4 - m pi/2)) for the most probable previous phase m
  static const cf32 toDifference[4] = { {  F_SQRT1_2,  F_SQRT1_2 }, {  F_SQRT1_2, -F_SQRT1_2 },
                                        { -F_SQRT1_2, -F_SQRT1_2 }, { -F_SQRT1_2,  F_SQRT1_2 } };

  for (i32 c = 0; c < cK; ++c)
  {
    const cf32 v = mSymbol[c] * mRotations[c];
    const f32 w = mWeight[c] / noiseFactor;

    const f32 h2 = std::norm(mHSmooth[c]);
    if (h2 > 0.0f && mWeight[c] > 0.0f)
    {
      const cf32 nearest = (std::abs(v.real()) >= std::abs(v.imag()) ? cf32(v.real() > 0.0f ? h2 : -h2, 0.0f)
                                                                       : cf32(0.0f, v.imag() > 0.0f ? h2 : -h2));
      // |H|^2 times the noise power of the PRS
      relativeError += std::norm(v - nearest) * mWeight[c] / (2.0f * h2);
      ++counted;
    }

    /*
     * Probabilities of the absolute phases 0, pi/2, pi, 3 pi/2, from their log-likelihoods w * Re(v), w * Im(v),
     * -w * Re(v), -w * Im(v). Both scaled down if necessary, so that exp() doesn't overflow and the products below
     * stay normal floats (denormal ones are very slow); log-likelihood ratios of 30 are far above what the soft bits can hold.
     */
    f32 x = w * v.real();
    f32 y = w * v.imag();
    const f32 largest = std::max(std::abs(x), std::abs(y));
    if (largest > cMaxLogLikelihood)
    {
      x *= cMaxLogLikelihood / largest;
      y *= cMaxLogLikelihood / largest;
    }
    const f32 ex = std::exp(x);
    const f32 ey = std::exp(y);
    std::array<f32, 4> p = { ex, ey, 1.0f / ex, 1.0f / ey };
    const f32 norm = 1.0f / (p[0] + p[1] + p[2] + p[3]);
    for (auto & pm : p)
    {
      pm = std::max(pm * norm, cMinProbability);
    }

    // Phase difference n * pi/2 + pi/4 to the previous symbol
    const auto & q = mPrevious[c];
    f32 pd[4];
    for (i32 n = 0; n < 4; ++n)
    {
      pd[n] = 0.0f;
      for (i32 m = 0; m < 4; ++m)
      {
        pd[n] += q[m] * p[(m + n) & 3];
      }
    }

    // The differences pi/4 and 7 pi/4 have a positive real part (bit 0), pi/4 and 3 pi/4 a positive imaginary part.
    // The log-likelihood ratios (> 0: bit 0) are scaled to the soft bits (> 0: bit 1).
    constexpr f32 tiny = 1e-30f;
    const i32 fftIdx = mFftBin[c];
    mSoftBitRe[fftIdx] = -cLlrScale * std::log((pd[0] + pd[3] + tiny) / (pd[1] + pd[2] + tiny));
    mSoftBitIm[fftIdx] = -cLlrScale * std::log((pd[0] + pd[1] + tiny) / (pd[2] + pd[3] + tiny));

    const i32 prevIdx = (i32)(std::max_element(q.begin(), q.end()) - q.begin());
    mDecisions[fftIdx] = v * toDifference[prevIdx];

    mPrevious[c] = p;
  }

  if (counted > 0)
  {
    mean_filter(mDataNoise, relativeError / (f32)counted, cDataNoiseAlpha);
  }
}

/*
 * PRS (symbol 0) of a frame for EMethod::DECISION_DIRECTED.
 *
 * Steps:
 *  1. Evaluation of the frame just finished (statistics collected by _demodulate_decision_directed()):
 *     was the channel slow enough, and the slow average mLlrRef of the mean |LLR|. The result mChannelSlow is
 *     applied to the frame that starts now (one frame delay).
 *  2. Reset of the statistics and of the symbol counter for the new frame.
 *  3. Raw channel estimate of every carrier from the known PRS.
 *  4. Tap mask: the taps of the impulse response that stand out of the noise. It is used by _clean_estimate() for
 *     the PRS and for every data symbol of this frame.
 *  5. Cleaned estimate of the PRS as the start of the chain of estimates H_0, H_1, ...
 *
 * After reset() the first frame never gives soft bits (mChannelSlow needs a complete frame before), so the coherent
 * soft bits start with the second frame at the earliest.
 */
void CoherentDemod::_process_prs_decision_directed(const TArrayTu & iV)
{
  // ---- 1. Evaluation of the frame just finished
  // Only a complete frame tells whether the channel changed slowly enough for the decisions. A channel that changes
  // fast (Doppler of some 40 Hz and more, a car on a motorway in Band III) makes the decided carriers, and with them
  // the estimate, unreliable: the next frame goes back to the differential demodulation, which follows the channel
  // without decisions.
  // Complete: the tap mask was valid for the whole frame and all data symbols 1 .. cL-1 came without a gap.
  const bool frameComplete = (mTapMaskValid && mNextOfdmSymbIdx == cL);
  // sum |H_l - H_{l-1}|^2 / sum |H_{l-1}|^2 over all carriers and symbols: mean relative power of the change from
  // symbol to symbol, limit 5 % (-13 dB). Noise left in the estimates and wrong decisions count as change as well.
  mChannelSlow = frameComplete && mDeltaHSum < cDdMaxChannelChange * mPowerHSum;
  if (mChannelSlow)
  {
    // Mean |LLR| per bit of the frame (2 bits per carrier, cL-1 data symbols), the reference for the scaling of the
    // soft bits. Only frames with a slow channel count, the decisions of the others are not trustworthy.
    const f32 meanLlr = (f32)(mLlrSum / (f64)(2 * cK * (cL - 1))) + 1e-12f;
    if (mLlrRef > 0.0f)
    {
      mean_filter(mLlrRef, meanLlr, cDdLlrRefAlpha);
    }
    else
    {
      mLlrRef = meanLlr; // first slow frame since reset(): start the average with it
    }
  }

  // ---- 2. Start of the new frame
  mLlrSum = 0.0;
  mDeltaHSum = 0.0;
  mPowerHSum = 0.0;
  mNextOfdmSymbIdx = 1; // _demodulate_decision_directed() expects the data symbols 1, 2, ... without a gap

  // ---- 3. The PRS is known: the raw estimate of the channel
  // y_0 conj(x_0) = H_0 + n (|x_0| = 1). y_0 is also kept as the "previous symbol" of data symbol 1.
  for (i32 c = 0; c < cK; ++c)
  {
    mYPrev[c] = iV[mFftBin[c]];
    mRawEst[c] = mYPrev[c] * std::conj(mPrs[c]);
  }

  // ---- 4. The taps of the impulse response that stand out of the noise, widened a bit
  // Impulse response: backward FFT of the raw estimate, the bins without a carrier (outside band, DC) set to 0
  mFftInBuffer.fill(cf32(0.0f, 0.0f));
  for (i32 c = 0; c < cK; ++c)
  {
    mFftInBuffer[mFftBin[c]] = mRawEst[c];
  }
  fftwf_execute(mFftPlanBwd); // not normalized, only the ratio to the threshold matters

  for (i32 n = 0; n < cTu; ++n)
  {
    mTapPower[n] = std::norm(mFftOutBuffer[n]);
  }
  // The paths occupy only a few taps, most taps contain only noise: the median is a robust measure of the noise
  // power of a tap (for exponentially distributed noise power the median is ln(2) times the mean).
  // A pure noise tap exceeds 12 times the median with the probability 2^-12, i.e. about 0.5 false taps per PRS.
  mTapPowerSorted = mTapPower;
  std::nth_element(mTapPowerSorted.begin(), mTapPowerSorted.begin() + cTu / 2, mTapPowerSorted.end());
  const f32 threshold = cDdTapThreshold * mTapPowerSorted[cTu / 2];

  // The mask contains the normalization of the backward FFT in _clean_estimate()
  // Every strong tap is widened by cDdTapWidening taps on each side: the paths do not lie exactly on the tap grid and
  // the missing carriers (band edges, DC) spread a path over its neighbours. The index wraps around: taps before the
  // first path (pre-echoes, FFT window too late) lie at the end of the buffer.
  mTapMask.fill(0.0f);
  for (i32 n = 0; n < cTu; ++n)
  {
    if (mTapPower[n] > threshold)
    {
      for (i32 d = -cDdTapWidening; d <= cDdTapWidening; ++d)
      {
        mTapMask[(n + d + cTu) % cTu] = 1.0f / (f32)cTu;
      }
    }
  }
  const i32 kept = (i32)std::count_if(mTapMask.begin(), mTapMask.end(), [](const f32 iM) { return iM > 0.0f; });

  // Nothing stands out (no signal) or nothing is sparse (noise-like): differential demodulation
  // (cTu / 4 is about the length of the guard interval, a usable impulse response is not longer)
  mTapMaskValid = (kept > 0 && kept <= cTu / 4);
  if (!mTapMaskValid)
  {
    return; // is_ready() false and _demodulate_decision_directed() idle for this frame
  }

  // ---- 5. Start of the chain of estimates
  _clean_estimate(mRawEst, mHCur);
  // Both estimates the same: the prediction 2 H_cur - H_prev for data symbol 1 is H_0 (no slope known yet)
  mHPrev = mHCur; // a straight line through the last two estimates predicts the next one
}

void CoherentDemod::_clean_estimate(const TArrayK & iE, TArrayK & oH)
{
  // Through the time domain, only the masked taps kept (mFftInBuffer and mFftOutBuffer are different buffers)
  mFftInBuffer.fill(cf32(0.0f, 0.0f));
  for (i32 c = 0; c < cK; ++c)
  {
    mFftInBuffer[mFftBin[c]] = iE[c];
  }
  fftwf_execute(mFftPlanBwd);

  for (i32 n = 0; n < cTu; ++n)
  {
    mFftInBuffer[n] = mFftOutBuffer[n] * mTapMask[n];
  }
  fftwf_execute(mFftPlanFwd);

  for (i32 c = 0; c < cK; ++c)
  {
    oH[c] = mFftOutBuffer[mFftBin[c]];
  }
}

f32 CoherentDemod::_max_star(const f32 iA, const f32 iB) const
{
  // max*(a, b) = log(e^a + e^b): the larger plus a correction from a table
  const f32 m = std::max(iA, iB);
  const f32 d = std::abs(iA - iB);
  if (!(d < 8.0f)) return m;
  return m + mMaxStarTable[(i32)(d * 8.0f)];
}

/*
 * Decision-directed coherent demodulation of the data symbol l = iCurOfdmSymbIdx.
 *
 * Model of carrier c (index c omitted): y_l = H_l x_l + n_l with the channel H_l, the absolute (unknown) symbol x_l
 * and complex Gaussian noise n_l of power sigma^2. The transmitted information is the phase difference
 * d_l = x_l / x_{l-1} in {e^(j pi/4), e^(j 3pi/4), e^(-j 3pi/4), e^(-j pi/4)}.
 *
 * State on entry (written by _process_prs_decision_directed() or by the previous call):
 *   mHCur  = H_{l-1}  cleaned estimate of the previous symbol (for l = 1: the PRS)
 *   mHPrev = H_{l-2}  cleaned estimate of the symbol before that (for l = 1: also the PRS)
 *   mYPrev = y_{l-1}  received carriers of the previous symbol
 *
 * Steps:
 *  1. Hard decision of x_l with the channel predicted linearly from H_{l-1} and H_{l-2}.
 *  2. New channel estimate H_l from y_l with the decided x_l taken out, cleaned in the time domain.
 *  3. Noise power sigma^2 of this symbol from what the cleaning removed.
 *  4. Soft bits of d_l (log-MAP, the absolute phase of x_{l-1} is unknown and summed out).
 *  5. Statistics for the next PRS (channel change, mean |LLR|), shift of the state.
 *  6. Scaling of the soft bits to the range of the Viterbi decoder.
 */
void CoherentDemod::_demodulate_decision_directed(const TArrayTu & iV, const i32 iCurOfdmSymbIdx)
{
  // No usable impulse response in the PRS of this frame: nothing to do until the next PRS (is_ready() is false, the
  // statistics are not collected, so the next PRS also sees the frame as incomplete).
  if (!mTapMaskValid)
  {
    return;
  }

  // The estimate is carried from symbol to symbol, a missing symbol breaks the chain
  if (iCurOfdmSymbIdx != mNextOfdmSymbIdx)
  {
    mTapMaskValid = false; // a gap in the symbols: the channel is lost until the next PRS
    return;
  }
  ++mNextOfdmSymbIdx;

  // The PRS is on the axes, every DQPSK step turns by an odd multiple of 45 degrees
  // (so x_l lies on the diagonals for odd l and on the axes for even l; x_{l-1} is on the other grid)
  const bool diag = ((iCurOfdmSymbIdx & 1) != 0);

  // ---- 1. + 2. Decide the carriers with the estimate predicted from the last two symbols, then estimate the channel of this symbol
  for (i32 c = 0; c < cK; ++c)
  {
    const cf32 y = iV[mFftBin[c]];
    // Linear prediction H_l ~ H_{l-1} + (H_{l-1} - H_{l-2}) follows a slowly turning or fading channel.
    // z = conj(H_pred) y ~ |H|^2 x_l: x_l with the channel phase removed (the amplitude does not matter for the decision).
    const cf32 z = std::conj(2.0f * mHCur[c] - mHPrev[c]) * y;
    // s: nearest of the four possible points of x_l (|s| = 1)
    cf32 s;
    if (diag)
    {
      // Diagonals: the quadrant of z
      s = cf32(z.real() >= 0.0f ? F_SQRT1_2 : -F_SQRT1_2, z.imag() >= 0.0f ? F_SQRT1_2 : -F_SQRT1_2);
    }
    else if (std::abs(z.real()) >= std::abs(z.imag()))
    {
      // Axes: the real axis is nearer
      s = cf32(z.real() >= 0.0f ? 1.0f : -1.0f, 0.0f);
    }
    else
    {
      // Axes: the imaginary axis is nearer
      s = cf32(0.0f, z.imag() >= 0.0f ? 1.0f : -1.0f);
    }
    // Raw estimate y conj(s) = H_l + n' if s is right (|s| = 1: the noise keeps its power). A wrong decision turns this carrier
    // by a multiple of 90 degrees, which looks like a single outlier to the cleaning below.
    mRawEst[c] = y * std::conj(s);
  }
  // Keep only the taps of the impulse response found in the PRS: removes most of the noise and largely smooths out single
  // wrong decisions over the neighbouring carriers.
  _clean_estimate(mRawEst, mHNew);

  // ---- 3. The noise of this symbol: what is left after the channel times the decided symbol (wrong decisions make it larger, not smaller)
  // residual ~ cK sigma^2 (a bit less, the part of the noise inside the kept taps stays in mHNew).
  f64 residual = 0.0;
  f64 power = 0.0;
  for (i32 c = 0; c < cK; ++c)
  {
    residual += std::norm(mRawEst[c] - mHNew[c]);
    power += std::norm(mHNew[c]);
  }
  // The lower bound limits the SNR to 60 dB (and avoids a division by zero for a noise-free or empty signal)
  const f32 g = (f32)(2.0 * cK / std::max(residual, 1e-6 * power + 1e-30)); // 2 / sigma^2

  // ---- 4. Soft bits
  // With the unknown absolute phase s = x_{l-1} the log-likelihood of (s, d) is, up to terms that do not depend on s and d:
  //   -|y_{l-1} - H_{l-1} s|^2 / sigma^2 - |y_l - H_l s d|^2 / sigma^2
  //   = (2 / sigma^2) Re(conj(s) (conj(H_{l-1}) y_{l-1} + conj(H_l) y_l conj(d))) + const
  //   = Re(conj(s) g (a + b conj(d)))
  // Both symbols contribute to the reference of the phase: with a poor estimate of H only the combination
  // a + b conj(d) ~ y_{l-1} + y_l conj(d) works (like differential detection), with a good one every term on its own (coherent).
  f64 llrSum = 0.0;
  for (i32 c = 0; c < cK; ++c)
  {
    const cf32 y = iV[mFftBin[c]];
    const cf32 a = std::conj(mHCur[c]) * mYPrev[c]; // conj(H_{l-1}) y_{l-1} ~ |H|^2 x_{l-1}
    const cf32 b = std::conj(mHNew[c]) * y;         // conj(H_l) y_l         ~ |H|^2 x_l

    // Log of the likelihood of d = (iDr + j iDi) / sqrt(2), summed over the four phases of the previous symbol (log-MAP)
    auto metric = [&](const f32 iDr, const f32 iDi)
    {
      // w = g (a + b conj(d)), written out in real and imaginary part
      const f32 wr = g * (a.real() + (b.real() * iDr + b.imag() * iDi) * F_SQRT1_2);
      const f32 wi = g * (a.imag() + (b.imag() * iDr - b.real() * iDi) * F_SQRT1_2);
      // log sum_s exp(Re(conj(s) w)) over the four possible s (equally likely, the prior cancels in the LLR)
      // s in {1, -1, j, -j}: Re(conj(s) w) = wr, -wr, wi, -wi
      if (diag) return _max_star(_max_star(wr, -wr), _max_star(wi, -wi)); // previous symbol on the axes
      // s in {(+-1 +- j) / sqrt(2)}: Re(conj(s) w) = (+-wr +- wi) / sqrt(2) = u, -u, v, -v
      const f32 u = (wr + wi) * F_SQRT1_2;
      const f32 v = (wr - wi) * F_SQRT1_2;
      return _max_star(_max_star(u, -u), _max_star(v, -v));
    };

    // The four phase differences: mpp = e^(j pi/4), mpm = e^(-j pi/4), mmp = e^(j 3pi/4), mmm = e^(-j 3pi/4)
    const f32 mpp = metric( 1.0f,  1.0f);
    const f32 mpm = metric( 1.0f, -1.0f);
    const f32 mmp = metric(-1.0f,  1.0f);
    const f32 mmm = metric(-1.0f, -1.0f);
    // Each bit is carried by the sign of one component of d: LLR = log(sum of the likelihoods with bit 1 / with bit 0)
    const f32 llrRe = _max_star(mmp, mmm) - _max_star(mpp, mpm); // > 0: the real part negative, bit 1
    const f32 llrIm = _max_star(mpm, mmm) - _max_star(mpp, mmp); // > 0: the imaginary part negative, bit 1

    const i32 fftIdx = mFftBin[c];
    mSoftBitRe[fftIdx] = llrRe; // still in the scale of the LLR, scaled below
    mSoftBitIm[fftIdx] = llrIm;
    mDecisions[fftIdx] = b * std::conj(a); // ~ |H|^4 d: the channel-corrected phase difference for the constellation diagram
    llrSum += std::abs(llrRe) + std::abs(llrIm);

    // ---- 5. Statistics of the frame, evaluated at the next PRS: relative power of the channel change from symbol to symbol
    // (decides whether the next frame may use this method, see _process_prs_decision_directed())
    mDeltaHSum += std::norm(mHNew[c] - mHCur[c]);
    mPowerHSum += std::norm(mHCur[c]);
    mYPrev[c] = y;
  }
  mLlrSum += llrSum; // for the slow average mLlrRef at the next PRS

  // Shift of the estimates for the prediction of the next symbol
  mHPrev = mHCur;
  mHCur = mHNew;

  // ---- 6. Scaling of the LLR to the soft bits
  // The soft bits are scaled by the slow average of the mean |LLR| of the frames with a slow channel, not by this
  // symbol's own: a faded symbol then gives weak soft bits that the Viterbi decoder takes as nearly erased.
  // A symbol better than the average is scaled by its own mean |LLR|, so that its mean |soft bit| is cDdSoftBitMean
  // and does not grow beyond it. As long as no frame set mLlrRef (still 0), the own mean |LLR| is taken.
  const f32 meanLlr = (f32)(llrSum / (f64)(2 * cK)) + 1e-12f;
  const f32 scale = cDdSoftBitMean / std::max(meanLlr, mLlrRef);
  for (i32 c = 0; c < cK; ++c)
  {
    const i32 fftIdx = mFftBin[c];
    mSoftBitRe[fftIdx] *= scale;
    mSoftBitIm[fftIdx] *= scale;
  }
}
