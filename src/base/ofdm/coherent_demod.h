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
 * The original copyright information is acknowledged.
 */
#pragma once

#include "dab_constants.h"
#include "phasetable.h"
#include <fftw3.h>
#include <array>

/*
 * Coherent demodulation of the DQPSK carriers, an alternative to the differential demodulation in OfdmDecoder for weak,
 * stationary signals. Two methods are available:
 *
 * EMethod::FRAME_AVERAGE (welle.io):
 * The channel (amplitude and phase of every carrier) is estimated from the phase reference symbol (PRS), averaged over
 * frames and smoothed in the time domain (the impulse response is shorter than the guard interval). Before averaging,
 * the estimate of the previous frames is turned to the timing and the phase of the current frame. Within the frame,
 * the phase drift (frequency offset) and the timing drift (sample clock) are tracked from symbol to symbol. The data
 * symbols are compared with this almost noise-free reference instead of with the noisy previous symbol, and the soft
 * bits of the phase difference are computed from the probabilities of the absolute phases of both symbols. The FIC
 * and the audio need up to 2 dB less SNR than with the differential demodulation (more with echoes). A channel that
 * changes within a frame (mobile reception) makes it worse.
 *
 * EMethod::DECISION_DIRECTED (OnAir):
 * The channel is estimated from the PRS of the current frame only and then again from every data symbol with the
 * decided carriers taken out (predicted linearly from the last two estimates for the decision). Every estimate is
 * cleaned in the time domain: only the taps of the impulse response that stand out of the noise in the PRS are kept.
 * The soft bits of the phase difference d come from both received symbols (log-MAP over the unknown absolute phase s
 * of the previous symbol):
 *   M(d) = max*_s Re(conj(s) (conj(H_{l-1}) y_{l-1} + conj(H_l) y_l conj(d)))
 * which turns into differential detection when the estimate is poor and into coherent detection when it is good.
 * A channel that changed too fast within the last frame (Doppler of some 40 Hz and more) lets OfdmDecoder use the
 * differential demodulation for the next frame.
 */
class CoherentDemod : private PhaseTable
{
public:
  enum class EMethod
  {
    FRAME_AVERAGE,     // welle.io: channel estimate averaged over frames, phase and timing drift tracked within the frame
    DECISION_DIRECTED  // OnAir: channel estimate from the PRS, re-estimated from every data symbol with the decided carriers
  };

  CoherentDemod();
  ~CoherentDemod();

  // Select the method, this also forgets the channel estimate
  void set_method(EMethod iMethod);

  // Forget the channel estimate (new channel, sync lost, mode switched)
  void reset();

  // iV: the PRS in FFT order (carrier k at index k mod cTu). Updates the channel estimate for the following data symbols of this frame.
  void process_prs(const TArrayTu & iV);

  // True once the channel estimate is usable for the soft bits of the data symbols of this frame
  [[nodiscard]] bool is_ready() const;

  // iV: data symbol iCurOfdmSymbIdx (1 .. cL-1) of the frame of the last PRS,
  // iGuardCorrSum: sum of x[n + cTu] * conj(x[n]) over the guard intervals (n = 0 .. cTg-1) of all data symbols of
  // this frame up to this one, for the frequency offset (used by EMethod::FRAME_AVERAGE only).
  // Has to be called for every data symbol of the frame (also if is_ready() is false, so that the channel can be tracked).
  // Afterwards (if is_ready()) get_soft_bit_real(fftIdx) and get_soft_bit_imag(fftIdx) are the soft bits of the two bits
  // of the carrier in FFT bin fftIdx, scaled to the soft bit range but not limited (> 0: bit 1), and get_decision(fftIdx)
  // its phase difference to the previous symbol (for the constellation diagram).
  void demodulate(const TArrayTu & iV, i32 iCurOfdmSymbIdx, const cf32 & iGuardCorrSum);

  [[nodiscard]] f32 get_soft_bit_real(i32 iFftIdx) const { return mSoftBitRe[iFftIdx]; }
  [[nodiscard]] f32 get_soft_bit_imag(i32 iFftIdx) const { return mSoftBitIm[iFftIdx]; }
  [[nodiscard]] cf32 get_decision(i32 iFftIdx) const { return mDecisions[iFftIdx]; }

private:
  using TArrayK = std::array<cf32, cK>;

