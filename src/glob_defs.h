/*
 * Copyright (c) 2023 by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
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

// Keep this Qt-free as also needed for Qt-free libs

#ifndef GLOB_DEFS_H
#define GLOB_DEFS_H

#include <cstdint>
#include <complex>

using cmplx = std::complex<float>;

template<typename T> inline T conv_rad_to_deg(T iVal)
{
  return iVal * (T)(180.0 / 3.14159265358979323846);
}

template<typename T> inline void limit_min_max(T & ioVal, T iMin, T iMax)
{
  if (ioVal < iMin)
  {
    ioVal = iMin;
  }
  else if (ioVal > iMax)
  {
    ioVal = iMax;
  }
}

template<typename T> static inline T fixround(float v)
{
  return static_cast<T>(std::roundf(v));
}

static inline bool is_indeterminate(float x)
{
  return x != x;
}

static inline bool is_infinite(float x)
{
  return x == std::numeric_limits<float>::infinity();
}

static inline float jan_abs(cmplx z)
{
  float re = real(z);
  float im = imag(z);

  if (re < 0)
  {
    re = -re;
  }
  if (im < 0)
  {
    im = -im;
  }
  if (re > im)
  {
    return re + 0.5f * im;
  }
  else
  {
    return im + 0.5f * re;
  }
}

inline cmplx norm_to_length_one(const cmplx & iVal)
{
  return iVal / std::fabs(iVal);
}

inline cmplx cmplx_from_phase(const float iPhase)
{
  return std::exp(cmplx(0.0f, iPhase));
}

inline cmplx abs_log10_with_offset_and_phase(const cmplx & iVal)
{
  return std::log10(std::abs(iVal) + 1.0f) * std::exp(cmplx(0.0f, std::arg(iVal)));
}

inline float turn_phase_to_first_quadrant(float iPhase)
{
  // following line does not work if a cyclic turn should be detected
  // return std::atan2(std::abs(sin(iPhase)), std::abs(cos(iPhase)));
  if (iPhase < 0.0f)
  {
    iPhase += M_PI; // fmod will not work as intended with negative values
  }
  return std::fmod(iPhase, (float)M_PI_2);
}

template<typename T> inline float abs_log10_with_offset(const T iVal)
{
  return std::log10(std::abs(iVal) + 1);
}

template <typename T> inline void mean_filter(T & ioVal, T iVal, const T iAlpha)
{
  ioVal += iAlpha * (iVal - ioVal);
}

#endif // GLOB_DEFS_H
