//
// Created by Thomas Neder on 6/29/23.
//

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

template<typename T> inline float abs_log10_with_offset(const T iVal)
{
  return std::log10(std::abs(iVal) + 1);
}

template <typename T> inline void mean_filter(T & ioVal, T iVal, const T iAlpha)
{
  ioVal += iAlpha * (iVal - ioVal);
}

// the "while(false)" is due to warning about a unuseful ";" on end of macro :-(
//#define mean_filter(ioVal_, iVal_, iAlpha_)  do { ioVal_ += iAlpha_ * (iVal_ - ioVal_); } while(false)

#endif // GLOB_DEFS_H
