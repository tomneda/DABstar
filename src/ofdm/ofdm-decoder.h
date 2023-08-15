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
#include "fft/fft-handler.h"
#include "freq-interleaver.h"
#include "ringbuffer.h"
#include <QObject>
#include <cstdint>
#include <vector>

class RadioInterface;

class OfdmDecoder : public QObject
{
Q_OBJECT
public:
  OfdmDecoder(RadioInterface *, uint8_t, RingBuffer<cmplx> * iqBuffer);
  ~OfdmDecoder() override = default;

  struct SQualityData
  {
    int32_t CurOfdmSymbolNo;
    float StdDeviation;
    float TimeOffset;
    float FreqOffset;
    float PhaseCorr;
  };

  void processBlock_0(std::vector<cmplx>);  // copy of vector is intended
  void decode(const std::vector<cmplx> & buffer, uint16_t iCurOfdmSymbIdx, float iPhaseCorr, std::vector<int16_t> & oBits);

private:
  RadioInterface * const mpRadioInterface;
  const DabParams::SDabPar mDabPar;
  FreqInterleaver mFreqInterleaver;
  fftHandler mFftHandler;
  RingBuffer<cmplx> * const mpIqBuffer;

  int32_t mShowCntStatistics = 0;
  int32_t mShowCntIqScope = 0;
  int32_t mNextShownOfdmSymbIdx = 1;
  std::vector<cmplx> mPhaseReference;
  std::vector<cmplx> mFftBuffer;
  std::vector<cmplx> mDataVector;

  // mQD has always be visible due to address access in another thread.
  // It isn't even thread safe but due to slow access this shouldn't be any matter
  SQualityData mQD{};

  float compute_mod_quality(const std::vector<cmplx> & v) const;
  float compute_time_offset(const std::vector<cmplx> & r, const std::vector<cmplx> & v) const;
  float compute_clock_offset(const cmplx * r, const cmplx * v) const;
  float compute_frequency_offset(const std::vector<cmplx> & r, const std::vector<cmplx> & c) const;

signals:
  void showIQ(int);
  void showQuality(const SQualityData *);
};

#endif
