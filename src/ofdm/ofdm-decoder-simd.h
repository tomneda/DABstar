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
// #include "dab-params.h"
#include "glob_enums.h"
#include "freq-interleaver.h"
#include "ringbuffer.h"
#include "simd_extensions.h"
#include <QObject>
#include <cstdint>
#include <vector>
#include <volk/volk.h>

class RadioInterface;


class OfdmDecoder : public QObject
{
Q_OBJECT
public:
  OfdmDecoder(RadioInterface *, uint8_t, RingBuffer<cmplx> * iqBuffer, RingBuffer<float> * ipCarrBuffer);
  ~OfdmDecoder() override = default;

  struct SLcdData
  {
    int32_t CurOfdmSymbolNo;
    float ModQuality;
    float TestData1;
    float TestData2;
    float PhaseCorr;
    float SNR;
  };

  void reset();
  void store_null_symbol_with_tii(const std::vector<cmplx> & iFftBuffer);
  void store_null_symbol_without_tii(const std::vector<cmplx> & iFftBuffer);
  void store_reference_symbol_0(const std::vector<cmplx> & iFftBuffer);
  void decode_symbol(const std::vector<cmplx> & iFftBuffer, uint16_t iCurOfdmSymbIdx, float iPhaseCorr, std::vector<int16_t> & oBits);

  void set_select_carrier_plot_type(ECarrierPlotType iPlotType);
  void set_select_iq_plot_type(EIqPlotType iPlotType);
  void set_soft_bit_gen_type(ESoftBitType iSoftBitType);
  void set_show_nominal_carrier(bool iShowNominalCarrier);

  inline void set_dc_offset(cmplx iDcOffset) { mDcAdc = iDcOffset; };
private:

  RadioInterface * const mpRadioInterface;
  // const DabParams::SDabPar mDabPar;
  FreqInterleaver mFreqInterleaver;

  RingBuffer<cmplx> * const mpIqBuffer;
  RingBuffer<float> * const mpCarrBuffer;

  ECarrierPlotType mCarrierPlotType = ECarrierPlotType::DEFAULT;
  EIqPlotType mIqPlotType = EIqPlotType::DEFAULT;
  ESoftBitType mSoftBitType = ESoftBitType::DEFAULT;

  bool mShowNomCarrier = false;

  int32_t mShowCntStatistics = 0;
  int32_t mShowCntIqScope = 0;
  int32_t mNextShownOfdmSymbIdx = 1;
  std::vector<cmplx> mIqVector;
  std::vector<float> mCarrVector;
  std::array<int16_t, cK> mMapNomToRealCarrIdx{};
  std::array<int16_t, cK> mMapNomToFftIdx{};

  SimdVec<cmplx> mSimdVecFftBinPhaseCorr{2, 1};
  SimdVec<cmplx> mSimdVecFftBinRaw{0};
  SimdVec<cmplx> mSimdVecPhaseReferenceNormed{0};
  SimdVec<cmplx> mSimdVecNomCarrier{0};
  SimdVec<cmplx> mSimdVecPhaseReference{0};
  SimdVec<cmplx> mSimdVecTempCmplx{0};
  SimdVec<float> mSimdVecWeightPerBin{0};
  SimdVec<float> mSimdVecFftBinWrapped{0};
  SimdVec<float> mSimdVecFftBinPhaseCorrArg{0};
  SimdVec<float> mSimdVecFftBinLevel{0};
  SimdVec<float> mSimdVecFftBinPower{1};
  SimdVec<float> mSimdVecFftBinPhaseCorrReal{0};
  SimdVec<float> mSimdVecFftBinPhaseCorrImag{0};
  SimdVec<float> mSimdVecTemp1Float{2};
  SimdVec<float> mSimdVecTemp2Float{0};
  SimdVec<float> mSimdVecDecodingReal{0};
  SimdVec<float> mSimdVecDecodingImag{0};
  SimdVec<float> mSimdVecMeanNullPowerWithoutTII{1};
  SimdVec<float> mSimdVecStdDevSqPhaseVec{1};
  SimdVec<float> mSimdVecMeanLevel{1};
  SimdVec<float> mSimdVecMeanPower{1};
  SimdVec<float> mSimdVecMeanNettoPower{0};
  SimdVec<float> mSimdVecSigmaSq{3};
  SimdVec<float> mSimdVecMeanSigmaSq{1};
  SimdVec<float> mSimdVecMeanNullLevel{0};
  SimdVec<float> mSimdVecIntegAbsPhase{0};

  float mMeanPowerOvrAll = 1.0f;
  float mAbsNullLevelMin = 0.0f;
  float mAbsNullLevelGain = 0.0f;
  float mMeanValue = 1.0f;
  cmplx mDcAdc{ 0.0f, 0.0f };
  cmplx mDcFft{ 0.0f, 0.0f };
  cmplx mDcFftLast{ 0.0f, 0.0f };

  // phase correction LUT to speed up process (there are no (good) SIMD commands for that)
  static constexpr float cPhaseShiftLimit = 20.0f;

  TimeMeas mTimeMeas{"ofdm-decoder", 1000};

  // mLcdData has always be visible due to address access in another thread.
  // It isn't even thread safe but due to slow access this shouldn't be any matter
  SLcdData mLcdData{};

  [[nodiscard]] float _compute_frequency_offset(const cmplx * const & r, const cmplx * const & v) const;
  [[nodiscard]] float _compute_noise_Power() const;
  void _eval_null_symbol_statistics(const std::vector<cmplx> & iFftBuffer);
  void _reset_null_symbol_statistics();
  void _display_iq_and_carr_vectors();
  // void _volk_mean_filter(float * ioValVec, const float * iValVec, const float iAlpha) const;
  void _volk_mean_filter_sum(float & ioValSum, const float * iValVec, const float iAlpha) const;

  static cmplx _interpolate_2d_plane(const cmplx & iStart, const cmplx & iEnd, float iPar);

signals:
  void signal_slot_show_iq(int, float);
  void signal_show_lcd_data(const SLcdData *);
};

#endif // HAVE_SSE_OR_AVX
#endif
