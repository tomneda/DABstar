///
/// \file SoapySDR/Time.h
///
/// Utility functions to convert time and ticks.
///
/// \copyright
/// Copyright (c) 2015-2015 Josh Blum
/// SPDX-License-Identifier: BSL-1.0
///

#pragma once
#include <SoapySDR/Config.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Convert a tick count into a time in nanoseconds using the tick rate.
 * \param ticks a integer tick count
 * \param rate the ticks per second
 * \return the time in nanoseconds
 */
SOAPY_SDR_API i64 SoapySDR_ticksToTimeNs(const i64 ticks, const f64 rate);

/*!
 * Convert a time in nanoseconds into a tick count using the tick rate.
 * \param timeNs time in nanoseconds
 * \param rate the ticks per second
 * \return the integer tick count
 */
SOAPY_SDR_API i64 SoapySDR_timeNsToTicks(const i64 timeNs, const f64 rate);

#ifdef __cplusplus
}
#endif
