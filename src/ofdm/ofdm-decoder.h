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

#include "dab-constants.h"
#include "dab-params.h"
#include "glob_enums.h"
#include "fft-handler.h"
#include "freq-interleaver.h"
#include "ringbuffer.h"
#include "qwt_defs.h"
#include <QObject>
#include <cstdint>
#include <vector>

class RadioInterface;

class OfdmDecoder : public QObject
{
Q_OBJECT
public:
  OfdmDecoder(RadioInterface *, uint8_t, RingBuffer<cmplx> * iqBuffer, RingBuffer<TQwtData> * ipCarrBuffer);
  ~OfdmDecoder() override = default;

  struct SQualityData
  {
    int32_t CurOfdmSymbolNo;
    float ModQuality;
    float TimeOffset;
    float FreqOffset;
    float PhaseCorr;
    float SNR;
  };

  void reset();
  void store_null_symbol_with_tii(const std::vector<cmplx> &);
  void store_null_symbol_without_tii(const std::vector<cmplx> &);
  void store_reference_symbol_0(std::vector<cmplx>);  // copy of vector is intended
  void decode_symbol(const std::vector<cmplx> & buffer, uint16_t iCurOfdmSymbIdx, float iPhaseCorr, std::vector<int16_t> & oBits);

  void set_select_carrier_plot_type(ECarrierPlotType iPlotType);
  void set_select_iq_plot_type(EIqPlotType iPlotType);
  void set_soft_bit_gen_type(ESoftBitType iSoftBitType);
  void set_show_nominal_carrier(bool iShowNominalCarrier);

  inline void set_dc_offset(cmplx iDcOffset) { mDcAdc = iDcOffset; };

private:
  RadioInterface * const mpRadioInterface;
  const DabParams::SDabPar mDabPar;
  FreqInterleaver mFreqInterleaver;
  FftHandler mFftHandler;
  RingBuffer<cmplx> * const mpIqBuffer;
  RingBuffer<TQwtData> * const mpCarrBuffer;

  ECarrierPlotType mCarrierPlotType = ECarrierPlotType::DEFAULT;
  EIqPlotType mIqPlotType = EIqPlotType::DEFAULT;
  ESoftBitType mSoftBitType = ESoftBitType::DEFAULT;

  bool mShowNomCarrier = false;

  int32_t mShowCntStatistics = 0;
  int32_t mShowCntIqScope = 0;
  int32_t mNextShownOfdmSymbIdx = 1;
  std::vector<cmplx> mPhaseReference;
  std::vector<cmplx> mFftBuffer;
  std::vector<cmplx> mIqVector;
  std::vector<TQwtData> mCarrVector;
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

  // mQD has always be visible due to address access in another thread.
  // It isn't even thread safe but due to slow access this shouldn't be any matter
  SQualityData mQD{};

  [[nodiscard]] float _compute_mod_quality(const std::vector<cmplx> & v) const;
  [[nodiscard]] float _compute_time_offset(const std::vector<cmplx> & r, const std::vector<cmplx> & v) const;
  [[nodiscard]] float _compute_clock_offset(const cmplx * r, const cmplx * v) const;
  [[nodiscard]] float _compute_frequency_offset(const std::vector<cmplx> & r, const std::vector<cmplx> & c) const;
  [[nodiscard]] float _compute_noise_Power() const;
  void _eval_null_symbol_statistics();
  void _reset_null_symbol_statistics();

  static cmplx _interpolate_2d_plane(const cmplx & iStart, const cmplx & iEnd, float iPar);

signals:
  void signal_slot_show_iq(int, float);
  void signal_show_mod_quality(const SQualityData *);
};

#endif
