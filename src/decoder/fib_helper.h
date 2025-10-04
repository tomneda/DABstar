//
// Created by tomneda on 2025-10-04.
//

#ifndef DABSTAR_FIB_HELPER_H
#define DABSTAR_FIB_HELPER_H

#include <QString>
#include <chrono>

class FibHelper
{
public:
  struct SFigBase
  {
    using TTP = std::chrono::time_point<std::chrono::steady_clock>;
    TTP TimePoint{};
    TTP TimePoint2ndCall{};
    void set_current_time() { TimePoint = std::chrono::steady_clock::now(); }
    void set_current_time_2nd_call() { if (TimePoint2ndCall ==  TTP()) TimePoint2ndCall = std::chrono::steady_clock::now(); }
  };

  QString format_time(const SFigBase & iFigBase) const;
};


#endif //DABSTAR_FIB_HELPER_H