///
/// \file SoapySDR/ConverterPrimatives.hpp
///
/// inline Soapy real format Converter Primitives.
///
/// \copyright
/// Copyright (c) 2017-2018 Coburn Wightman
/// SPDX-License-Identifier: BSL-1.0
///

#pragma once
#include <stdint.h>

namespace SoapySDR
{
  
const u32 U32_ZERO_OFFSET = u32(1<<31);
const u16 U16_ZERO_OFFSET = u16(1<<15);
const u8   U8_ZERO_OFFSET =  u8(1<<7);

const u32 S32_FULL_SCALE = u32(1<<31);
const u16 S16_FULL_SCALE = u16(1<<15);
const u8   S8_FULL_SCALE =  u8(1<<7);
  
/*!
 * Conversion Primitives for converting real values between Soapy formats.
 * \param from the value to convert from
 * \return the converted value
 */

// type conversion: f32 <> signed integers

inline i32 F32toS32(f32 from){
  return i32(from * S32_FULL_SCALE);
}
inline f32 S32toF32(i32 from){
  return f32(from) / S32_FULL_SCALE;
}

inline i16 F32toS16(f32 from){
  return i16(from * S16_FULL_SCALE);
}
inline f32 S16toF32(i16 from){
  return f32(from) / S16_FULL_SCALE;
}

inline i8 F32toS8(f32 from){
  return i8(from * S8_FULL_SCALE);
}
inline f32 S8toF32(i8 from){
  return f32(from) / S8_FULL_SCALE;
}


// type conversion: offset binary <> two's complement (signed) integers

inline i32 U32toS32(u32 from){
  return i32(from - U32_ZERO_OFFSET);
}

inline u32 S32toU32(i32 from){
  return u32(from) + U32_ZERO_OFFSET;
}

inline i16 U16toS16(u16 from){
  return i16(from - U16_ZERO_OFFSET);
}

inline u16 S16toU16(i16 from){
  return u16(from) + U16_ZERO_OFFSET;
}

inline i8 U8toS8(u8 from){
  return i8(from - U8_ZERO_OFFSET);
}

inline u8 S8toU8(i8 from){
  return u8(from) + U8_ZERO_OFFSET;
}

// size conversion: signed <> signed

inline i16 S32toS16(i32 from){
  return i16(from >> 16);
}
inline i32 S16toS32(i16 from){
  return i32(from << 16);
}

inline i8 S16toS8(i16 from){
  return i8(from >> 8);
}
inline i16 S8toS16(i8 from){
  return i16(from << 8);
}

// compound conversions

// f32 <> unsigned (type and size)

inline u32 F32toU32(f32 from){
  return S32toU32(F32toS32(from));
}
inline f32 U32toF32(u32 from){
  return S32toF32(U32toS32(from));
}

inline u16 F32toU16(f32 from){
  return S16toU16(F32toS16(from));
}
inline f32 U16toF32(u16 from){
  return S16toF32(U16toS16(from));
}

inline u8 F32toU8(f32 from){
  return S8toU8(F32toS8(from));
}
inline f32 U8toF32(u8 from){
  return S8toF32(U8toS8(from));
}

// signed <> unsigned (type and size)

inline u16 S32toU16(i32 from){
  return S16toU16(S32toS16(from));
}
inline i32 U16toS32(u16 from){
  return S16toS32(U16toS16(from));
}

inline u8 S32toU8(i32 from){
  return S8toU8(S16toS8(S32toS16(from)));
}
inline i32 U8toS32(u8 from){
  return S16toS32(S8toS16(U8toS8(from)));
}

inline u8 S16toU8(i16 from){
  return S8toU8(S16toS8(from));
}
inline i16 U8toS16(u8 from){
  return S8toS16(U8toS8(from));
}

inline u16 S8toU16(i8 from){
  return S16toU16(S8toS16(from));
}
inline i8 U16toS8(u16 from){
  return S16toS8(U16toS16(from));
}


}
