/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 * and improved by old-dab (https://github.com/old-dab)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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
//
//	This is a (partial) rewrite of the GNU radio code, for use
//	within the DAB/DAB+ sdr-j receiver software
//	all rights are acknowledged.
//
#include "firecode-checker.h"
#include <cstring>

//	g(x)=(x^11+1)(x^5+x^3+x^2+x+1)=1+x+x^2+x^3+x^5+x^11+x^12+x^13+x^14+x^16

FirecodeChecker::FirecodeChecker()
{
  fill_crc_table(crc_table, polynom);
  fill_syndrome_table();
}

void FirecodeChecker::fill_crc_table(u16 *crc_table, const u16 poly)
{
  // prepare the table
  for (int i = 0; i < 256; i++)
  {
    u16 crc = i << 8;
    for (int j = 0; j < 8; j++)
    {
      if (crc & 0x8000)
      {
        crc <<= 1;
        crc ^= poly;
      }
      else
        crc <<= 1;
    }
    crc_table[i] = crc;
  }
}

void FirecodeChecker::fill_syndrome_table()
{
  u8 error[11];
  int i, j, bit;

  memset(error, 0, 11);
  memset(syndrome_table, 0, 65536 * sizeof(syndrome_table[0]));
  // 0 bit schifted
  for (i = 0; i < 11; i++)
  {
  for(j = 0; j < 124; j++)
  {
    bit = i * 8;
      error[i] = pattern[j];
      u16 syndrome = crc16(error);
      if(syndrome_table[syndrome] == 0)
        syndrome_table[syndrome] = (bit << 8) + pattern[j];
    //else
    //  fprintf(stderr, "syndrome 0x%04x already there! pattern=%02x\n", syndrome, pattern[j]);
      error[i] = 0;
    }
  }
  // 4 bits schifted
  for (i = 0; i < 10; i++)
  {
  for(j = 0; j < 45; j++)
  {
    bit = i * 8 + 4;
      error[i]   = pattern[j] >> 4;
      error[i+1] = pattern[j] << 4;
      u16 syndrome = crc16(error);
      if(syndrome_table[syndrome] == 0)
        syndrome_table[syndrome] = (bit << 8) + pattern[j];
      //else
    //  fprintf(stderr, "syndrome 0x%04x already there! pattern=%02x, bit=%d, pattern=%02x, bit=%d\n",
    //          syndrome, syndrome_table[syndrome] & 0xff, syndrome_table[syndrome] >> 8, pattern[j], bit);
      error[i]   = 0;
      error[i+1] = 0;
    }
  }
  // 2 bits schifted
  for (i = 0; i < 10; i++)
  {
  for(j = 45; j < 75; j++)
  {
    bit = i * 8 + 2;
      error[i]   = pattern[j] >> 2;
      error[i+1] = pattern[j] << 6;
      u16 syndrome = crc16(error);
      if(syndrome_table[syndrome] == 0)
        syndrome_table[syndrome] = (bit << 8) + pattern[j];
      //else
    //  fprintf(stderr, "syndrome 0x%04x already there! pattern=%02x, bit=%d, pattern=%02x, bit=%d\n",
    //          syndrome, syndrome_table[syndrome] & 0xff, syndrome_table[syndrome] >> 8, pattern[j], bit);
      error[i]   = 0;
      error[i+1] = 0;
    }
  }
  // 6 bits schifted
  for (int i = 0; i < 10; i++)
  {
  for(int j = 60; j < 90; j++)
  {
    int bit = i * 8 + 6;
      error[i]   = pattern[j] >> 6;
      error[i+1] = pattern[j] << 2;
      u16 syndrome = crc16(error);
      if(syndrome_table[syndrome] == 0)
        syndrome_table[syndrome] = (bit << 8) + pattern[j];
      //else
    //  fprintf(stderr, "syndrome 0x%04x already there! pattern=%02x, bit=%d, pattern=%02x, bit=%d\n",
    //          syndrome, syndrome_table[syndrome] & 0xff, syndrome_table[syndrome] >> 8, pattern[j], bit);
      error[i]   = 0;
      error[i+1] = 0;
    }
  }
}

// x[0-1] contains parity, x[2-10] contains data
u16 FirecodeChecker::crc16(const u8 * x)
{
  u16 crc = 0;
  for (int i = 2; i < 11; i++)
  {
    const int pos = (crc >> 8) ^ x[i];
    crc = (crc << 8) ^ crc_table[pos];
  }
  for (int i = 0; i < 2; i++)
  {
    const int pos = (crc >> 8) ^ x[i];
    crc = (crc << 8) ^ crc_table[pos];
  }
  return crc;
}

// return true if firecode check is passed
bool FirecodeChecker::check(const u8 * x)
{
  return crc16(x) == 0;
}

// return true if firecode check is passed or a error burst up to 6 bits was corrected
bool FirecodeChecker::check_and_correct_6bits(u8 * x)
{
  u16 syndrome = crc16(x);
  if (syndrome == 0) // no error
    return true;

  u8 error = syndrome_table[syndrome] & 0xff;
  if(error)
  {
    int bit = syndrome_table[syndrome] >> 8;
    x[bit/8]   ^= error >> (bit % 8);
    x[bit/8+1] ^= error << (8 - (bit % 8));
    return true;
  }
  return false;
}
