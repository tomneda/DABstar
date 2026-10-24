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

#include "dab_constants.h"
#include "phasetable.h"
#include <fftw3.h>
#include <array>

/*
 * Coherent demodulation of the DQPSK carriers, an alternative to the differential demodulation in OfdmDecoder for weak,
 * stationary signals.
 *
 * The channel (amplitude and phase of every carrier) is estimated from the phase reference symbol (PRS), averaged over
 * frames and smoothed in the time domain (the impulse response is shorter than the guard interval). Before averaging,
 * the estimate of the previous frames is turned to the timing and the phase of the current frame. Within the frame,
 * the phase drift (frequency offset) and the timing drift (sample clock) are tracked from symbol to symbol. The data
 * symbols are compared with this almost noise-free reference instead of with the noisy previous symbol, and the soft
 * bits of the phase difference are computed from the probabilities of the absolute phases of both symbols. The FIC
 * and the audio need up to 2 dB less SNR than with the differential demodulation (more with echoes). A channel that
 * changes within a frame (mobile reception) makes it worse.
 */
class CoherentDemod : private PhaseTable
{
public:
  CoherentDemod();
  ~CoherentDemod();

  // Forget the channel estimate (new channel, sync lost, mode switched)
  void reset();

  // iV: the PRS in FFT order (carrier k at index k mod cTu). Updates the channel estimate for the following data symbols of this frame.
  void process_prs(const TArrayTu & iV);

  // True once the channel estimate and the noise are known
  [[nodiscard]] bool is_ready() const { return mFrames >= 2; }

  // iV: data symbol iCurOfdmSymbIdx (1 .. cL-1) of the frame of the last PRS,
  // iGuardCorrSum: sum of x[n + cTu] * conj(x[n]) over the guard intervals (n = 0 .. cTg-1) of all data symbols of
  // this frame up to this one, for the frequency offset.
  // Afterwards get_llr_real(fftIdx) and get_llr_imag(fftIdx) are the log-likelihood ratios of the two bits of the
  // carrier in FFT bin fftIdx (> 0: bit 0), and get_decision(fftIdx) its phase difference to the previous symbol
  // (for the constellation diagram).
  void demodulate(const TArrayTu & iV, i32 iCurOfdmSymbIdx, const cf32 & iGuardCorrSum);

  [[nodiscard]] f32 get_llr_real(i32 iFftIdx) const { return mLlrRe[iFftIdx]; }
  [[nodiscard]] f32 get_llr_imag(i32 iFftIdx) const { return mLlrIm[iFftIdx]; }
  [[nodiscard]] cf32 get_decision(i32 iFftIdx) const { return mDecisions[iFftIdx]; }

private:
  using TArrayK = std::array<cf32, cK>;

  // Carrier number k and FFT bin of carrier c = 0 .. cK-1
  std::array<i32, cK> mCarrier{};
  std::array<i32, cK> mFftBin{};
  TArrayK mPrs{};              // PRS of every carrier

  i32 mFrames = 0;
  TArrayK mHRaw{};             // estimate of the last PRS only
  TArrayK mHAvg{};             // averaged over frames
  TArrayK mHSmooth{};          // mHAvg smoothed in the time domain
  TArrayK mReference{};        // conj(mHSmooth * mPrs)
  std::array<f32, cK> mNoiseVar{}; // noise power of every carrier
  std::array<f32, cK> mWeight{};   // 2 / mNoiseVar, smoothed
  TArrayK mCross{};            // mHRaw * conj(mHAvg)
  f32 mSnr = 0.0f;             // mean SNR of the PRS, linear

  alignas(64) TArrayTu mFftInBuffer;
  alignas(64) TArrayTu mFftOutBuffer;
  fftwf_plan mFftPlanFwd;
  fftwf_plan mFftPlanBwd;

  // working buffers of _smooth_channel()
  std::array<f32, cTu> mTapPower{};
  std::array<f32, cTu> mTapPowerSorted{};
  std::array<bool, cTu> mTapInWindow{};

  // Per carrier: probabilities of the four absolute phases of the previous symbol
  std::array<std::array<f32, 4>, cK> mPrevious{};
  // Phase drift since the PRS: common to all carriers, and per carrier number (timing)
  f32 mCommonPhase = 0.0f;
  f32 mTimingPhase = 0.0f;
  // Change of the timing per symbol
  f32 mTimingRate = 0.0f;
  // Noise of the data symbols relative to the noise of the PRS
  f32 mDataNoise = 1.0f;
  TArrayK mSymbol{};           // carriers without channel and PRS
  TArrayK mResidual{};         // mSymbol without the decided phases
  TArrayK mRotations{};        // see _rotation()

  std::array<f32, cTu> mLlrRe{};
  std::array<f32, cTu> mLlrIm{};
  TArrayTu mDecisions{};

  void _smooth_channel();
  void _rotation(f32 iPhase, f32 iSlope, TArrayK & oV) const;
  [[nodiscard]] f32 _timing_rotation(); // Phase per carrier between the new and the averaged estimate
  void _ifft(); // normalized backward FFT from mFftInBuffer to mFftOutBuffer
};
