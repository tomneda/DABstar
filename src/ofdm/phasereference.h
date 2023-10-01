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

class PhaseReference : public QObject, public PhaseTable
{
Q_OBJECT
public:
  PhaseReference(const RadioInterface * const ipRadio, const ProcessParams * const ipParam);
  ~PhaseReference() override = default;

  [[nodiscard]] int32_t correlate_with_phase_ref_and_find_max_peak(std::vector<cmplx> iV, float iThreshold);  // copy of iV is intended
  [[nodiscard]] int16_t estimate_carrier_offset_from_sync_symbol_0(std::vector<cmplx> iV);       // copy of iV is intended
  [[nodiscard]] static float phase(const std::vector<cmplx> & iV, int32_t iTs) ;

  static constexpr int16_t IDX_NOT_FOUND = 10000;

private:
  static constexpr int16_t DIFFLENGTH = 128;
  static constexpr int16_t SEARCHRANGE = (2 * 35);

  std::array<float, SEARCHRANGE + DIFFLENGTH + 1> mComputedDiffs{};
  std::array<float, DIFFLENGTH> mPhaseDifferences;

  const DabParams::SDabPar mDabPar;
  const int32_t mFramesPerSecond;
  int32_t mDisplayCounter = 0;

  fftHandler mFftForward;
  fftHandler mFftBackwards;

  RingBuffer<float> * const mpResponse;
  std::vector<cmplx> mRefTable;
  std::vector<float> mCorrPeakValues;

signals:
  void show_correlation(int, int, const QVector<int> &);
};

#endif

