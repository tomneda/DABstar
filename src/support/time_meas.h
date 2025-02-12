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

#include <iostream>
#include <chrono>
#include <assert.h>

// use steady_clock instead of high_resolution_clock
// #define USE_STEADY_CLOCK

// helper class to average several rounds
class TimeMeas
{
public:
  explicit TimeMeas(const std::string & iName, const uint64_t iPrintCntSteps = 1)
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
    if (mTimeMin > lastTimeDuration) mTimeMin = lastTimeDuration;
    if (mTimeMax < lastTimeDuration) mTimeMax = lastTimeDuration;
    ++mCntAbsEnd;
  }

  inline void trigger_loop()
  {
    // calculate overall runtime between loops
    trigger_end();
    trigger_begin();
  }

  [[nodiscard]] inline uint64_t get_time_per_round_in_ns() const
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
      mCntAbsBeginPrinted = mCntAbsBegin + mPrintCntSteps;
      std::cout << "Duration of " << mName
                << ":  " << _add_ticks(mTimeAbs.count() / mCntAbsBegin) << " ns"
                << ",  Min: " << _add_ticks(mTimeMin.count()) << " ns"
                << ",  Max: " << _add_ticks(mTimeMax.count()) << " ns"
                << ",  Rounds: " << _add_ticks(mCntAbsBegin) << std::endl;
    }
  }

  void reset()
  {
    mTimeAbs = std::chrono::nanoseconds(0);
    mCntAbsBegin = 0;
    mCntAbsEnd = 0; // for plausibility check
  }

private:
#ifdef USE_STEADY_CLOCK
  std::chrono::steady_clock::time_point mLastCheckedTimePoint;
#else
  std::chrono::high_resolution_clock::time_point mLastCheckedTimePoint;
#endif
  std::chrono::nanoseconds mTimeAbs = std::chrono::nanoseconds::zero();
  std::chrono::nanoseconds mTimeMin = std::chrono::nanoseconds::max();
  std::chrono::nanoseconds mTimeMax = std::chrono::nanoseconds::min();
  uint64_t mCntAbsBegin = 0;
  uint64_t mCntAbsEnd = 0;
  uint64_t mCntAbsBeginPrinted = 0;
  uint64_t mPrintCntSteps = 1;
  std::string mName;

  std::string _add_ticks(const uint64_t iVal)
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
