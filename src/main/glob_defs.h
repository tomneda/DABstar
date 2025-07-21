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

#include "glob_data_types.h"
#include <cstdint>
#include <complex>
#include <cassert>
#include <cmath>
#include <iomanip>


#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
  #define M_PI_2 (M_PI / 2.0)
#endif
#ifndef M_PI_4
  #define M_PI_4 (M_PI / 4.0)
#endif


// "Mode 1" constants, other modes are not used anymore in the DAB standard
constexpr auto cL        =     76;  // blocks per frame
constexpr auto cK        =   1536;  // number carriers
constexpr auto cTn       =   2656;  // null length
constexpr auto cTF       = 196608;  // samples per frame (T_n + L * T_s)
constexpr auto cTs       =   2552;  // block length
constexpr auto cTu       =   2048;  // useful part, FFT length
constexpr auto cTg       =    504;  // guard length (T_s - T_u)
constexpr auto cCarrDiff =   1000;  // freq. dist. between each OFDM Bin in Hz

constexpr auto cBitsPerSymb       = 2;
constexpr auto cFicPerFrame       = 4;
constexpr auto cFibPerFic         = 3;
constexpr auto c2K            = cK * cBitsPerSymb;           // = 3072 Bits per symbol
constexpr auto cFicSizeVitIn  = c2K * 3 / 4;                 // = 2304 Bits (FIC size before Viterbi)
constexpr auto cFicSizeVitOut     = 768;                         // =  768 Bits (FIC size after Viterbi)
constexpr auto cFibSizeVitOut = cFicSizeVitOut / cFibPerFic; // =  256 Bits (FIB size after Viterbi)

using cf32 = std::complex<f32>;
using ci16 = std::complex<i16>;
using TArrayTu = std::array<cf32, cTu>;
using TArrayTn = std::array<cf32, cTn>;

// do not make this as a template as it freed the allocated stack buffer at once after call
#define make_vla(T, iSize) static_cast<T *>(alloca(iSize * sizeof(T))) // vla = variable length array on stack

template<typename T> inline T conv_rad_to_deg(T iVal)
{
  return iVal * (T)(180.0 / M_PI);
}

template<typename T> inline T conv_deg_to_rad(T iVal)
{
  return iVal * (T)(M_PI / 180.0);
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

template<typename T> inline void limit_symmetrically(T & ioVal, const T iLimit)
{
  if (ioVal > iLimit)
  {
    ioVal = iLimit;
  }
  else if (ioVal < -iLimit)
  {
    ioVal = -iLimit;
  }
}

template<typename T> static inline T fixround(f32 v)
{
  return static_cast<T>(std::roundf(v));
}

static inline bool is_indeterminate(f32 x)
{
  return x != x;
}

static inline bool is_infinite(f32 x)
{
  return x == std::numeric_limits<f32>::infinity();
}

static inline f32 fast_abs_with_clip_det(cf32 z, bool & oClipped, f32 iClipLimit)
{
  // this is the former jan_abs() from Jan van Katwijk, should be faster than std::abs()... but is it really? (depends on the device)
  f32 re = real(z);
  f32 im = imag(z);

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
    if (re > iClipLimit)
    {
      oClipped = true; // only overwrite this!
    }
    return re + 0.5f * im;
  }
  else
  {
    if (im > iClipLimit)
    {
      oClipped = true; // only overwrite this!
    }
    return im + 0.5f * re;
  }
}

inline cf32 norm_to_length_one(const cf32 & iVal)
{
  const f32 length = std::abs(iVal);
  return (length == 0.0f ? 0.0f : iVal / length);
}

inline cf32 cmplx_from_phase(const f32 iPhase)
{
  return {std::cos(iPhase), std::sin(iPhase)};
  //return std::exp(cf32(0.0f, iPhase));
}

inline f32 log10_times_10(const f32 iVal)
{
  return 10.0f * std::log10(iVal + 1.0e-20f);
}

inline f32 log10_times_20(const f32 iVal)
{
  return 20.0f * std::log10(iVal + 1.0e-20f);
}

inline cf32 abs_log10_with_offset_and_phase(const cf32 & iVal)
{
  return std::log10(std::abs(iVal) + 1.0f) * std::exp(cf32(0.0f, std::arg(iVal)));
}

template<typename T> inline f32 abs_log10_with_offset(const T iVal)
{
  return std::log10(std::abs(iVal) + 1);
}

inline f32 turn_phase_to_first_quadrant(f32 iPhase)
{
  // following line does not work if a cyclic turn should be detected
  // return std::atan2(std::abs(sin(iPhase)), std::abs(cos(iPhase)));
  if (iPhase < 0.0f)
  {
    iPhase += (f32)M_PI; // fmod will not work as intended with negative values
  }
  return std::fmod(iPhase, (f32)M_PI_2);
}

