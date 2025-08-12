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

#ifndef BIT_EXTRAKTORS_H
#define BIT_EXTRAKTORS_H

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

#endif // BIT_EXTRAKTORS_H
