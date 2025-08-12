/*
 * Copyright 2004,2010 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

//	This is a (partial) rewrite of the GNU radio code, for use
//	within the DAB/DAB+ sdr-j receiver software
//	all rights are acknowledged.

#ifndef  FIRECODE_CHECKER_H
#define  FIRECODE_CHECKER_H

#include "glob_data_types.h"

class FirecodeChecker
{
public:
  FirecodeChecker();
  ~FirecodeChecker() = default;

  bool check(const u8 * x); // return true if firecode check is passed
  bool check_and_correct_6bits(u8 * x); // return true if firecode check is passed

private:
  void fill_syndrome_table();
  u16 crc16(const uint8_t * x); // return syndrome

  // g(x)=(x^11+1)(x^5+x^3+x^2+x+1)=x^16+x^14+x^13+x^12+x^11+x^5+x^3+x^2+x+1
  const u16 polynom = 0x782f;
  u16 crc_table[256];
  u16 syndrome_table[65536];
  const u8 pattern[124] = {
  // 45 * 4 bit shift
   17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 30, 31, 34,
   36, 38, 40, 42, 44, 46, 50, 52, 54, 56, 60, 62, 68, 72, 76,
   84, 88, 92,100,104,108,120,124,136,152,168,184,200,216,248,
  // 15 * 2 bit shift
   33, 35, 37, 39, 41, 43, 45, 49, 51, 53, 55, 57, 59, 61, 63,
  // 15 * 2 and 6 bit shift
   66, 70, 74, 78, 82, 86, 90, 98,102,106,110,114,118,122,126,
  // 15 * 6 bit shift
  132,140,148,156,164,172,180,196,204,212,220,228,236,244,252,
  // 34 * 0 bit shift
    1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 29, 32, 48, 58, 64, 80, 96,112,116,128,144,160,176,192,
  208,224,232,240,};
  void fill_crc_table(u16 *crc_table, const u16 poly);
};

#endif

