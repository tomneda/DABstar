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
 *    This file is part of the Qt-DAB program
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
 */
#ifndef OFDM_DECODER_H
#define OFDM_DECODER_H

#ifndef HAVE_SSE_OR_AVX

#include "dab-constants.h"
#include "glob_enums.h"
#include "freq-interleaver.h"
#include "ringbuffer.h"
#include <QObject>
#include <cstdint>
#include <vector>

#define USE_PHASE_CORR_LUT

class RadioInterface;

class OfdmDecoder : public QObject
{
Q_OBJECT
public:
  OfdmDecoder(RadioInterface *, uint8_t, RingBuffer<cmplx> * iqBuffer, RingBuffer<float> * ipCarrBuffer);
  ~OfdmDecoder() override;

  struct SLcdData
  {
    int32_t CurOfdmSymbolNo;
    float PhaseCorr;
    float SNR;
    float ModQuality;
    float TestData1;
    float TestData2;
  };

  void reset();
  void store_null_symbol_with_tii(const TArrayTu &);
  void store_null_symbol_without_tii(const TArrayTu &);
  void store_reference_symbol_0(const std::vector<cmplx> &);
  void decode_symbol(const TArrayTu & iV, const uint16_t iCurOfdmSymbIdx, const float iPhaseCorr, std::vector<int16_t> & oBits);

  void set_select_carrier_plot_type(ECarrierPlotType iPlotType);
  void set_select_iq_plot_type(EIqPlotType iPlotType);
  void set_soft_bit_gen_type(ESoftBitType iSoftBitType);
  void set_show_nominal_carrier(bool iShowNominalCarrier);

  inline void set_dc_offset(cmplx iDcOffset) { mDcAdc = iDcOffset; };

private:
  RadioInterface * const mpRadioInterface;
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
  std::vector<cmplx> mPhaseReference;
  std::vector<cmplx> mIqVector;
  std::vector<float> mCarrVector;
  std::vector<float> mStdDevSqPhaseVector;
  std::vector<float> mIntegAbsPhaseVector;
  std::vector<float> mMeanPhaseVector;
  std::vector<float> mMeanLevelVector;
  std::vector<float> mMeanSigmaSqVector;
  std::vector<float> mMeanPowerVector;
  std::vector<float> mMeanNullLevel;
  std::vector<float> mMeanNullPowerWithoutTII;
  float mMeanPowerOvrAll = 1.0f;
  float mAbsNullLevelMin = 0.0f;
  float mAbsNullLevelGain = 0.0f;
  float mMeanValue = 1.0f;
  cmplx mDcAdc{ 0.0f, 0.0f };

  // phase correction LUT to speed up process (there are no (good) SIMD commands for that)
  static constexpr float cPhaseShiftLimit = 20.0f;
#ifdef USE_PHASE_CORR_LUT
  static constexpr int32_t cLutLen2 = 127; // -> 255 values in LUT
  static constexpr float cLutFact = cLutLen2 / (F_RAD_PER_DEG * cPhaseShiftLimit);
  std::array<cmplx, cLutLen2 * 2 + 1> mLutPhase2Cmplx;
#endif

  // mLcdData has always be visible due to address access in another thread.
  // It isn't even thread safe but due to slow access this shouldn't be any matter
  SLcdData mLcdData{};

  [[nodiscard]] float _compute_frequency_offset(const std::vector<cmplx> & r, const std::vector<cmplx> & c) const;
  [[nodiscard]] float _compute_noise_Power() const;
  void _eval_null_symbol_statistics(const TArrayTu &);
  void _reset_null_symbol_statistics();
#ifndef USE_PHASE_CORR_LUT
  cmplx cmplx_from_phase2(const float iPhase);
#endif

  static cmplx _interpolate_2d_plane(const cmplx & iStart, const cmplx & iEnd, float iPar);

signals:
  void signal_slot_show_iq(int, float);
  void signal_show_lcd_data(const SLcdData *);
};

#endif // HAVE_SSE_OR_AVX
#endif
