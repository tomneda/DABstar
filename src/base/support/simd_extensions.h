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

// https://github.com/simd-everywhere/simde
// https://github.com/gnuradio/volk
// https://wiki.gnuradio.org/index.php/Volk
// https://www.gnuradio.org/doc/doxygen-3.7/volk_guide.html

#pragma once

#include "glob_defs.h"
#include <volk/volk.h>
#include <algorithm>
#include <cmath>

/*
 * Most vector operations below use VOLK, which selects the best kernel for the CPU at runtime (also NEON on ARM).
 * The few helpers without a VOLK kernel are written with SSE/SSE2 intrinsics on x86 (__SSE2__ is always defined on
 * x86-64, Intel and AMD) and as plain C++ loops on other architectures like ARM (the compiler can vectorize them, e.g. with NEON).
 */
#if defined(__SSE2__)
#include <pmmintrin.h>

// we try to use the older SSE3 commands to let also older HW be working with that

// perform abs() on f32 input vector (remove sign)
inline void simd_abs(f32 * opOut, const f32 * ipInp, const size_t iN)
{
  assert(iN % 4 == 0);

  const f32 * pInp = reinterpret_cast<const f32 *>(ipInp);
  f32 * pOut = reinterpret_cast<f32 *>(opOut);

  __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));

  for (size_t i = 0; i < iN; i += 4, pInp += 4, pOut += 4)
  {
    __m128 data = _mm_load_ps(pInp);
    __m128 result = _mm_and_ps(data, mask);
    _mm_store_ps(pOut, result);
  }
}

inline void simd_normalize(cf32 * opOut, const cf32 * ipInp, const size_t iN)
{
  assert(iN % 4 == 0);

  const f32 * pInp = reinterpret_cast<const f32 *>(ipInp);
  f32 * pOut = reinterpret_cast<f32 *>(opOut);

  for (size_t i = 0; i < iN; i += 2, pInp += 4, pOut += 4)
  {
    // Load 4 floats: [real1, imag1, real2, imag2]
    __m128 v = _mm_load_ps(pInp);

    // Square: [real1^2, imag1^2, real2^2, imag2^2]
    __m128 squared = _mm_mul_ps(v, v);

    // Shuffle and horizontal add to compute real^2 + imag^2 for each complex number
    __m128 shuffled = _mm_shuffle_ps(squared, squared, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 magnitudeSquared = _mm_add_ps(squared, shuffled);

    // Compute the reciprocal square root (1 / sqrt(real^2 + imag^2))
    __m128 invMagnitude = _mm_rsqrt_ps(magnitudeSquared); // no element of magnitudeSquared may be 0

    // Normalize: Multiply original vector `v` with the reciprocal magnitude
    __m128 normalized = _mm_mul_ps(v, invMagnitude);

    // Store the normalized values to the output array
    _mm_store_ps(pOut, normalized);
  }
}

// Turn each complex input value by a multiple of 90 degrees into the sector -45..45 degrees around the positive real axis
// (removes a QPSK decision on the axes: the result is v * conj(nearest axis direction)).
inline void simd_fold_to_positive_real_axis(cf32 * opOut, const cf32 * ipInp, const size_t iN)
{
  assert(iN % 2 == 0);

  const f32 * pInp = reinterpret_cast<const f32 *>(ipInp);
  f32 * pOut = reinterpret_cast<f32 *>(opOut);

  const __m128 absMask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
  const __m128 signMask = _mm_castsi128_ps(_mm_set1_epi32((i32)0x80000000));
  const __m128 signImag = _mm_castsi128_ps(_mm_set_epi32((i32)0x80000000, 0, (i32)0x80000000, 0)); // sign bit of the imaginary parts

  for (size_t i = 0; i < iN; i += 2, pInp += 4, pOut += 4)
  {
    const __m128 v = _mm_load_ps(pInp);                                  // [r0, i0, r1, i1]
    const __m128 vAbs = _mm_and_ps(v, absMask);                          // [|r0|, |i0|, |r1|, |i1|]
    const __m128 vAbsSw = _mm_shuffle_ps(vAbs, vAbs, _MM_SHUFFLE(2, 3, 0, 1)); // [|i0|, |r0|, |i1|, |r1|]
    __m128 nearReal = _mm_cmpge_ps(vAbs, vAbsSw);                        // lane 0 and 2: |r| >= |i|
    nearReal = _mm_shuffle_ps(nearReal, nearReal, _MM_SHUFFLE(2, 2, 0, 0)); // broadcast to both parts of each complex value
    const __m128 vSw = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));    // [i0, r0, i1, r1]
    const __m128 vMulMinusJ = _mm_xor_ps(vSw, signImag);                 // v * (-j) = [i0, -r0, i1, -r1]
    __m128 t = _mm_or_ps(_mm_and_ps(nearReal, v), _mm_andnot_ps(nearReal, vMulMinusJ)); // nearer to the real axis now
    __m128 realSign = _mm_shuffle_ps(t, t, _MM_SHUFFLE(2, 2, 0, 0));     // real parts broadcast
    realSign = _mm_and_ps(realSign, signMask);
    t = _mm_xor_ps(t, realSign);                                         // negative real part: turn by 180 degrees
    _mm_store_ps(pOut, t);
  }
}

