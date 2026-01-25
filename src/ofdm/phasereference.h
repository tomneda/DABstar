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

#include  "phasetable.h"
#include  "dab-constants.h"
#include  "process-params.h"
#include  "ringbuffer.h"
#include  <QObject>
#include  <cstdio>
#include  <cstdint>
#include  <vector>
#include  <fftw3.h>

class DabRadio;


class PhaseReference : public QObject, public PhaseTable
{
Q_OBJECT
public:
  PhaseReference(const DabRadio * const ipRadio, const ProcessParams * const ipParam);
  ~PhaseReference() override;

  [[nodiscard]] i32 correlate_with_phase_ref_and_find_max_peak(const TArrayTn & iV, const f32 iThreshold);
  [[nodiscard]] i32 estimate_carrier_offset_from_sync_symbol_0(const TArrayTu & iV);
  [[nodiscard]] static f32 phase(const std::vector<cf32> & iV, i32 iTs);
  void set_sync_on_strongest_peak(bool sync);

  static constexpr i32 IDX_NOT_FOUND = 100000;

private:
  static constexpr i16 SEARCHRANGE = (2 * 70);

  void CalculateRelativePhase(const cf32 *fft_in, TArrayTu & arg_out);
  std::vector<cf32> cRefArg;

  const i32 mFramesPerSecond;
  i32 mDisplayCounter = 0;
  bool mSyncOnStrongestPeak = false;

  alignas(64) TArrayTu mFftInBuffer;
  alignas(64) TArrayTu mFftOutBuffer;
  fftwf_plan mFftPlanFwd;
  fftwf_plan mFftPlanBwd;

  RingBuffer<f32> * const mpResponse;
  std::vector<f32> mCorrPeakValues;
  std::vector<f32> mMeanCorrPeakValues;

signals:
  void signal_show_correlation(f32, const QVector<i32> &);
};

#endif
