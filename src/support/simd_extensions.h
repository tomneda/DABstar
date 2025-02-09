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

// perform abs() on float input vector (remove sign)
inline void simd_abs(float * opOut, const float * ipInp, const size_t iN)
{
  assert(iN % 4 == 0);

  const float * pInp = reinterpret_cast<const float *>(ipInp);
  float * pOut = reinterpret_cast<float *>(opOut);

  __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));

  for (size_t i = 0; i < iN; i += 4, pInp += 4, pOut += 4)
  {
    __m128 data = _mm_loadu_ps(pInp);
    __m128 result = _mm_and_ps(data, mask);
    _mm_storeu_ps(pOut, result);
  }
}

inline void simd_normalize(cmplx * opOut, const cmplx * ipInp, const size_t iN)
{
  assert(iN % 4 == 0);

  const float * pInp = reinterpret_cast<const float *>(ipInp);
  float * pOut = reinterpret_cast<float *>(opOut);

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
  __attribute__((aligned(16))) float yArray[4], xArray[4], result[4];
  _mm_storeu_ps(yArray, y);
  _mm_storeu_ps(xArray, x);

  for (int i = 0; i < 4; i++)
  {
    result[i] = atan2f(yArray[i], xArray[i]);
  }

  return _mm_loadu_ps(result);
}