// ioVal += iA * iB
inline void simd_multiply_add(f32 * ioVal, const f32 * ipA, const f32 * ipB, const size_t iN)
{
  assert(iN % 4 == 0);

  for (size_t i = 0; i < iN; i += 4)
  {
    const __m128 prod = _mm_mul_ps(_mm_load_ps(ipA + i), _mm_load_ps(ipB + i));
    _mm_store_ps(ioVal + i, _mm_add_ps(_mm_load_ps(ioVal + i), prod));
  }
}

// exact 1 / x (not the approximation _mm_rcp_ps()), fallback for VOLK < 3.3 (see SimdVec::set_reciprocal_each_element())
inline void simd_reciprocal(f32 * opOut, const f32 * ipInp, const size_t iN)
{
  assert(iN % 4 == 0);

  const __m128 one = _mm_set1_ps(1.0f);
  for (size_t i = 0; i < iN; i += 4)
  {
    _mm_store_ps(opOut + i, _mm_div_ps(one, _mm_load_ps(ipInp + i)));
  }
}

#else // !__SSE2__: portable versions of the helpers above with the same results (apart from the rounding in simd_normalize())

// perform abs() on f32 input vector (remove sign)
inline void simd_abs(f32 * opOut, const f32 * ipInp, const size_t iN)
{
  assert(iN % 4 == 0);

  for (size_t i = 0; i < iN; ++i)
  {
    opOut[i] = std::abs(ipInp[i]);
  }
}

inline void simd_normalize(cf32 * opOut, const cf32 * ipInp, const size_t iN)
{
  assert(iN % 4 == 0);

  for (size_t i = 0; i < iN; ++i)
  {
    // exact 1 / sqrt() here, the SSE version uses the approximation _mm_rsqrt_ps() (no element may be 0)
    opOut[i] = ipInp[i] * (1.0f / std::sqrt(std::norm(ipInp[i])));
  }
}

// see SSE version above
inline void simd_fold_to_positive_real_axis(cf32 * opOut, const cf32 * ipInp, const size_t iN)
{
  assert(iN % 2 == 0);

  for (size_t i = 0; i < iN; ++i)
  {
    const f32 re = ipInp[i].real();
    const f32 im = ipInp[i].imag();
    // nearer to the real axis: keep, else turn by -90 degrees (v * (-j))
    f32 tRe = re;
    f32 tIm = im;
    if (!(std::abs(re) >= std::abs(im)))
    {
      tRe = im;
      tIm = -re;
    }
    // negative real part (sign bit set, as in the SSE version): turn by 180 degrees
    if (std::signbit(tRe))
    {
      tRe = -tRe;
      tIm = -tIm;
    }
    opOut[i] = cf32(tRe, tIm);
  }
}

// ioVal += iA * iB
inline void simd_multiply_add(f32 * ioVal, const f32 * ipA, const f32 * ipB, const size_t iN)
{
  assert(iN % 4 == 0);

  for (size_t i = 0; i < iN; ++i)
  {
    ioVal[i] += ipA[i] * ipB[i];
  }
}

// exact 1 / x, fallback for VOLK < 3.3 (see SimdVec::set_reciprocal_each_element())
inline void simd_reciprocal(f32 * opOut, const f32 * ipInp, const size_t iN)
{
  assert(iN % 4 == 0);

  for (size_t i = 0; i < iN; ++i)
  {
    opOut[i] = 1.0f / ipInp[i];
  }
}

#endif // __SSE2__

template<typename T>
class SimdVec
{
public:
  SimdVec() = delete;

