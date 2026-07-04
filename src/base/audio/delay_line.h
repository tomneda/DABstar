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

#include "glob_data_types.h"
#include <vector>

template <typename T>
class DelayLine
{
public:
  explicit DelayLine(const T & iDefault)
    : mDefault(iDefault)
  {
    set_delay_steps(0); // reserve memory for at least one sample
  }

  void set_delay_steps(const i32 iSteps)
  {
    mDataPtrIdx = 0;
    mDelayBuffer.resize(iSteps + 1);
    reset();
  }

  const T & get_set_value(const T & iVal)
  {
    mDelayBuffer[mDataPtrIdx] = iVal;
    mDataPtrIdx = (mDataPtrIdx + 1) % mDelayBuffer.size();
    return mDelayBuffer[mDataPtrIdx];
  }

  void reset()
  {
    std::fill(mDelayBuffer.begin(), mDelayBuffer.end(), mDefault);
  }

private:
  u32 mDataPtrIdx = 0;
  std::vector<T> mDelayBuffer;
  T mDefault;
};