// get the phase, works but is slow related to VOLK, do not use this! (_mm_atan2_ps)
inline void simd_phase(float * opOut, const cmplx * ipInp, size_t size)
{
  assert(size % 4 == 0);

  const float * pInp = reinterpret_cast<const float *>(ipInp);
  float * pOut = opOut;

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
  explicit SimdVec(uint32_t iSize) : mSize(iSize)
  {
    mVolkVec = (T *)volk_malloc((iSize) * sizeof(T), volk_get_alignment());
  }

  SimdVec(SimdVec&& other) noexcept : mSize(other.mSize), mVolkVec(other.mVolkVec)
  {
    other.mVolkVec = nullptr; // Null out the moved-from pointer
  }

  SimdVec & operator=(SimdVec&& other) noexcept
  {
    if (this != &other)
    {
      volk_free(mVolkVec);
      mVolkVec = other.mVolkVec;
      other.mVolkVec = nullptr;
    }
    return *this;
  }

  ~SimdVec()
  {
    volk_free(mVolkVec);
    delete mpVolkTemp1Vec;
    delete mpVolkTemp2Vec;
  }

  const T &operator[](uint32_t iIdx) const
  {
    assert(iIdx < mSize);
    return mVolkVec[iIdx];
  }

  T &operator[](uint32_t iIdx)
  {
    assert(iIdx < mSize);
    return mVolkVec[iIdx];
  }

  void fill_zeros()
  {
    std::fill_n(mVolkVec, mSize, T());
  }

  [[nodiscard]] T * get() const
  {
    return mVolkVec;
  }

  operator T*() const
  {
    return mVolkVec;
  }

  [[nodiscard]] uint32_t size() const
  {
    return mSize;
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cmplx>, void>
  inline normalize(const SimdVec<cmplx> & iVec)
  {
    simd_normalize(mVolkVec, iVec, mSize);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cmplx>, void>
  inline multiply_conj(const SimdVec<cmplx> & iVec1, const SimdVec<cmplx> & iVec2)
  {
    volk_32fc_x2_multiply_conjugate_32fc_a(mVolkVec, iVec1, iVec2, mSize);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, float>, void>
  inline sqared_magnitude(const SimdVec<cmplx> & iVec)
  {
    volk_32fc_magnitude_squared_32f_a(mVolkVec, iVec, mSize);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, float>, void>
  inline make_sqrt(const SimdVec<float> & iVec)
  {
    volk_32f_sqrt_32f_a(mVolkVec, iVec, mSize);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, float>, void>
  inline make_arg(const SimdVec<cmplx> & iVec)
  {
    volk_32fc_s32f_atan2_32f_a(mVolkVec, iVec, 1.0f /*no weigth*/, mSize); // this is not really faster than a simple loop
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, float>, void>
  inline wrap_4QPSK_to_phase_zero(const SimdVec<float> & iVec)
  {
    volk_32f_s32f_s32f_mod_range_32f_a(mVolkVec, iVec, 0.0f, F_M_PI_2, mSize);
    volk_32f_s32f_add_32f_a(mVolkVec, mVolkVec, -F_M_PI_4, mSize);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, float>, void>
  inline multiply_vector_and_scalar(const SimdVec<float> & iVec, float iScalar)
  {
    volk_32f_s32f_multiply_32f_a(mVolkVec, iVec, iScalar, mSize);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, float>, void>
  inline accumulate_vector(const SimdVec<float> & iVec)
  {
    volk_32f_x2_add_32f_a(mVolkVec, mVolkVec, iVec, mSize);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, float>, void>
  inline square(const SimdVec<float> & iVec)
  {
    volk_32f_x2_multiply_32f_a(mVolkVec, iVec, iVec, mSize);
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, float>, float>
  inline sum_of_elements()
  {
    alignas(16) float sum;  // normally (the) VOLK should be asked about the alignment but this takes more time?!
    volk_32f_accumulator_s32f_a(&sum, mVolkVec, mSize);  // this sums all vector elements to the first element
    return sum;
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, float>, void>
  inline mean_filter(const float * iVec, const float iAlpha) const
  {
    T * volkTempVec = _get_temp1_vector();

    // ioVal += iAlpha * (iVal - ioVal);
    volk_32f_x2_subtract_32f_a(volkTempVec, iVec, mVolkVec, mSize);         // temp = (iVec - mVolkVec)
    volk_32f_s32f_multiply_32f_a(volkTempVec, volkTempVec, iAlpha, mSize);  // temp = alpha * temp
    volk_32f_x2_add_32f_a(mVolkVec, mVolkVec, volkTempVec, mSize);          // ioVal += temp
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, float>, float>
  inline mean_filter_sum_of_elements(const float iLastMeanVal, const float iAlpha) const
  {
    T * volkTempVec = _get_temp1_vector();
    alignas(16) float sum;  // normally (the) VOLK should be asked about the alignment but this takes more time?!

    // ioVal += iAlpha * (iVal - ioVal);
    volk_32f_s32f_add_32f_a(volkTempVec, mVolkVec, -iLastMeanVal, mSize);   // temp = (mVolkVec - iVal) -> iVal is normally the last returned sum value
    volk_32f_s32f_multiply_32f_a(volkTempVec, volkTempVec, iAlpha, mSize);  // temp = alpha * temp
    volk_32f_accumulator_s32f_a(&sum, volkTempVec, mSize);                  // this sums all temp vector elements to the first element
    return sum / (float)mSize + iLastMeanVal; // new mean value over all vector elements
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, float>, void>
  inline limit_symmetrically(const float iLimit)
  {
    // TODO: check for SIMD
    for (uint32_t idx = 0; idx < mSize; ++idx)
    {
      ::limit_symmetrically(mVolkVec[idx], iLimit);
    }
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, float>, void>
  inline sq_dist_to_nearest_constellation_point(const SimdVec<float> & iLevelRealVec, const SimdVec<float> & iLevelImagVec, const SimdVec<float> & iMeanLevelVec, float iAlpha)
  {
    T * const volkTempRealVec = _get_temp1_vector();
    T * const volkTempImagVec = _get_temp2_vector();

    // calculate the mean squared distance from the current point in 1st quadrant to the point where it should be
    simd_abs(volkTempRealVec, iLevelRealVec, mSize);
    simd_abs(volkTempImagVec, iLevelImagVec, mSize);

    // TODO: search for a more SIMD way
    for (uint32_t idx = 0; idx < mSize; ++idx)
    {
      const float meanLevelAtAxisPerBin = iMeanLevelVec[idx] * F_SQRT1_2;
      const cmplx meanLevelAtAxisPerBinCplx = cmplx(meanLevelAtAxisPerBin, meanLevelAtAxisPerBin);
      const cmplx levelAtAxisPerBinAbsCplx = cmplx(volkTempRealVec[idx], volkTempImagVec[idx]);

      volk_32fc_x2_square_dist_32f_a(volkTempRealVec, &levelAtAxisPerBinAbsCplx, &meanLevelAtAxisPerBinCplx, 1);

      ::mean_filter(mVolkVec[idx], volkTempRealVec[0], iAlpha);
    }
  }

  template <typename U = T>
  typename std::enable_if_t<std::is_same_v<U, cmplx>, void>
  inline store_to_real_and_imag(SimdVec<float> & oRealVec, SimdVec<float> & oImagVec)
  {
    volk_32fc_deinterleave_32f_x2_a(oRealVec, oImagVec, mVolkVec, mSize);
  }

private:
  const uint32_t mSize;
  T * mVolkVec = nullptr;
  mutable SimdVec<T> * mpVolkTemp1Vec = nullptr;
  mutable SimdVec<T> * mpVolkTemp2Vec = nullptr;

  T * _get_temp1_vector() const
  {
     if (mpVolkTemp1Vec == nullptr)
     {
       mpVolkTemp1Vec = new SimdVec<T>(mSize);
     }
     return mpVolkTemp1Vec->get();
  }

  T * _get_temp2_vector() const
  {
    if (mpVolkTemp2Vec == nullptr)
    {
      mpVolkTemp2Vec = new SimdVec<T>(mSize);
    }
    return mpVolkTemp2Vec->get();
  }

  friend SimdVec<T> operator*(const SimdVec<T> &lhs, const SimdVec<T> &rhs)
  {
    assert (lhs.mSize == rhs.mSize);
    SimdVec<T> result(lhs.mSize);

    for (size_t i = 0; i < lhs.mSize; ++i)
    {
      result.mVolkVec[i] = lhs.mVolkVec[i] * rhs.mVolkVec[i];
    }

    return result;
  }
};



#endif //SIMD_EXTENSIONS_H
