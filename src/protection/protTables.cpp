/*
 *    Copyright (C) 2014 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB.
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "protTables.h"
#include <cassert>

// chapter 11.1.2, table 13
static constexpr i8 cPI_Codes[24][32] =
{
  { 1, 1, 0, 0,   1, 0, 0, 0,   1, 0, 0, 0,   1, 0, 0, 0,   1, 0, 0, 0,   1, 0, 0, 0,   1, 0, 0, 0,   1, 0, 0, 0 }, //  1, code rate = 8/ 9
  { 1, 1, 0, 0,   1, 0, 0, 0,   1, 0, 0, 0,   1, 0, 0, 0,   1, 1, 0, 0,   1, 0, 0, 0,   1, 0, 0, 0,   1, 0, 0, 0 }, //  2, code rate = 8/10
  { 1, 1, 0, 0,   1, 0, 0, 0,   1, 1, 0, 0,   1, 0, 0, 0,   1, 1, 0, 0,   1, 0, 0, 0,   1, 0, 0, 0,   1, 0, 0, 0 }, //  3, code rate = 8/11
  { 1, 1, 0, 0,   1, 0, 0, 0,   1, 1, 0, 0,   1, 0, 0, 0,   1, 1, 0, 0,   1, 0, 0, 0,   1, 1, 0, 0,   1, 0, 0, 0 }, //  4, code rate = 8/12
  { 1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 0, 0, 0,   1, 1, 0, 0,   1, 0, 0, 0,   1, 1, 0, 0,   1, 0, 0, 0 }, //  5, code rate = 8/13
  { 1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 0, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 0, 0, 0 }, //  6, code rate = 8/14
  { 1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 0, 0, 0 }, //  7, code rate = 8/15
  { 1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0 }, //  8, code rate = 8/16
  { 1, 1, 1, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0 }, //  9, code rate = 8/17
  { 1, 1, 1, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 1, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0 }, // 10, code rate = 8/18
  { 1, 1, 1, 0,   1, 1, 0, 0,   1, 1, 1, 0,   1, 1, 0, 0,   1, 1, 1, 0,   1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0 }, // 11, code rate = 8/19
  { 1, 1, 1, 0,   1, 1, 0, 0,   1, 1, 1, 0,   1, 1, 0, 0,   1, 1, 1, 0,   1, 1, 0, 0,   1, 1, 1, 0,   1, 1, 0, 0 }, // 12, code rate = 8/20
  { 1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 0, 0,   1, 1, 1, 0,   1, 1, 0, 0,   1, 1, 1, 0,   1, 1, 0, 0 }, // 13, code rate = 8/21
  { 1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 0, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 0, 0 }, // 14, code rate = 8/22
  { 1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 0, 0 }, // 15, code rate = 8/23
  { 1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0 }, // 16, code rate = 8/24
  { 1, 1, 1, 1,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0 }, // 17, code rate = 8/25
  { 1, 1, 1, 1,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 1,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0 }, // 18, code rate = 8/26
  { 1, 1, 1, 1,   1, 1, 1, 0,   1, 1, 1, 1,   1, 1, 1, 0,   1, 1, 1, 1,   1, 1, 1, 0,   1, 1, 1, 0,   1, 1, 1, 0 }, // 19, code rate = 8/27
  { 1, 1, 1, 1,   1, 1, 1, 0,   1, 1, 1, 1,   1, 1, 1, 0,   1, 1, 1, 1,   1, 1, 1, 0,   1, 1, 1, 1,   1, 1, 1, 0 }, // 20, code rate = 8/28
  { 1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 0,   1, 1, 1, 1,   1, 1, 1, 0,   1, 1, 1, 1,   1, 1, 1, 0 }, // 21, code rate = 8/29
  { 1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 0,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 0 }, // 22, code rate = 8/30
  { 1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 0 }, // 23, code rate = 8/31
  { 1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1 }  // 24, code rate = 8/32
};

const i8 * get_PI_codes(const i16 iPiCode)
{
  assert(iPiCode >= 1 && iPiCode <= 24);
  return cPI_Codes[iPiCode - 1];
}
