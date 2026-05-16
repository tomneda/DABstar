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

#include "test_tone.h"
#include "dab-constants.h"

void TestTone::set_sample_rate(const u32 iSampleRateKHz)
{
  mSampleRateHz = (f32)iSampleRateKHz * 1000.0f;
}

void TestTone::activate(const bool iActive)
{
  assert(cTimePeriod >= cStartWaitingTime);
  mNoSampleRemain = 0;
  mTimePeriodCounter = (u32)(mSampleRateHz * (cTimePeriod - cStartWaitingTime)); // let the tone start with cStartWaitingTime delay
  mEnabled = iActive;
}

void TestTone::process(i16 * const iopData, const i32 iNumSamples)
{
  if (!mEnabled)
  {
    return;
  }

  for (i32 idx = 0; idx < iNumSamples; idx += 2)
  {
    i16 & left  = iopData[idx + 0];
    i16 & right = iopData[idx + 1];
    left  *= (1.0f - cLevel);
    right *= (1.0f - cLevel);

    if (mNoSampleRemain > 0)
    {
      mNoSampleRemain--;
      mCurPhase += mPhaseIncr;
      mCurPhase = constrain_pi(mCurPhase);
      const i16 smpl = std::lround(std::sin(mCurPhase) * cLevel * 0x7FFF);
      left  += smpl;
      right += smpl;
    }
    else if (++mTimePeriodCounter > mSampleRateHz * cTimePeriod)
    {
      mTimePeriodCounter = 0;
      mNoSampleRemain = (u32)(mSampleRateHz * cSignalDuration);
      mCurPhase = 0.0f;
      mPhaseIncr = F_2_M_PI / mSampleRateHz * cToneFreqHz;
    }
  }
}