  EMethod mMethod = EMethod::FRAME_AVERAGE;

  // Carrier number k and FFT bin of carrier c = 0 .. cK-1
  std::array<i32, cK> mCarrier{};
  std::array<i32, cK> mFftBin{};
  TArrayK mPrs{};              // PRS of every carrier

  alignas(64) TArrayTu mFftInBuffer;
  alignas(64) TArrayTu mFftOutBuffer;
  fftwf_plan mFftPlanFwd;
  fftwf_plan mFftPlanBwd;

  // working buffers of _smooth_channel() and _process_prs_decision_directed()
  std::array<f32, cTu> mTapPower{};
  std::array<f32, cTu> mTapPowerSorted{};
  std::array<bool, cTu> mTapInWindow{};

  std::array<f32, cTu> mSoftBitRe{};
  std::array<f32, cTu> mSoftBitIm{};
  TArrayTu mDecisions{};

  // ---- EMethod::FRAME_AVERAGE
  i32 mFrames = 0;
  TArrayK mHRaw{};             // estimate of the last PRS only
  TArrayK mHAvg{};             // averaged over frames
  TArrayK mHSmooth{};          // mHAvg smoothed in the time domain
  TArrayK mReference{};        // conj(mHSmooth * mPrs)
  std::array<f32, cK> mNoiseVar{}; // noise power of every carrier
  std::array<f32, cK> mWeight{};   // 2 / mNoiseVar, smoothed
  TArrayK mCross{};            // mHRaw * conj(mHAvg)
  f32 mSnr = 0.0f;             // mean SNR of the PRS, linear

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

  // ---- EMethod::DECISION_DIRECTED
  bool mTapMaskValid = false;  // the impulse response of the PRS of this frame stands out of the noise
  bool mChannelSlow = false;   // the channel changed slowly enough within the last complete frame
  i32 mNextOfdmSymbIdx = 0;    // the data symbol expected next (the symbols have to come without a gap)
  std::array<f32, cTu> mTapMask{}; // 1 / cTu for the kept taps, else 0
  TArrayK mRawEst{};           // raw estimate of the channel (carrier times the conjugated decided symbol)
  TArrayK mHCur{};             // cleaned estimate of the previous symbol
  TArrayK mHPrev{};            // cleaned estimate of the symbol before that (for the linear prediction)
  TArrayK mHNew{};             // cleaned estimate of the current symbol
  TArrayK mYPrev{};            // carriers of the previous symbol
  std::array<f32, 65> mMaxStarTable{}; // correction log(1 + exp(-d)) of _max_star() in steps of 1/8
  f64 mLlrSum = 0.0;           // sum of |LLR| over the current frame
  f64 mDeltaHSum = 0.0;        // sum of |H_l - H_{l-1}|^2 over the current frame
  f64 mPowerHSum = 0.0;        // sum of |H_{l-1}|^2 over the current frame
  f32 mLlrRef = 0.0f;          // slow average of the mean |LLR| over the frames with a slow channel (the scale of the soft bits)

  void _process_prs_frame_average(const TArrayTu & iV);
  void _demodulate_frame_average(const TArrayTu & iV, i32 iCurOfdmSymbIdx, const cf32 & iGuardCorrSum);
  void _smooth_channel();
  void _rotation(f32 iPhase, f32 iSlope, TArrayK & oV) const;
  [[nodiscard]] f32 _timing_rotation(); // Phase per carrier between the new and the averaged estimate

  void _process_prs_decision_directed(const TArrayTu & iV);
  void _demodulate_decision_directed(const TArrayTu & iV, i32 iCurOfdmSymbIdx);
  void _clean_estimate(const TArrayK & iE, TArrayK & oH); // keep only the masked taps of the impulse response of iE
  [[nodiscard]] f32 _max_star(f32 iA, f32 iB) const;     // log(exp(iA) + exp(iB))

  void _ifft(); // normalized backward FFT from mFftInBuffer to mFftOutBuffer
};
