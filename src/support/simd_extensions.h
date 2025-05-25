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

#ifndef SIMD_EXTENSIONS_H
#define SIMD_EXTENSIONS_H

#include "glob_defs.h"
#include <pmmintrin.h>
//#include <immintrin.h>
#include <volk/volk.h>

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
    __m128 data = _mm_loadu_ps(pInp);
    __m128 result = _mm_and_ps(data, mask);
    _mm_storeu_ps(pOut, result);
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
    __m128 v = _mm_loadu_ps(pInp);

    // Square: [real1^2, imag1^2, real2^2, imag2^2]
    __m128 squared = _mm_mul_ps(v, v);

    // Shuffle and horizontal add to compute real^2 + imag^2 for each complex number
    __m128 shuffled = _mm_shuffle_ps(squared, squared, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 magnitudeSquared = _mm_add_ps(squared, shuffled);

    // Compute the reciprocal square root (1 / sqrt(real^2 + imag^2))
    __m128 invMagnitude = _mm_rsqrt_ps(magnitudeSquared);

    // Normalize: Multiply original vector `v` with the reciprocal magnitude
    __m128 normalized = _mm_mul_ps(v, invMagnitude);

    // Store the normalized values to the output array
    _mm_storeu_ps(pOut, normalized);
  }
}

#if 0
// Alternative to implement _mm_atan2_ps if unavailable
static inline __m128 _mm_atan2_ps(__m128 y, __m128 x)
{
  __attribute__((aligned(16))) f32 yArray[4], xArray[4], result[4];
  _mm_storeu_ps(yArray, y);
  _mm_storeu_ps(xArray, x);

  for (int i = 0; i < 4; i++)
  {
    result[i] = atan2f(yArray[i], xArray[i]);
  }

  return _mm_loadu_ps(result);
}

// get the phase, works but is slow related to VOLK, do not use this! (_mm_atan2_ps)
inline void simd_phase(f32 * opOut, const cf32 * ipInp, size_t size)
{
  assert(size % 4 == 0);

  const f32 * pInp = reinterpret_cast<const f32 *>(ipInp);
  f32 * pOut = opOut;

  for (size_t i = 0; i < size; i += 4, pInp += 8, pOut += 4)
  {
    // Extract real parts: [real1, real2, real3, real4]
    __m128 real = _mm_set_ps(pInp[6], pInp[4], pInp[2], pInp[0]);

    // Extract imaginary parts: [imag1, imag2, imag3, imag4]
    __m128 imag = _mm_set_ps(pInp[7], pInp[5], pInp[3], pInp[1]);

    // Calculate the phase using atan2
    __m128 phase = _mm_atan2_ps(imag, real); // Compute phase for 4 elements

    // Store the result in the output array
    _mm_storeu_ps(pOut, phase);
  }
}
#endif



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

    volk_32f_cos_32f(mVolkTempFloat1Vec, iPhaseVec, cK);
    volk_32f_sin_32f(mVolkTempFloat2Vec, iPhaseVec, cK);
    volk_32f_x2_interleave_32fc(mVolkTempCmplx1Vec, mVolkTempFloat1Vec, mVolkTempFloat2Vec, cK);
    volk_32fc_x2_multiply_conjugate_32fc(mVolkVec, iVec, mVolkTempCmplx1Vec, cK);
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
  inline get_mean_filter_sum_of_elements(const f32 iLastMeanVal, const f32 iAlpha) const
  {
    assert(mVolkTempFloat1Vec != nullptr);

    volk_32f_s32f_add_32f_a(mVolkTempFloat1Vec, mVolkVec, -iLastMeanVal, cK);   // temp = (mVolkVec - iLastMeanVal) -> iLastMeanVal is normally the last returned sum value
    volk_32f_accumulator_s32f_a(mVolkSingleFloat, mVolkTempFloat1Vec, cK);      // this sums all temp vector elements to the first element
    return iAlpha * mVolkSingleFloat[0] / (f32)cK + iLastMeanVal;             // new mean value over all vector elements
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
    // TODO: check for SIMD
    for (u32 idx = 0; idx < cK; ++idx)
    {
      ::limit_symmetrically(mVolkVec[idx], iLimit);
    }
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, f32>, void>
  inline modify_check_negative_or_zero_values_and_fallback_each_element(const f32 iFallbackLimit)
  {
    // TODO: check for SIMD
    for (u32 idx = 0; idx < cK; ++idx)
    {
      if (mVolkVec[idx] <= 0)
      {
        // fprintf(stderr, "Change idx %d from %f to fallback value %f\n", idx, mVolkVec[idx], iFallbackLimit);
        mVolkVec[idx] = iFallbackLimit;
      }
    }
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
  inline store_to_real_and_imag_each_element(SimdVec<f32> & oRealVec, SimdVec<f32> & oImagVec)
  {
    volk_32fc_deinterleave_32f_x2_a(oRealVec, oImagVec, mVolkVec, cK);
  }

private:
  T * mVolkSingleFloat = nullptr;
  T * mVolkVec = nullptr;
  f32 * mVolkTempFloat1Vec = nullptr;
  f32 * mVolkTempFloat2Vec = nullptr;
  f32 * mVolkTempFloat3Vec = nullptr;
  cf32 * mVolkTempCmplx1Vec = nullptr;
};



#endif //SIMD_EXTENSIONS_H
