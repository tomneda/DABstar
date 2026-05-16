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

#include "fib-helper.h"

void FibHelper::get_statistics(const SFigBase & iFigBase, SStatistic & ioStatistic, std::chrono::milliseconds * opDuration1 /*= nullptr*/, std::chrono::milliseconds * opDuration2 /*= nullptr*/) const
{
  const auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(iFigBase.TimePoint - ioStatistic.TimePointRef);
  const auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(iFigBase.TimePoint2ndCall - iFigBase.TimePoint);

  ioStatistic.Count++;
  if (duration1 < ioStatistic.DurationMin) ioStatistic.DurationMin = duration1;
  if (duration1 > ioStatistic.DurationMax) ioStatistic.DurationMax = duration1;
  if (duration2 < ioStatistic.Duration2ndCallMin) ioStatistic.Duration2ndCallMin = duration2;
  if (duration2 > ioStatistic.Duration2ndCallMax) ioStatistic.Duration2ndCallMax = duration2;

  if (opDuration1) *opDuration1 = duration1;
  if (opDuration2) *opDuration2 = duration2;
}

QString FibHelper::print_duration_and_get_statistics(const SFigBase & iFigBase, SStatistic & ioStatistic) const
{
  std::chrono::milliseconds duration1, duration2;
  get_statistics(iFigBase, ioStatistic, &duration1, &duration2);

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