  explicit SimdVec(u32 iNrTempFloatVec, u32 iNrTempCmplxVec = 0)
  {
    const size_t align = volk_get_alignment();
    mVolkVec = (T *)volk_malloc(cK * sizeof(T), align);
    mVolkSingleFloat = (T *)volk_malloc(1 * sizeof(T), align);
    assert(mVolkSingleFloat != nullptr);
    assert(mVolkVec != nullptr);

    switch (iNrTempFloatVec)
    {
    case 3:
      mVolkTempFloat3Vec = (f32 *)volk_malloc(cK * sizeof(f32), align);
      assert(mVolkTempFloat3Vec != nullptr);
      [[fallthrough]];
    case 2:
      mVolkTempFloat2Vec = (f32 *)volk_malloc(cK * sizeof(f32), align);
      assert(mVolkTempFloat2Vec != nullptr);
      [[fallthrough]];
    case 1:
      mVolkTempFloat1Vec = (f32 *)volk_malloc(cK * sizeof(f32), align);
      assert(mVolkTempFloat1Vec != nullptr);
      [[fallthrough]];
    default:;
    }

    switch (iNrTempCmplxVec)
    {
    case 1:
      mVolkTempCmplx1Vec = (cf32 *)volk_malloc(cK * sizeof(cf32), align);
      assert(mVolkTempCmplx1Vec != nullptr);
      [[fallthrough]];
    default:;
    }
  }

  ~SimdVec()
  {
    volk_free(mVolkTempCmplx1Vec);
    volk_free(mVolkTempFloat3Vec);
    volk_free(mVolkTempFloat2Vec);
    volk_free(mVolkTempFloat1Vec);
    volk_free(mVolkSingleFloat);
    volk_free(mVolkVec);
  }

  void fill_zeros()
  {
    std::fill_n(mVolkVec, cK, T());
  }

  void fill(const T & iValue)
  {
    std::fill_n(mVolkVec, cK, iValue);
  }

  void set_copy(const SimdVec<T> & iVec)
  {
    std::copy_n(iVec.mVolkVec, cK, mVolkVec);
  }

  const T &operator[](u32 iIdx) const
  {
    assert(iIdx < cK);
    return mVolkVec[iIdx];
  }

  T &operator[](u32 iIdx)
  {
    assert(iIdx < cK);
    return mVolkVec[iIdx];
  }

  [[nodiscard]] T * get() const
  {
    return mVolkVec;
  }

  operator T*() const
  {
    return mVolkVec;
  }

  [[nodiscard]] u32 size() const
  {
    return cK;
  }

