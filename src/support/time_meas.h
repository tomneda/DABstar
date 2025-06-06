/*
 * Copyright (c) 2024 by Thomas Neder (https://github.com/tomneda)
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

#ifndef TIME_MEAS_H
#define TIME_MEAS_H

#include "glob_data_types.h"
#include <iostream>
#include <chrono>
#include <assert.h>

// use steady_clock instead of high_resolution_clock
// #define USE_STEADY_CLOCK

// helper class to average several rounds
class TimeMeas
{
public:
  explicit TimeMeas(const std::string & iName, const u64 iPrintCntSteps = 1)
    : mPrintCntSteps(iPrintCntSteps)
    , mName(iName)
  {
    assert(mPrintCntSteps > 0);

#if 0
    // tests
    std::cout << _add_ticks(0) << std::endl;
    std::cout << _add_ticks(9) << std::endl;
    std::cout << _add_ticks(98) << std::endl;
    std::cout << _add_ticks(987) << std::endl;
    std::cout << _add_ticks(9876) << std::endl;
    std::cout << _add_ticks(98765)<< std::endl;
    std::cout << _add_ticks(987654) << std::endl;
    std::cout << _add_ticks(9876543) << std::endl;
    std::cout << _add_ticks(98765432) << std::endl;
    std::cout << _add_ticks(987654321) << std::endl;
    std::cout << _add_ticks(9876543219) << std::endl;
    std::cout << _add_ticks(98765432198) << std::endl;
#endif
  }
  ~TimeMeas() = default;

  inline void trigger_begin()
  {
#ifdef USE_STEADY_CLOCK
    mLastCheckedTimePoint = std::chrono::steady_clock::now();
#else
    mLastCheckedTimePoint = std::chrono::high_resolution_clock::now();
#endif
    ++mCntAbsBegin;
  }

  inline void trigger_end()
  {
    if (mCntAbsBegin == 0) return; // no trigger_begin happened yet
#ifdef USE_STEADY_CLOCK
    const auto curTimePoint = std::chrono::steady_clock::now();
#else
    const auto curTimePoint = std::chrono::high_resolution_clock::now();
#endif
    const auto lastTimeDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(curTimePoint - mLastCheckedTimePoint);
    mTimeAbs += lastTimeDuration;
    if (mTimeLocMin > lastTimeDuration) mTimeLocMin = lastTimeDuration;
    if (mTimeLocMax < lastTimeDuration) mTimeLocMax = lastTimeDuration;
    ++mCntAbsEnd;
  }

  inline void trigger_loop()
  {
    // calculate overall runtime between loops
    trigger_end();
    trigger_begin();
  }

  [[nodiscard]] inline u64 get_time_per_round_in_ns() const
  {
    assert(mCntAbsBegin == mCntAbsEnd || mCntAbsBegin == mCntAbsEnd + 1); // were trigger_begin() and trigger_end() evenly called? (loop meas is one behind)
    if (mCntAbsBegin == 0)
      return 0;

    return mTimeAbs.count() / mCntAbsBegin;
  }

  void print_time_per_round()
  {
    assert(mCntAbsBegin == mCntAbsEnd || mCntAbsBegin == mCntAbsEnd + 1); // were trigger_begin() and trigger_end() evenly called? (loop meas is one behind)

    if (mCntAbsBegin >= mCntAbsBeginPrinted)
    {
      if (mTimeAbsMin > mTimeLocMin) mTimeAbsMin = mTimeLocMin;
      if (mTimeAbsMax < mTimeLocMax) mTimeAbsMax = mTimeLocMax;

      mCntAbsBeginPrinted = mCntAbsBegin + mPrintCntSteps;
      std::cout << "Duration [ns] of " << mName
                << "  Abs["
                <<    "Avr: " << _add_ticks(mTimeAbs.count() / mCntAbsBegin)
                << ",  Min: " << _add_ticks(mTimeAbsMin.count())
                << ",  Max: " << _add_ticks(mTimeAbsMax.count())
                << ",  Cnt: " << _add_ticks(mCntAbsBegin)
                << "]  Loc["
                <<    "Avr: " << _add_ticks((mTimeAbs.count() - mTimeAbsLast.count()) / (mCntAbsBegin - mCntAbsBeginLast))
                << ",  Min: " << _add_ticks(mTimeLocMin.count())
                << ",  Max: " << _add_ticks(mTimeLocMax.count())
                << ",  Cnt: " << _add_ticks(mCntAbsBegin - mCntAbsBeginLast)
                << "]" << std::endl;

      mTimeLocMin = std::chrono::nanoseconds::max();
      mTimeLocMax = std::chrono::nanoseconds::min();
      mCntAbsBeginLast = mCntAbsBegin;
      mTimeAbsLast = mTimeAbs;
    }
  }

private:
#ifdef USE_STEADY_CLOCK
  std::chrono::steady_clock::time_point mLastCheckedTimePoint;
#else
  std::chrono::high_resolution_clock::time_point mLastCheckedTimePoint;
#endif
  std::chrono::nanoseconds mTimeAbs = std::chrono::nanoseconds::zero();
  std::chrono::nanoseconds mTimeAbsLast = std::chrono::nanoseconds::zero();
  std::chrono::nanoseconds mTimeAbsMin = std::chrono::nanoseconds::max();
  std::chrono::nanoseconds mTimeAbsMax = std::chrono::nanoseconds::min();
  std::chrono::nanoseconds mTimeLocMin = std::chrono::nanoseconds::max();
  std::chrono::nanoseconds mTimeLocMax = std::chrono::nanoseconds::min();
  u64 mCntAbsBegin = 0;
  u64 mCntAbsBeginLast = 0;
  u64 mCntAbsEnd = 0;
  u64 mCntAbsBeginPrinted = 0;
  u64 mPrintCntSteps = 1;
  std::string mName;

  std::string _add_ticks(const u64 iVal)
  {
    const std::string nsString = std::to_string(iVal);
    const size_t length = nsString.size();
    std::string outStr;
    outStr.reserve(2 * length); // very roughly

    for (size_t i = 0; i < length; ++i)
    {
      outStr += nsString[i];
      if (i < length - 1 && (length - i - 1) % 3 == 0)
      {
        outStr += '\''; // Add a tick ' on every 1000 step
      }
    }
    return outStr;
  }
};

#ifdef USE_STEADY_CLOCK
  #define time_meas_begin(m_)         const auto __ts__##m_ = std::chrono::steady_clock::now()
  #define time_meas_end(m_)           const auto __te__##m_ = std::chrono::steady_clock::now()
#else
  #define time_meas_begin(m_)         const auto __ts__##m_ = std::chrono::high_resolution_clock::now()
  #define time_meas_end(m_)           const auto __te__##m_ = std::chrono::high_resolution_clock::now()
#endif

#define time_meas_print_us(m_)        const auto __td__##m_ = std::chrono::duration_cast<std::chrono::microseconds>(__te__##m_ - __ts__##m_); \
                                      std::cout << "Duration of " #m_ ":  " << __td__##m_.count() << " us" << std::endl
#define time_meas_print_ns(m_)        const auto __td__##m_ = std::chrono::duration_cast<std::chrono::nanoseconds>(__te__##m_ - __ts__##m_); \
                                      std::cout << "Duration of " #m_ ":  " << __td__##m_.count() << " ns" << std::endl
#define time_meas_end_print_us(m_)    time_meas_end(m_); time_meas_print_us(m_)
#define time_meas_end_print_ns(m_)    time_meas_end(m_); time_meas_print_ns(m_)

#endif // TIME_MEAS_H
