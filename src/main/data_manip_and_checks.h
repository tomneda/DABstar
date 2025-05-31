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

#ifndef DATA_MANIP_AND_CHECKS_H
#define DATA_MANIP_AND_CHECKS_H

#include <cinttypes>

//	generic, up to 16 bits
static inline u16 getBits(const u8 * d, i32 offset, i16 size)
{
  i16 i;
  u16 res = 0;

  for (i = 0; i < size; i++)
  {
    res <<= 1;
    res |= (d[offset + i]) & 01;
  }
  return res;
}

static inline u16 getBits_1(const u8 * d, i32 offset)
{
  return (d[offset] & 0x01);
}

static inline u16 getBits_2(const u8 * d, i32 offset)
{
  u16 res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  return res;
}

static inline u16 getBits_3(const u8 * d, i32 offset)
{
  u16 res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  res <<= 1;
  res |= d[offset + 2];
  return res;
}

static inline u16 getBits_4(const u8 * d, i32 offset)
{
  u16 res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  res <<= 1;
  res |= d[offset + 2];
  res <<= 1;
  res |= d[offset + 3];
  return res;
}

static inline u16 getBits_5(const u8 * d, i32 offset)
{
  u16 res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  res <<= 1;
  res |= d[offset + 2];
  res <<= 1;
  res |= d[offset + 3];
  res <<= 1;
  res |= d[offset + 4];
  return res;
}

static inline u16 getBits_6(const u8 * d, i32 offset)
{
  u16 res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  res <<= 1;
  res |= d[offset + 2];
  res <<= 1;
  res |= d[offset + 3];
  res <<= 1;
  res |= d[offset + 4];
  res <<= 1;
  res |= d[offset + 5];
  return res;
}

static inline u16 getBits_7(const u8 * d, i32 offset)
{
  u16 res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  res <<= 1;
  res |= d[offset + 2];
  res <<= 1;
  res |= d[offset + 3];
  res <<= 1;
  res |= d[offset + 4];
  res <<= 1;
  res |= d[offset + 5];
  res <<= 1;
  res |= d[offset + 6];
  return res;
}

static inline u16 getBits_8(const u8 * d, i32 offset)
{
  u16 res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  res <<= 1;
  res |= d[offset + 2];
  res <<= 1;
  res |= d[offset + 3];
  res <<= 1;
  res |= d[offset + 4];
  res <<= 1;
  res |= d[offset + 5];
  res <<= 1;
  res |= d[offset + 6];
  res <<= 1;
  res |= d[offset + 7];
  return res;
}


static inline u32 getLBits(const u8 * d, i32 offset, i16 amount)
{
  u32 res = 0;
  i16 i;

  for (i = 0; i < amount; i++)
  {
    res <<= 1;
    res |= (d[offset + i] & 01);
  }
  return res;
}

static inline bool check_CRC_bits(const u8 * const iIn, const i32 iSize)
{
  static const u8 crcPolynome[] = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 };  // MSB .. LSB
  i32 f;
  u8 b[16];
  i16 Sum = 0;

  memset(b, 1, 16);

  for (i32 i = 0; i < iSize; i++)
  {
    const u8 invBit = (i >= iSize - 16 ? 1 : 0);

    if ((b[0] ^ (iIn[i] ^ invBit)) == 1)
    {
      for (f = 0; f < 15; f++)
      {
        b[f] = crcPolynome[f] ^ b[f + 1];
      }
      b[15] = 1;
    }
    else
    {
      memmove(&b[0], &b[1], sizeof(u8) * 15); // Shift
      b[15] = 0;
    }
  }

  for (i32 i = 0; i < 16; i++)
  {
    Sum += b[i];
  }

  return Sum == 0;
}

static inline bool check_crc_bytes(const u8 * const msg, const i32 len)
{
  u16 accumulator = 0xFFFF;

  for (i32 i = 0; i < len; i++)
  {
    u16 data = (u16)(msg[i]) << 8;
    for (i32 j = 8; j > 0; j--)
    {
      if ((data ^ accumulator) & 0x8000)
      {
        constexpr u16 genpoly = 0x1021;
        accumulator = ((accumulator << 1) ^ genpoly) & 0xFFFF;
      }
      else
      {
        accumulator = (accumulator << 1) & 0xFFFF;
      }
      data = (data << 1) & 0xFFFF;
    }
  }
  //	ok, now check with the crc that is contained in the au
  const u16 crc = ~((msg[len] << 8) | msg[len + 1]) & 0xFFFF;
  return (crc ^ accumulator) == 0;
}

#endif // DATA_MANIP_AND_CHECKS_H
