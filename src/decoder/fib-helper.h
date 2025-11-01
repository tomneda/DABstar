/*
 * Copyright (c) 2025 by Thomas Neder (https://github.com/tomneda)
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
#ifndef DABSTAR_FIB_HELPER_H
#define DABSTAR_FIB_HELPER_H

#include <QString>
#include <chrono>

#include "glob_data_types.h"

class FibHelper
{
public:
  using TCT = std::chrono::steady_clock;
  using TTP = std::chrono::time_point<TCT>;
  using TT  = std::chrono::milliseconds;

  struct SFigBase
  {
    TTP TimePoint{};
    TTP TimePoint2ndCall{};
    void set_current_time() { TimePoint = std::chrono::steady_clock::now(); }
    void set_current_time_2nd_call() { if (TimePoint2ndCall ==  TTP()) TimePoint2ndCall = std::chrono::steady_clock::now(); }
  };

  struct SStatistic
  {
    SStatistic(TTP iTimePointRef) : TimePointRef(iTimePointRef) {}
    const TTP TimePointRef;
    TT  DurationMin = TT::max();
    TT  DurationMax = TT::min();
    TT  Duration2ndCallMin = TT::max();
    TT  Duration2ndCallMax = TT::min();
    u32 Count = 0;
  };

  void get_statistics(const SFigBase & iFigBase, SStatistic & ioStatistic, std::chrono::milliseconds * opDuration1 = nullptr, std::chrono::milliseconds * opDuration2 = nullptr) const;
  QString print_duration_and_get_statistics(const SFigBase & iFigBase, SStatistic & ioStatistic) const;
  QString print_statistic_header() const;
  QString print_statistic(const SStatistic & iStatistic) const;
};


#endif //DABSTAR_FIB_HELPER_H