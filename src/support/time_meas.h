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

#define time_meas_begin(m_)       const auto __ts__##m_ = std::chrono::high_resolution_clock::now()
#define time_meas_end(m_)         const auto __te__##m_ = std::chrono::high_resolution_clock::now()
#define time_meas_print(m_)       const auto __td__##m_ = std::chrono::duration_cast<std::chrono::microseconds>(__te__##m_ - __ts__##m_); \
                                  std::cout << "Duration of " #m_ ":  " << __td__##m_.count() << " us" << std::endl
#define time_meas_end_print(m_)   time_meas_end(m_); \
                                  time_meas_print(m_)

#endif // TIME_MEAS_H
