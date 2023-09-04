//
// Created by work on 6/29/23.
//

// Keep this Qt free as also needed for Qt free libs

#ifndef GLOB_DEFS_H
#define GLOB_DEFS_H

#include <cstdint>
#include <complex>

using cmplx = std::complex<float>;

template<typename T>
inline T conv_rad_to_deg(T iVal) { return iVal * (T)(180.0 / 3.14159265358979323846); }
inline cmplx norm_to_length_one(const cmplx & iVal) { return iVal / std::fabs(iVal); }

#endif // GLOB_DEFS_H
