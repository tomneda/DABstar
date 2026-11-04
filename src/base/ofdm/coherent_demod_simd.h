/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * The coherent demodulator was taken over from the project welle.io (https://github.com/AlbrechtL/welle.io),
 * file src/backend/ofdm-decoder.cpp/.h, class CoherentDemodulator. Many thanks to the welle.io team for this
 * contribution! It was adapted to the programming style and the data structures of DABstar.
 *
 * The original copyright information is acknowledged.
 */
#pragma once

#ifdef HAVE_SSE_OR_AVX

#include "dab_constants.h"
#include "freq_interleaver.h"
#include "simd_extensions.h"
#include <fftw3.h>
#include <array>
#include <vector>

/*
 * SIMD version of the coherent demodulation (see coherent_demod.h for the description of the algorithm), used by the
 * SIMD OfdmDecoder.
 *
 * All carrier vectors are in the nominal (frequency de-interleaved) carrier order of the SIMD OfdmDecoder, so the
 * carriers already collected by it are used directly and the soft bits are delivered in its order. The few steps that
 * need the frequency order (timing from carriers in a fixed distance, noise averaged over neighbouring carriers) use
 * index maps.
 */
class CoherentDemod
{
public:
  CoherentDemod(const FreqInterleaver & iFreqInterleaver, const TArrayTu & iRefTable);
  ~CoherentDemod();

  // Forget the channel estimate (new channel, sync lost, mode switched)
  void reset();

  // iV: the PRS carriers in nominal carrier order. Updates the channel estimate for the following data symbols of this frame.
  void process_prs(const SimdVec<cf32> & iV);

  // True once the channel estimate is usable for the soft bits of the data symbols of this frame
  [[nodiscard]] bool is_ready() const { return mFrames >= 2; } // the channel estimate and the noise are known

  // iV: carriers of data symbol iCurOfdmSymbIdx (1 .. cL-1) of the frame of the last PRS in nominal carrier order,
  // iGuardCorrSum: sum of x[n + cTu] * conj(x[n]) over the guard intervals (n = 0 .. cTg-1) of all data symbols of
  // this frame up to this one, for the frequency offset.
  // iCalcConstellation: also calculate the constellation points of this symbol (only needed for the IQ scope).
  // Has to be called for every data symbol of the frame (also if is_ready() is false, so that the channel can be tracked).
  // Afterwards (if is_ready()) get_soft_bits_real() and get_soft_bits_imag() are the soft bits of the two bits of each
  // carrier in nominal carrier order, scaled to the soft bit range but not limited (> 0: bit 1).
  void demodulate(const SimdVec<cf32> & iV, i32 iCurOfdmSymbIdx, const cf32 & iGuardCorrSum, bool iCalcConstellation);

  [[nodiscard]] const SimdVec<f32> & get_soft_bits_real() const { return mSoftBitRe; }
  [[nodiscard]] const SimdVec<f32> & get_soft_bits_imag() const { return mSoftBitIm; }

  // Constellation point of the carrier with nominal index iNomCarrIdx (only valid after demodulate() with
  // iCalcConstellation and if is_ready()), see coherent_demod.h
  [[nodiscard]] cf32 get_constellation_carr_normed(i32 iNomCarrIdx) const { return mConstCarrNormed[iNomCarrIdx]; }
  [[nodiscard]] cf32 get_constellation_mean_normed(i32 iNomCarrIdx) const { return mConstMeanNormed[iNomCarrIdx]; }

private:
  // ---- index maps
  std::array<i16, cK> mFftBin{};    // nominal carrier index -> FFT bin
  std::array<i16, cK> mFreqToNom{}; // carriers sorted by frequency (lowest first) -> nominal carrier index
  // nominal carrier indices of the carrier pairs with the distance cTrackingDistance (and cCoarseTrackingDistance):
  // carrier mPairHigh[i] is the given distance above carrier mPairLow[i]
  std::vector<i16> mPairLowFine, mPairHighFine;
  std::vector<i16> mPairLowCoarse, mPairHighCoarse;

  SimdVec<f32> mCarrier{0};    // carrier number k (-cK/2 .. cK/2, without 0)
  SimdVec<cf32> mPrs{0};       // PRS of every carrier

  alignas(64) TArrayTu mFftInBuffer;
  alignas(64) TArrayTu mFftOutBuffer;
  fftwf_plan mFftPlanFwd;
  fftwf_plan mFftPlanBwd;

  // working buffers of _smooth_channel() (in FFT order)
  std::array<f32, cTu> mTapPower{};
  std::array<f32, cTu> mTapPowerSorted{};
  std::array<bool, cTu> mTapInWindow{};
  std::array<f64, cK + 1> mNoisePrefixSum{}; // see _update_weights()

  // ---- channel estimate (per frame)
  i32 mFrames = 0;
  SimdVec<cf32> mHRaw{0};       // estimate of the last PRS only
  SimdVec<cf32> mHAvg{0};       // averaged over frames
  SimdVec<cf32> mHSmooth{0};    // mHAvg smoothed in the time domain
  SimdVec<cf32> mReference{0};  // conj(mHSmooth * mPrs)
  SimdVec<cf32> mCross{0};      // mHRaw * conj(mHAvg)
  SimdVec<f32> mNoiseVar{0};    // noise power of every carrier
  SimdVec<f32> mWeight{0};      // 2 / mNoiseVar, smoothed
  SimdVec<f32> mH2{0};          // |mHSmooth|^2
  SimdVec<f32> mErrScale{0};    // mWeight / (2 mH2) for the noise of the data symbols, 0 for unusable carriers
  i32 mErrCount = 0;            // number of usable carriers in mErrScale
  f32 mSnr = 0.0f;              // mean SNR of the PRS, linear
  f32 mInvRmsChannel = 0.0f;    // 1 / RMS of |mHSmooth| over all carriers (for the constellation)

  // ---- tracking within the frame
  // Phase drift since the PRS: common to all carriers, and per carrier number (timing)
  f32 mCommonPhase = 0.0f;
  f32 mTimingPhase = 0.0f;
  // Change of the timing per symbol
  f32 mTimingRate = 0.0f;
  // Noise of the data symbols relative to the noise of the PRS
  f32 mDataNoise = 1.0f;

  // ---- working vectors of demodulate()
  SimdVec<f32> mPhase{0};       // phase of _phasor()
  SimdVec<cf32> mPhasor{2};     // see _phasor()
  SimdVec<cf32> mTmpCmplx{0};
  SimdVec<cf32> mSymbol{0};     // carriers without channel and PRS
  SimdVec<cf32> mFolded{0};     // mSymbol (or mV) without the decided phases
  SimdVec<cf32> mResidual{0};   // mFolded weighted
  SimdVec<cf32> mV{0};          // mSymbol with the tracked phase and timing removed
  SimdVec<cf32> mPairLow{0};    // carrier pairs of _slope(), the unused elements stay 0
  SimdVec<cf32> mPairHigh{0};
  SimdVec<f32> mRe{0};
  SimdVec<f32> mIm{0};
  SimdVec<f32> mX{0};
  SimdVec<f32> mY{0};
  SimdVec<f32> mW{0};
  SimdVec<f32> mTmp1{2};
  SimdVec<f32> mTmp2{0};
  // Probabilities of the four absolute phases 0, pi/2, pi, 3 pi/2 of every carrier: of the current symbol (mP) and of the previous one (mQ)
  std::array<SimdVec<f32>, 4> mP{ SimdVec<f32>(0), SimdVec<f32>(0), SimdVec<f32>(0), SimdVec<f32>(0) };
  std::array<SimdVec<f32>, 4> mQ{ SimdVec<f32>(0), SimdVec<f32>(0), SimdVec<f32>(0), SimdVec<f32>(0) };
  // Probabilities of the four phase differences pi/4 + n pi/2 to the previous symbol
  std::array<SimdVec<f32>, 4> mPd{ SimdVec<f32>(0), SimdVec<f32>(0), SimdVec<f32>(0), SimdVec<f32>(0) };

  SimdVec<f32> mSoftBitRe{0};
  SimdVec<f32> mSoftBitIm{0};
  std::array<cf32, cK> mConstCarrNormed{};
  std::array<cf32, cK> mConstMeanNormed{};

  void _smooth_channel();
  void _update_weights();
  [[nodiscard]] f32 _timing_rotation(); // Phase per carrier between the new and the averaged estimate
  void _phasor(f32 iPhase, f32 iSlope, SimdVec<cf32> & oV); // exp(j (iPhase + iSlope k)) for every carrier number k
  [[nodiscard]] f32 _slope(const std::vector<i16> & iLow, const std::vector<i16> & iHigh, i32 iDistance); // timing from mResidual
  void _calc_constellation();
  void _ifft(); // normalized backward FFT from mFftInBuffer to mFftOutBuffer
};

#endif // HAVE_SSE_OR_AVX
