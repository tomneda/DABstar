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
#ifndef OFDM_DECODER_SIMD_H
#define OFDM_DECODER_SIMD_H
#include "time_meas.h"

#ifdef HAVE_SSE_OR_AVX

#include "dab-constants.h"
#include "glob_enums.h"
#include "freq-interleaver.h"
#include "ringbuffer.h"
#include "simd_extensions.h"
#include <QObject>
#include <cstdint>
#include <vector>
#include <volk/volk.h>

class DabRadio;


class OfdmDecoder : public QObject
{
Q_OBJECT
public:
  OfdmDecoder(DabRadio *, RingBuffer<cf32> * iqBuffer, RingBuffer<f32> * ipCarrBuffer);
  ~OfdmDecoder() override = default;

  struct SLcdData
  {
    i32 CurOfdmSymbolNo;
    f32 ModQuality;
    f32 TestData1;
    f32 TestData2;
    f32 MeanSigmaSqFreqCorr;
    f32 SNR;
  };

  void reset();
  void store_null_symbol_with_tii(const TArrayTu & iV);
  void store_null_symbol_without_tii(const TArrayTu & iV);
  void store_reference_symbol_0(const TArrayTu & iFftBuffer);
  void decode_symbol(const TArrayTu & iV, const u16 iCurOfdmSymbIdx, const f32 iPhaseCorr, const f32 iClockErr, std::vector<i16> & oBits);

  void set_select_carrier_plot_type(ECarrierPlotType iPlotType);
  void set_select_iq_plot_type(EIqPlotType iPlotType);
  void set_soft_bit_gen_type(ESoftBitType iSoftBitType);
  void set_show_nominal_carrier(bool iShowNominalCarrier);

  inline void set_dc_offset(cf32 iDcOffset) { mDcAdc = iDcOffset; };
private:

  DabRadio * const mpRadioInterface;
  FreqInterleaver mFreqInterleaver;

  RingBuffer<cf32> * const mpIqBuffer;
  RingBuffer<f32> * const mpCarrBuffer;

  ECarrierPlotType mCarrierPlotType = ECarrierPlotType::DEFAULT;
  EIqPlotType mIqPlotType = EIqPlotType::DEFAULT;
  ESoftBitType mSoftBitType = ESoftBitType::DEFAULT;

  bool mShowNomCarrier = false;

  i32 mShowCntStatistics = 0;
  i32 mShowCntIqScope = 0;
  i32 mNextShownOfdmSymbIdx = 1;
  std::vector<cf32> mIqVector;
  std::vector<f32> mCarrVector;
  std::array<i16, cK> mMapNomToRealCarrIdx{};
  std::array<i16, cK> mMapNomToFftIdx{};

  SimdVec<cf32> mSimdVecFftBinPhaseCorr{2, 1};
  SimdVec<cf32> mSimdVecFftBinRaw{0};
  SimdVec<cf32> mSimdVecPhaseReferenceNormed{0};
  SimdVec<cf32> mSimdVecNomCarrier{0};
  SimdVec<cf32> mSimdVecPhaseReference{0};
  SimdVec<cf32> mSimdVecTempCmplx{0};
  SimdVec<f32> mSimdVecWeightPerBin{0};
  SimdVec<f32> mSimdVecFftBinWrapped{0};
  SimdVec<f32> mSimdVecFftBinPhaseCorrArg{0};
  SimdVec<f32> mSimdVecFftBinLevel{0};
  SimdVec<f32> mSimdVecFftBinPower{1};
  SimdVec<f32> mSimdVecFftBinPhaseCorrReal{0};
  SimdVec<f32> mSimdVecFftBinPhaseCorrImag{0};
  SimdVec<f32> mSimdVecTemp1Float{2};
  SimdVec<f32> mSimdVecTemp2Float{0};
  SimdVec<f32> mSimdVecDecodingReal{0};
  SimdVec<f32> mSimdVecDecodingImag{0};
  SimdVec<f32> mSimdVecMeanNullPowerWithoutTII{1};
  SimdVec<f32> mSimdVecStdDevSqPhaseVec{1};
  SimdVec<f32> mSimdVecMeanLevel{1};
  SimdVec<f32> mSimdVecMeanPower{1};
  SimdVec<f32> mSimdVecMeanNettoPower{0};
  SimdVec<f32> mSimdVecSigmaSq{3};
  SimdVec<f32> mSimdVecMeanSigmaSq{1};
  SimdVec<f32> mSimdVecMeanNullLevel{0};
  SimdVec<f32> mSimdVecIntegAbsPhase{0};
  SimdVec<f32> mSimdVecPhaseErr{0};

  f32 mMeanPowerOvrAll = 1.0f;
  f32 mAbsNullLevelMin = 0.0f;
  f32 mAbsNullLevelGain = 0.0f;
  f32 mMeanValue = 1.0f;
  f32 mMeanSigmaSqFreqCorr = 0.0f;
  cf32 mDcAdc{ 0.0f, 0.0f };
  cf32 mDcFft{ 0.0f, 0.0f };
  cf32 mDcFftLast{ 0.0f, 0.0f };

  // phase correction LUT to speed up process (there are no (good) SIMD commands for that)
  static constexpr f32 cPhaseShiftLimit = 20.0f;

  TimeMeas mTimeMeas{"ofdm-decoder", 1000};

  // mLcdData has always be visible due to address access in another thread.
  // It isn't even thread safe but due to slow access this shouldn't be any matter
  SLcdData mLcdData{};

  [[nodiscard]] f32 _compute_noise_Power() const;
  void _eval_null_symbol_statistics(const TArrayTu & iV);
  void _reset_null_symbol_statistics();
  void _display_iq_and_carr_vectors();
  // void _volk_mean_filter(f32 * ioValVec, const f32 * iValVec, const f32 iAlpha) const;
  void _volk_mean_filter_sum(f32 & ioValSum, const f32 * iValVec, const f32 iAlpha) const;

  static cf32 _interpolate_2d_plane(const cf32 & iStart, const cf32 & iEnd, f32 iPar);

signals:
  void signal_slot_show_iq(i32, f32);
  void signal_show_lcd_data(const SLcdData *);
};

#endif // HAVE_SSE_OR_AVX
#endif
