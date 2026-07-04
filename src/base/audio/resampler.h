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
#include <algorithm>
#include <cmath>

// Stereo linear-interpolation resampler for clock-drift compensation (±5%).
//
// Converts a stream of stereo interleaved i16 samples from a slightly-off
// input rate to the nominal output rate.
//
// ratio = outputRate / inputRate  (nominally 1.0)
//   < 1.0 : input is running fast  → drop samples
//   > 1.0 : input is running slow  → stretch/interpolate
//
// For ±1% deviation linear interpolation is sufficient: worst-case aliasing
// occurs at 99% of Nyquist, well below any audible threshold.
//
class Resampler
{
public:
  explicit Resampler(f32 iRatio = 1.0f) : mRatio(iRatio) {}

  // Update ratio dynamically (e.g. from a closed-loop buffer-fill controller).
  void set_ratio(f32 iRatio) { mRatio = std::clamp(iRatio, 0.90f, 1.10f); }

  f32 get_ratio() const { return mRatio; }

  // Reset internal state. Call when the sample rate changes or on a stream gap.
  void reset()
  {
    mPhase = 0.0f;
    mPrevL = 0;
    mPrevR = 0;
  }

  // Resample a block of stereo interleaved i16 samples.
  //
  // pIn     : input buffer, interleaved L/R, length = inCount i16 values
  // inCount : number of i16 values in pIn  (must be even; inCount/2 = frames)
  // pOut    : caller-allocated output buffer
  //           required capacity: (inCount/2 + 2) * 2  i16 values
  // outCount: set by the method to the number of i16 values written
  void resample(const i16 * pIn, i32 inCount, i16 * pOut, i32 & outCount)
  {
    const i32 inFrames = inCount / 2;
    const f32 step = 1.0f / mRatio; // input advance per output sample
    i32 written = 0;
    i32 inIdx = 0; // index of the "right" boundary frame in pIn

    while (true)
    {
      // Advance phase until it is inside [0, 1).
      // Each time phase crosses 1.0 we step one input frame forward.
      while (mPhase >= 1.0f)
      {
        mPhase -= 1.0f;
        ++inIdx;
        if (inIdx >= inFrames)
        {
          // All input consumed — save last frame for next call.
          if (inFrames > 0)
          {
            mPrevL = pIn[(inFrames - 1) * 2 + 0];
            mPrevR = pIn[(inFrames - 1) * 2 + 1];
          }
          outCount = written * 2;
          return;
        }
      }

      // Left (previous) and right (current) frames bracketing mPhase.
      const i16 L0 = (inIdx == 0) ? mPrevL : pIn[(inIdx - 1) * 2 + 0];
      const i16 R0 = (inIdx == 0) ? mPrevR : pIn[(inIdx - 1) * 2 + 1];
      const i16 L1 = pIn[inIdx * 2 + 0];
      const i16 R1 = pIn[inIdx * 2 + 1];

      // Linear interpolation: out = L0 + t * (L1 - L0)
      const f32 t = mPhase;
      pOut[written * 2 + 0] = static_cast<i16>(std::lroundf((1.0f - t) * L0 + t * L1));
      pOut[written * 2 + 1] = static_cast<i16>(std::lroundf((1.0f - t) * R0 + t * R1));
      ++written;

      mPhase += step;
    }
  }

private:
  f32 mRatio = 1.0f; // outputRate / inputRate
  f32 mPhase = 0.0f; // fractional read position within current input interval [0, 1)
  i16 mPrevL = 0;    // last input frame carried across block boundaries
  i16 mPrevR = 0;
};
