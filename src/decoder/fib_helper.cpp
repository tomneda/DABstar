//
// Created by work on 2025-10-04.
//

#include "fib_helper.h"

QString FibHelper::format_time(const SFigBase & iFigBase) const
{
  const auto curTimePoint = std::chrono::steady_clock::now();
  const auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(curTimePoint - iFigBase.TimePoint);
  const auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(iFigBase.TimePoint2ndCall - iFigBase.TimePoint);
  return QString("[%1 ms][%2 ms]").arg(duration1.count(), 5, 10).arg(duration2.count(), 5, 10);
}

