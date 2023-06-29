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
#ifndef  PHASEREFERENCE_H
#define  PHASEREFERENCE_H

#include  <QObject>
#include  <cstdio>
#include  <cstdint>
#include  <vector>
#include  "fft/fft-handler.h"
#include  "phasetable.h"
#include  "dab-constants.h"
#include  "dab-params.h"
#include  "process-params.h"
#include  "ringbuffer.h"

class RadioInterface;

class PhaseReference : public QObject, public phaseTable
{
Q_OBJECT
public:
  PhaseReference(RadioInterface *, processParams *);
  ~PhaseReference() override = default;

  [[nodiscard]] int32_t find_index(std::vector<cmplx> iV, float iThreshold);
  [[nodiscard]] int16_t estimate_carrier_offset(std::vector<cmplx> v) const;
  [[nodiscard]] float phase(const std::vector<cmplx> & iV, int32_t iTs) const;

  static constexpr int16_t IDX_NOT_FOUND = 100;

private:
  const DabParams::SDabPar mDabPar;
  const int16_t mDiffLength = 128;
  const int32_t mFramesPerSecond;
  int32_t mDisplayCounter = 0;

  fftHandler mFftForward;
  fftHandler mFftBackwards;

  RingBuffer<float> * const mResponse;
  std::vector<cmplx> mRefTable;
  std::vector<float> mPhaseDifferences;
  std::vector<float> mCorrPeakValues;

signals:
  void show_correlation(int, int, const QVector<int> &);
};

#endif