  /* Method naming rules:
   *   set_...    -> the method will set (overwrite) the object value with the calculation
   *   get_...    -> the method will return a calculated value without changing the object
   *   store_...  -> the method will return changed values in the arguments without changing the object
   *   modify_... -> the method will modify the object value with the calculation (uses former state)
   */

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cf32>, void>
  inline set_normalize_each_element(const SimdVec<cf32> & iVec)
  {
    simd_normalize(mVolkVec, iVec, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cf32>, void>
  inline set_back_rotate_phase_each_element(const SimdVec<cf32> & iVec, const SimdVec<f32> & iPhaseVec)
  {
    assert(mVolkTempFloat1Vec != nullptr);
    assert(mVolkTempFloat2Vec != nullptr);
    assert(mVolkTempCmplx1Vec != nullptr);

#if (VOLK_VERSION >= 30300)
    volk_32f_sincos_32f_x2_a(mVolkTempFloat2Vec, mVolkTempFloat1Vec, iPhaseVec, cK);
#else
    volk_32f_cos_32f_a(mVolkTempFloat1Vec, iPhaseVec, cK);
    volk_32f_sin_32f_a(mVolkTempFloat2Vec, iPhaseVec, cK);
#endif
    volk_32f_x2_interleave_32fc_a(mVolkTempCmplx1Vec, mVolkTempFloat1Vec, mVolkTempFloat2Vec, cK);
    volk_32fc_x2_multiply_conjugate_32fc_a(mVolkVec, iVec, mVolkTempCmplx1Vec, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline modify_multiply_scalar_each_element(const f32 iScalar)
  {
    volk_32f_s32f_multiply_32f_a(mVolkVec, mVolkVec, iScalar, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cf32>, void>
  inline set_multiply_conj_each_element(const SimdVec<cf32> & iVec1, const SimdVec<cf32> & iVec2)
  {
    volk_32fc_x2_multiply_conjugate_32fc_a(mVolkVec, iVec1, iVec2, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_multiply_each_element(const SimdVec<f32> & iVec1, const SimdVec<f32> & iVec2)
  {
    volk_32f_x2_multiply_32f_a(mVolkVec, iVec1, iVec2, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline modify_multiply_each_element(const SimdVec<f32> & iVec1)
  {
    volk_32f_x2_multiply_32f_a(mVolkVec, mVolkVec, iVec1, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_divide_each_element(const SimdVec<f32> & iVec1, const SimdVec<f32> & iVec2)
  {
    volk_32f_x2_divide_32f_a(mVolkVec, iVec1, iVec2, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_add_each_element(const SimdVec<f32> & iVec1, const SimdVec<f32> & iVec2)
  {
    volk_32f_x2_add_32f_a(mVolkVec, iVec1, iVec2, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_subtract_each_element(const SimdVec<f32> & iVec1, const SimdVec<f32> & iVec2)
  {
    volk_32f_x2_subtract_32f_a(mVolkVec, iVec1, iVec2, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_magnitude_each_element(const SimdVec<cf32> & iVec)
  {
    volk_32fc_magnitude_32f_a(mVolkVec, iVec, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_squared_magnitude_each_element(const SimdVec<cf32> & iVec)
  {
    volk_32fc_magnitude_squared_32f_a(mVolkVec, iVec, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_sqrt_each_element(const SimdVec<f32> & iVec)
  {
    volk_32f_sqrt_32f_a(mVolkVec, iVec, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline modify_sqrt_each_element()
  {
    volk_32f_sqrt_32f_a(mVolkVec, mVolkVec, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_arg_each_element(const SimdVec<cf32> & iVec)
  {
    volk_32fc_s32f_atan2_32f_a(mVolkVec, iVec, 1.0f /*no weigth*/, cK); // this is not really faster than a simple loop
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_wrap_4QPSK_to_phase_zero_each_element(const SimdVec<f32> & iVec)
  {
    volk_32f_s32f_s32f_mod_range_32f_a(mVolkVec, iVec, 0.0f, F_M_PI_2, cK);
    volk_32f_s32f_add_32f_a(mVolkVec, mVolkVec, -F_M_PI_4, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline modify_add_scalar_each_element(f32 iScalar)
  {
    volk_32f_s32f_add_32f_a(mVolkVec, mVolkVec, iScalar, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_add_vector_and_scalar_each_element(const SimdVec<f32> & iVec, f32 iScalar)
  {
    volk_32f_s32f_add_32f_a(mVolkVec, iVec, iScalar, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_multiply_vector_and_scalar_each_element(const SimdVec<f32> & iVec, f32 iScalar)
  {
    volk_32f_s32f_multiply_32f_a(mVolkVec, iVec, iScalar, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline modify_accumulate_each_element(const SimdVec<f32> & iVec)
  {
    volk_32f_x2_add_32f_a(mVolkVec, mVolkVec, iVec, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_square_each_element(const SimdVec<f32> & iVec)
  {
    volk_32f_x2_multiply_32f_a(mVolkVec, iVec, iVec, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline modify_mean_filter_each_element(const SimdVec<f32> & iVec, const f32 iAlpha) const
  {
    assert(mVolkTempFloat1Vec != nullptr);

    // ioVal += iAlpha * (iVal - ioVal);
    volk_32f_x2_subtract_32f_a(mVolkTempFloat1Vec, iVec, mVolkVec, cK);                // temp = (iVec - mVolkVec)
    volk_32f_s32f_multiply_32f_a(mVolkTempFloat1Vec, mVolkTempFloat1Vec, iAlpha, cK);  // temp = alpha * temp
    volk_32f_x2_add_32f_a(mVolkVec, mVolkVec, mVolkTempFloat1Vec, cK);                 // ioVal += temp
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, f32>
  inline get_sum_of_elements() const
  {
    volk_32f_accumulator_s32f_a(mVolkSingleFloat, mVolkVec, cK);  // this sums all vector elements to the first element
    return mVolkSingleFloat[0];
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline modify_limit_symmetrically_each_element(const f32 iLimit)
  {
    volk_32f_s32f_x2_clamp_32f_a(mVolkVec, mVolkVec, -iLimit, iLimit, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline modify_check_negative_or_zero_values_and_fallback_each_element(const f32 iFallbackLimit)
  {
    // TODO: check for SIMD
    /*for (u32 idx = 0; idx < cK; ++idx)
    {
      if (mVolkVec[idx] <= 0)
      {
        // fprintf(stderr, "Change idx %d from %f to fallback value %f\n", idx, mVolkVec[idx], iFallbackLimit);
        mVolkVec[idx] = iFallbackLimit;
      }
    }*/
    volk_32f_s32f_x2_clamp_32f_a(mVolkVec, mVolkVec, iFallbackLimit, 1e6f, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_squared_distance_to_nearest_constellation_point_each_element(const SimdVec<f32> & iLevelRealVec, const SimdVec<f32> & iLevelImagVec, const SimdVec<f32> & iMeanLevelVec)
  {
    // calculate the mean squared distance from the current point in 1st quadrant to the point where it should be
    assert(mVolkTempFloat1Vec != nullptr);
    assert(mVolkTempFloat2Vec != nullptr);
    assert(mVolkTempFloat3Vec != nullptr);

    // the value of the mean level on each axis (45° angle assumed in 1st quadrant)
    volk_32f_s32f_multiply_32f_a(mVolkTempFloat3Vec, iMeanLevelVec, F_SQRT1_2, cK); // n = iMean / sqrt(2)

    // swap input values to 1st quadrant (phase turn is not necessary)
    simd_abs(mVolkTempFloat1Vec, iLevelRealVec, cK); // a
    simd_abs(mVolkTempFloat2Vec, iLevelImagVec, cK); // b

    // subtract mean value to get the distance on each axis
    volk_32f_x2_subtract_32f_a(mVolkTempFloat1Vec, mVolkTempFloat1Vec, mVolkTempFloat3Vec, cK); // a - n
    volk_32f_x2_subtract_32f_a(mVolkTempFloat2Vec, mVolkTempFloat2Vec, mVolkTempFloat3Vec, cK); // b - n

    // now calculate the squared cartesian distance
    volk_32f_x2_multiply_32f_a(mVolkTempFloat1Vec, mVolkTempFloat1Vec, mVolkTempFloat1Vec, cK); // (a - n)^2
    volk_32f_x2_multiply_32f_a(mVolkTempFloat2Vec, mVolkTempFloat2Vec, mVolkTempFloat2Vec, cK); // (b - n)^2
    volk_32f_x2_add_32f_a(mVolkVec, mVolkTempFloat1Vec, mVolkTempFloat2Vec, cK);                // (a - n)^2 + (b - n)^2
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_squared_magnitude_of_elements(const SimdVec<f32> & iLevelRealVec, const SimdVec<f32> & iLevelImagVec)
  {
    // calculate the mean squared distance from the current point in 1st quadrant to the point where it should be
    assert(mVolkTempFloat1Vec != nullptr);
    assert(mVolkTempFloat2Vec != nullptr);

    volk_32f_x2_multiply_32f_a(mVolkTempFloat1Vec, iLevelRealVec, iLevelRealVec, cK); // real^2
    volk_32f_x2_multiply_32f_a(mVolkTempFloat2Vec, iLevelImagVec, iLevelImagVec, cK); // imag^2
    volk_32f_x2_add_32f_a(mVolkVec, mVolkTempFloat1Vec, mVolkTempFloat2Vec, cK);      // real^2 + imag^2
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cf32>, void>
  inline store_to_real_and_imag_each_element(SimdVec<f32> & oRealVec, SimdVec<f32> & oImagVec) const
  {
    volk_32fc_deinterleave_32f_x2_a(oRealVec, oImagVec, mVolkVec, cK);
  }

  // ---- complex vectors

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cf32>, void>
  inline set_multiply_each_element(const SimdVec<cf32> & iVec1, const SimdVec<cf32> & iVec2)
  {
    volk_32fc_x2_multiply_32fc_a(mVolkVec, iVec1, iVec2, cK);
  }

  // complex vector times real vector
  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cf32>, void>
  inline set_multiply_real_each_element(const SimdVec<cf32> & iVec, const SimdVec<f32> & iRealVec)
  {
    volk_32fc_32f_multiply_32fc_a(mVolkVec, iVec, iRealVec, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cf32>, void>
  inline modify_multiply_scalar_each_element(const cf32 & iScalar)
  {
#if (VOLK_VERSION >= 30000)
    volk_32fc_s32fc_multiply2_32fc_a(mVolkVec, mVolkVec, &iScalar, cK);
#else
    volk_32fc_s32fc_multiply_32fc_a(mVolkVec, mVolkVec, iScalar, cK);
#endif
  }

  // exp(j * iPhaseVec) of each element
  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cf32>, void>
  inline set_unit_phasor_each_element(const SimdVec<f32> & iPhaseVec)
  {
    assert(mVolkTempFloat1Vec != nullptr);
    assert(mVolkTempFloat2Vec != nullptr);

#if (VOLK_VERSION >= 30300)
    volk_32f_sincos_32f_x2_a(mVolkTempFloat2Vec, mVolkTempFloat1Vec, iPhaseVec, cK);
#else
    volk_32f_cos_32f_a(mVolkTempFloat1Vec, iPhaseVec, cK);
    volk_32f_sin_32f_a(mVolkTempFloat2Vec, iPhaseVec, cK);
#endif
    volk_32f_x2_interleave_32fc_a(mVolkVec, mVolkTempFloat1Vec, mVolkTempFloat2Vec, cK);
  }

  // turn each element by a multiple of 90 degrees into the sector -45..45 degrees around the positive real axis
  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cf32>, void>
  inline set_fold_to_positive_real_axis_each_element(const SimdVec<cf32> & iVec)
  {
    simd_fold_to_positive_real_axis(mVolkVec, iVec, cK);
  }

  // sum of this[i] * iVec[i] over the first iN elements
  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cf32>, cf32>
  inline get_dot_product(const SimdVec<cf32> & iVec, const u32 iN = cK) const
  {
    assert(iN <= cK);
    cf32 result;
    volk_32fc_x2_dot_prod_32fc_a(&result, mVolkVec, iVec, iN);
    return result;
  }

  // sum of this[i] * conj(iVec[i]) over the first iN elements
  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cf32>, cf32>
  inline get_conj_dot_product(const SimdVec<cf32> & iVec, const u32 iN = cK) const
  {
    assert(iN <= cK);
    cf32 result;
    volk_32fc_x2_conjugate_dot_prod_32fc_a(&result, mVolkVec, iVec, iN);
    return result;
  }

  // ---- real vectors

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_abs_each_element(const SimdVec<f32> & iVec)
  {
    simd_abs(mVolkVec, iVec, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_max_each_element(const SimdVec<f32> & iVec1, const SimdVec<f32> & iVec2)
  {
    volk_32f_x2_max_32f_a(mVolkVec, iVec1, iVec2, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline modify_clamp_each_element(const f32 iMin, const f32 iMax)
  {
    volk_32f_s32f_x2_clamp_32f_a(mVolkVec, mVolkVec, iMin, iMax, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_exp_each_element(const SimdVec<f32> & iVec)
  {
    volk_32f_exp_32f_a(mVolkVec, iVec, cK);
  }

  // natural logarithm, all elements have to be > 0
  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_ln_each_element(const SimdVec<f32> & iVec)
  {
    constexpr f32 cLn2 = 0.69314718055994530942f;
    volk_32f_log2_32f_a(mVolkVec, iVec, cK);
    volk_32f_s32f_multiply_32f_a(mVolkVec, mVolkVec, cLn2, cK);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline set_reciprocal_each_element(const SimdVec<f32> & iVec)
  {
#if (VOLK_VERSION >= 30300)
    volk_32f_reciprocal_32f_a(mVolkVec, iVec, cK); // exact with SSE/AVX/NEON, rcp14 approximation with AVX512 (relative error < 2^-14)
#else
    simd_reciprocal(mVolkVec, iVec, cK);
#endif
  }

  // this += iVec1 * iVec2
  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline modify_add_product_each_element(const SimdVec<f32> & iVec1, const SimdVec<f32> & iVec2)
  {
    simd_multiply_add(mVolkVec, iVec1, iVec2, cK);
  }

  // sum of this[i] * iVec[i]
  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, f32>
  inline get_dot_product(const SimdVec<f32> & iVec) const
  {
    volk_32f_x2_dot_prod_32f_a(mVolkSingleFloat, mVolkVec, iVec, cK);
    return mVolkSingleFloat[0];
  }

private:
  T * mVolkSingleFloat = nullptr;
  T * mVolkVec = nullptr;
  f32 * mVolkTempFloat1Vec = nullptr;
  f32 * mVolkTempFloat2Vec = nullptr;
  f32 * mVolkTempFloat3Vec = nullptr;
  cf32 * mVolkTempCmplx1Vec = nullptr;
};



