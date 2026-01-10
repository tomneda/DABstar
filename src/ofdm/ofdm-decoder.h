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

class DabRadio;

class OfdmDecoder : public QObject
{
Q_OBJECT
public:
  OfdmDecoder(DabRadio *, RingBuffer<cf32> * iqBuffer, RingBuffer<f32> * ipCarrBuffer);
  ~OfdmDecoder() override;

  struct SLcdData
  {
    i32 CurOfdmSymbolNo;
    f32 MeanSigmaSqFreqCorr;
    f32 SNR;
    f32 ModQuality;
    f32 TestData1;
    f32 TestData2;
  };

  void reset();
  void store_null_symbol_with_tii(const TArrayTu &);
  void store_null_symbol_without_tii(const TArrayTu &);
  void store_reference_symbol_0(const TArrayTu &);
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
  std::vector<cf32> mPhaseReference;
  std::vector<cf32> mIqVector;
  std::vector<f32> mCarrVector;
  std::vector<f32> mStdDevSqPhaseVector;
  std::vector<f32> mIntegAbsPhaseVector;
  std::vector<f32> mMeanPhaseVector;
  std::vector<f32> mMeanSigmaSqVector;
  std::vector<f32> mMeanPowerVector;
  std::vector<f32> mMeanNullLevel;
  std::vector<f32> mMeanNullPowerWithoutTII;
  f32 mMeanPowerOvrAll = 1.0f;
  f32 mAbsNullLevelMin = 0.0f;
  f32 mAbsNullLevelGain = 0.0f;
  f32 mMeanValue = 1.0f;
  f32 mMeanSigmaSqFreqCorr = 0.0f;
  cf32 mDcAdc{ 0.0f, 0.0f };

  // mLcdData has always be visible due to address access in another thread.
  // It isn't even thread safe but due to slow access this shouldn't be any matter
  SLcdData mLcdData{};

  [[nodiscard]] f32 _compute_noise_Power() const;
  void _eval_null_symbol_statistics(const TArrayTu &);
  void _reset_null_symbol_statistics();
  cf32 cmplx_from_phase2(const f32 iPhase);

  static cf32 _interpolate_2d_plane(const cf32 & iStart, const cf32 & iEnd, f32 iPar);

signals:
  void signal_slot_show_iq(i32, f32);
  void signal_show_lcd_data(const SLcdData *);
};

#endif // HAVE_SSE_OR_AVX
#endif
