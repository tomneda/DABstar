//
// Created by work on 2025-10-04.
//

#include "fib_helper.h"

QString FibHelper::print_duration_and_get_statistics(const SFigBase & iFigBase, SStatistic & ioStatistic) const
{
  const auto curTimePoint = std::chrono::steady_clock::now();
  const auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(curTimePoint - iFigBase.TimePoint);
  const auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(iFigBase.TimePoint2ndCall - iFigBase.TimePoint);

  ioStatistic.Count++;
  // if (iFigBase.TimePoint < ioStatistic.TimePointMin) ioStatistic.TimePointMin = iFigBase.TimePoint;
  // if (iFigBase.TimePoint > ioStatistic.TimePointMax) ioStatistic.TimePointMax = iFigBase.TimePoint;
  // if (iFigBase.TimePoint2ndCall < ioStatistic.TimePoint2ndCallMin) ioStatistic.TimePoint2ndCallMin = iFigBase.TimePoint2ndCall;
  // if (iFigBase.TimePoint2ndCall > ioStatistic.TimePoint2ndCallMax) ioStatistic.TimePoint2ndCallMax = iFigBase.TimePoint2ndCall;
  if (duration1 < ioStatistic.DurationMin) ioStatistic.DurationMin = duration1;
  if (duration1 > ioStatistic.DurationMax) ioStatistic.DurationMax = duration1;
  if (duration2 < ioStatistic.Duration2ndCallMin) ioStatistic.Duration2ndCallMin = duration2;
  if (duration2 > ioStatistic.Duration2ndCallMax) ioStatistic.Duration2ndCallMax = duration2;

  return QString("[%1 ms][%2 ms]").arg(duration1.count(), 5, 10).arg(duration2.count(), 5, 10);
}

QString FibHelper::print_statistic_header() const
{
  return QString("Count [Min/Max of time since FIG first stored] = [Diff of left]  [Min/Max of time since same FIG received 2nd time] = [Diff of left]");
}

QString FibHelper::print_statistic(const SStatistic & iStatistic) const
{
  // [InbMin][InbMax][InbDiff] [CycMin][CycMax][CycDiff]
  // auto durationTP = iStatistic.TimePointMax - iStatistic.TimePointMin;
  // auto msTP = std::chrono::duration_cast<std::chrono::milliseconds>(durationTP).count();
  // auto durationTP2nd = iStatistic.TimePoint2ndCallMax - iStatistic.TimePoint2ndCallMin;
  // auto msTP2nd = std::chrono::duration_cast<std::chrono::milliseconds>(durationTP2nd).count();

  if (iStatistic.Count == 0)
  {
    return QString("Count:  0");
  }

  return QString("Count: %1 [%2 ms  %3 ms] = [%4 ms]  [%5 ms  %6 ms] = [%7 ms]")
                  .arg(iStatistic.Count, 2, 10)
                  .arg(iStatistic.DurationMin.count(), 5, 10).arg(iStatistic.DurationMax.count(), 5, 10)
                  .arg(iStatistic.DurationMax.count() - iStatistic.DurationMin.count(), 5, 10)
                  .arg(iStatistic.Duration2ndCallMin.count(), 5, 10).arg(iStatistic.Duration2ndCallMax.count(), 5, 10)
                  .arg(iStatistic.Duration2ndCallMax.count() - iStatistic.Duration2ndCallMin.count(), 5, 10);
}