//
// Created by tomneda on 2025-10-04.
//

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