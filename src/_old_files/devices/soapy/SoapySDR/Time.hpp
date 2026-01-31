///
/// \file SoapySDR/Time.hpp
///
/// Utility functions to convert time and ticks.
///
/// \copyright
/// Copyright (c) 2015-2015 Josh Blum
/// SPDX-License-Identifier: BSL-1.0
///

#pragma once
#include <SoapySDR/Config.hpp>
#include <SoapySDR/Time.h>

namespace SoapySDR
{

/*!
 * Convert a tick count into a time in nanoseconds using the tick rate.
 * \param ticks a integer tick count
 * \param rate the ticks per second
 * \return the time in nanoseconds
 */
static inline i64 ticksToTimeNs(const i64 ticks, const f64 rate);

/*!
 * Convert a time in nanoseconds into a tick count using the tick rate.
 * \param timeNs time in nanoseconds
 * \param rate the ticks per second
 * \return the integer tick count
 */
static inline i64 timeNsToTicks(const i64 timeNs, const f64 rate);

}

static inline i64 SoapySDR::ticksToTimeNs(const i64 ticks, const f64 rate)
{
    return SoapySDR_ticksToTimeNs(ticks, rate);
}

static inline i64 SoapySDR::timeNsToTicks(const i64 timeNs, const f64 rate)
{
    return SoapySDR_timeNsToTicks(timeNs, rate);
}
