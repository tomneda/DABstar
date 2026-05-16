/*
 * Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
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
#pragma once

#include "glob_defs.h"

class TestTone
{
public:
  void set_sample_rate(u32 iSampleRateKHz);
  void activate(bool iActive);
  void process(i16 * iopData, i32 iNumSamples);

private:
  static constexpr f32 cTimePeriod = 2.0f;
  static constexpr f32 cStartWaitingTime = 0.25f;
  static constexpr f32 cSignalDuration = 0.025f;
  static constexpr f32 cToneFreqHz = 1000.0f;
  static constexpr f32 cLevel = 0.9f;

  bool mEnabled = false;
  u32  mTimePeriodCounter = 0;
  u32  mNoSampleRemain = 0;
  f32  mCurPhase = 0.0f;
  f32  mPhaseIncr = 0.0f;
  f32  mSampleRateHz = 0.0f;
};
