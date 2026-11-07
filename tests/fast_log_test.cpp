#include "src/common/glob_defs.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <limits>

namespace
{
bool nearly_equal(const char * name, const f32 input, const f32 actual, const f32 expected, const f32 tolerance = 2.0e-5f)
{
  if (std::abs(actual - expected) <= tolerance * std::max(1.0f, std::abs(expected)))
  {
    return true;
  }
  std::fprintf(stderr, "%s(%g): got %.9g, expected %.9g (tolerance %.3g)\n", name, input, actual, expected, tolerance);
  return false;
}

bool nearly_equal(const char * name, const cf32 & actual, const cf32 & expected, const f32 tolerance = 2.0e-5f)
{
  if (std::abs(actual - expected) <= tolerance * std::max(1.0f, std::abs(expected)))
  {
    return true;
  }
  std::fprintf(stderr, "%s: got (%g, %g), expected (%g, %g) (tolerance %.3g)\n",
               name, actual.real(), actual.imag(), expected.real(), expected.imag(), tolerance);
  return false;
}
}

int main()
{
  constexpr f32 cMaxExpRelativeError = 0.031f;
  for (i32 i = 0; i <= 1000; ++i)
  {
    const f32 value = -15.0f + 30.0f * static_cast<f32>(i) / 1000.0f;
    const f32 expected = std::exp(value);
    const f32 actual = fast_exp(value);
    const f32 relativeError = std::abs(actual - expected) / expected;
    if (relativeError > cMaxExpRelativeError)
    {
      std::fprintf(stderr, "fast_exp(%g): got %.9g, expected %.9g (relative error %.3g, maximum %.3g)\n",
                   value, actual, expected, relativeError, cMaxExpRelativeError);
      return EXIT_FAILURE;
    }
  }

  if (std::bit_cast<std::uint32_t>(fast_exp(-100.0f)) != 0u ||
      std::bit_cast<std::uint32_t>(fast_exp(100.0f)) != 0x7f800000u)
  {
    std::fprintf(stderr, "fast_exp saturation did not produce zero and positive infinity\n");
    return EXIT_FAILURE;
  }

  for (i32 exponent = -126; exponent <= 126; ++exponent)
  {
    for (i32 mantissaStep = 0; mantissaStep <= 8; ++mantissaStep)
    {
      const f32 value = std::ldexp(1.0f + static_cast<f32>(mantissaStep) / 8.0f, exponent);
      if (!nearly_equal("fast_log", value, fast_log(value), std::log(value)) ||
          !nearly_equal("fast_log10", value, fast_log10(value), std::log10(value)))
      {
        return EXIT_FAILURE;
      }
    }
  }

  const f32 logEdgeValues[] = { std::numeric_limits<f32>::min(),
                                std::nextafter(1.0f, 0.0f), 1.0f,
                                std::nextafter(1.0f, 2.0f),
                                std::numeric_limits<f32>::max() };
  for (const f32 value : logEdgeValues)
  {
    if (!nearly_equal("fast_log", value, fast_log(value), std::log(value)) ||
        !nearly_equal("fast_log10", value, fast_log10(value), std::log10(value)))
    {
      return EXIT_FAILURE;
    }
  }

  constexpr f32 dbValues[] = { 0.0f, 0.01f, 0.1f, 1.0f, 10.0f, 100.0f };
  for (const f32 value : dbValues)
  {
    const f32 withOffset = value + 1.0e-20f;
    if (!nearly_equal("log10_times_10", value, log10_times_10(value), 10.0f * std::log10(withOffset)) ||
        !nearly_equal("log10_times_20", value, log10_times_20(value), 20.0f * std::log10(withOffset)))
    {
      return EXIT_FAILURE;
    }
  }

  if (!nearly_equal("abs_log10_with_offset", 9.0f, abs_log10_with_offset(9.0f), std::log10(10.0f)))
  {
    return EXIT_FAILURE;
  }

  const cf32 value(3.0f, 4.0f);
  const f32 expectedMagnitudeLog = std::log10(std::abs(value) + 1.0f);
  const cf32 expected = expectedMagnitudeLog * std::exp(cf32(0.0f, std::arg(value)));
  if (!nearly_equal("abs_log10_with_offset_and_phase", abs_log10_with_offset_and_phase(value), expected))
  {
    return EXIT_FAILURE;
  }

  bool clipped = false;
  const f32 approximateMagnitude = fast_abs_with_clip_det(value, clipped, 3.5f);
  if (!clipped || std::abs(approximateMagnitude - std::abs(value)) / std::abs(value) > 0.11f)
  {
    std::fprintf(stderr, "fast_abs_with_clip_det(3, 4): got %.9g, expected magnitude %.9g, clipped %d\n",
                 approximateMagnitude, std::abs(value), clipped);
    return EXIT_FAILURE;
  }

  clipped = false;
  const f32 unclippedMagnitude = fast_abs_with_clip_det(cf32(1.0f, 1.0f), clipped, 1.0f);
  if (clipped || std::abs(unclippedMagnitude - std::sqrt(2.0f)) / std::sqrt(2.0f) > 0.07f)
  {
    std::fprintf(stderr, "fast_abs_with_clip_det(1, 1): got %.9g, expected magnitude %.9g, clipped %d\n",
                 unclippedMagnitude, std::sqrt(2.0f), clipped);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}