// turns the given complex point into first quadrant
inline cf32 turn_complex_phase_to_first_quadrant(const cf32 & iValue)
{
  if (real(iValue) < 0.0f)
  {
    if (imag(iValue) < 0.0f)
    {
      return -iValue; // -90° ... -180°
    }
    return {imag(iValue), -real(iValue)}; // 90 ... 180°
  }
  else // real(iValue) >= 0.0f
  {
    if (imag(iValue) < 0.0f)
    {
      return {-imag(iValue), real(iValue)}; // 0° ... -90°
    }
    return iValue; // 0° ... 90°
  }
}

template<typename T> inline T calc_adaptive_alpha(const T iAlphaBegin, const T iAlphaFinal, const T iCurValNormed)
{
  assert(iAlphaBegin > 0 && iAlphaFinal > 0);
  const T alphaBeginLog = std::log10(iAlphaBegin);
  const T alphaFinalLog = std::log10(iAlphaFinal);
  T beta = std::abs(iCurValNormed - 1);  // iCurValNormed is expected at 1 in swing-in state
  if (beta > 1) beta = 1;
  const T alphaLog = alphaBeginLog * beta + alphaFinalLog * (1 - beta);
  const T alpha = std::pow((T)10, alphaLog);
  return alpha;
}

template<typename T> inline void mean_filter(T & ioVal, const T iVal, const T iAlpha)
{
  ioVal += iAlpha * (iVal - ioVal);
}

template<typename T> inline void mean_filter_adaptive(T & ioVal, const T iVal, const T iValFinal, const T iAlphaBegin, const T iAlphaFinal)
{
  assert(iValFinal != 0);
  const T alpha = calc_adaptive_alpha(iAlphaBegin, iAlphaFinal, ioVal / iValFinal);
  ioVal += alpha * (iVal - ioVal);
}

template<typename T> inline void create_blackman_window(T * opVal, i32 iWindowWidth)
{
#if 0 // The exact(er) version place zeros at the 3rd and 4th side-lobes but result in a discontinuity at the edges and a 6 dB/oct fall-off
  constexpr f64 a0 = 7938.0/18608.0; // 0.426590714
  constexpr f64 a1 = 9240.0/18608.0; // 0.496560619
  constexpr f64 a2 = 1430.0/18608.0; // 0.076848667.

  for (i32 i = 0; i < iWindowWidth; i++)
  {
    opVal[i] = static_cast<T>(a0
                            - a1 * cos((2.0 * M_PI * i) / (iWindowWidth - 1))
                            + a2 * cos((4.0 * M_PI * i) / (iWindowWidth - 1)));
  }
#else // The truncated coefficients do not null the sidelobes as well, but have an improved 18 dB/oct fall-off
  for (i32 i = 0; i < iWindowWidth; i++)
  {
    opVal[i] = static_cast<T>(0.42
                            - 0.50 * std::cos((2.0 * M_PI * i) / (iWindowWidth - 1))
                            + 0.08 * std::cos((4.0 * M_PI * i) / (iWindowWidth - 1)));
  }
#endif
}

template<typename T> inline void create_flattop_window(T * opVal, i32 iWindowWidth)
{
  //constexpr f64 a[5] = { 1.000, -1.930, 1.290, -0.388, 0.028 }; // max signal DC gain = 1.0 (sum / iWindowsWidth)
  constexpr f64 a[5] = { 0.21557895, -0.41663158, 0.277263158, -0.083578947, 0.006947368 }; // max. pulse gain is 1.0

  for (i32 i = 0; i < iWindowWidth; i++)
  {
    f64 w = 0.0;
    for (i32 j = 0; j < 5; ++j)
    {
      w += a[j] * std::cos(j * i * 2.0 * M_PI / (iWindowWidth - 1));
    }
    opVal[i] = static_cast<T>(w);
  }
}

inline i32 get_range_from_bit_depth(i32 iBitDepth)
{
  assert(iBitDepth >= 1 && iBitDepth <= 31);
  i32 range = 1;
  while (--iBitDepth > 0)
  {
    range <<= 1;
  }
  return range;
}

template<typename T> inline T fft_shift(const T iIdx, const i32 iFftSize)
{
  return (iIdx < 0 ? iIdx + iFftSize : iIdx);
}

template<typename T> inline T fft_shift_skip_dc(const T iIdx, const i32 iFftSize)
{
  return (iIdx < 0 ? iIdx + iFftSize : iIdx + 1);
}

inline std::string cmplx_to_polar_str(const cf32 & iVal)
{
  std::ostringstream s;
  s << std::fixed << std::setw(7) << std::setprecision(2) << std::abs(iVal) << "/" << std::setw(4) << std::setprecision(0) << conv_rad_to_deg(std::arg(iVal)) << "deg";
  return s.str();
}

#endif // GLOB_DEFS_H
