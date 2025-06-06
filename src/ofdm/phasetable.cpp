/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
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

#include  "phasetable.h"
#include  <array>

static const std::array<PhaseTable::SPhasetableElement, 49> modeI_table {{
  { -768,  -737,  0, 1 },
  { -736,  -705,  1, 2 },
  { -704,  -673,  2, 0 },
  { -672,  -641,  3, 1 },
  { -640,  -609,  0, 3 },
  { -608,  -577,  1, 2 },
  { -576,  -545,  2, 2 },
  { -544,  -513,  3, 3 },
  { -512,  -481,  0, 2 },
  { -480,  -449,  1, 1 },
  { -448,  -417,  2, 2 },
  { -416,  -385,  3, 3 },
  { -384,  -353,  0, 1 },
  { -352,  -321,  1, 2 },
  { -320,  -289,  2, 3 },
  { -288,  -257,  3, 3 },
  { -256,  -225,  0, 2 },
  { -224,  -193,  1, 2 },
  { -192,  -161,  2, 2 },
  { -160,  -129,  3, 1 },
  { -128,  -97,   0, 1 },
  { -96,   -65,   1, 3 },
  { -64,   -33,   2, 1 },
  { -32,   -1,    3, 2 },
  { 1,     32,    0, 3 },
  { 33,    64,    3, 1 },
  { 65,    96,    2, 1 },
  { 97,    128,   1, 1 },
  { 129,   160,   0, 2 },
  { 161,   192,   3, 2 },
  { 193,   224,   2, 1 },
  { 225,   256,   1, 0 },
  { 257,   288,   0, 2 },
  { 289,   320,   3, 2 },
  { 321,   352,   2, 3 },
  { 353,   384,   1, 3 },
  { 385,   416,   0, 0 },
  { 417,   448,   3, 2 },
  { 449,   480,   2, 1 },
  { 481,   512,   1, 3 },
  { 513,   544,   0, 3 },
  { 545,   576,   3, 3 },
  { 577,   608,   2, 3 },
  { 609,   640,   1, 0 },
  { 641,   672,   0, 3 },
  { 673,   704,   3, 0 },
  { 705,   736,   2, 1 },
  { 737,   768,   1, 1 },
  { -1000, -1000, 0, 0 }
}};

static const std::array<PhaseTable::SPhasetableElement, 13> modeII_table
{{
  { -192,  -161,  0, 2 },
  { -160,  -129,  1, 3 },
  { -128,  -97,   2, 2 },
  { -96,   -65,   3, 2 },
  { -64,   -33,   0, 1 },
  { -32,   -1,    1, 2 },
  { 1,     32,    2, 0 },
  { 33,    64,    1, 2 },
  { 65,    96,    0, 2 },
  { 97,    128,   3, 1 },
  { 129,   160,   2, 0 },
  { 161,   192,   1, 3 },
  { -1000, -1000, 0, 0 }
}};

static const std::array<PhaseTable::SPhasetableElement, 25> modeIV_table
{{
  { -384,  -353,  0, 0 },
  { -352,  -321,  1, 1 },
  { -320,  -289,  2, 1 },
  { -288,  -257,  3, 2 },
  { -256,  -225,  0, 2 },
  { -224,  -193,  1, 2 },
  { -192,  -161,  2, 0 },
  { -160,  -129,  3, 3 },
  { -128,  -97,   0, 3 },
  { -96,   -65,   1, 1 },
  { -64,   -33,   2, 3 },
  { -32,   -1,    3, 2 },
  { 1,     32,    0, 0 },
  { 33,    64,    3, 1 },
  { 65,    96,    2, 0 },
  { 97,    128,   1, 2 },
  { 129,   160,   0, 0 },
  { 161,   192,   3, 1 },
  { 193,   224,   2, 2 },
  { 225,   256,   1, 2 },
  { 257,   288,   0, 2 },
  { 289,   320,   3, 1 },
  { 321,   352,   2, 3 },
  { 353,   384,   1, 0 },
  { -1000, -1000, 0, 0 }
}};

PhaseTable::PhaseTable()
{
  mpCurrentTable = modeI_table.data();

  // mRefTable is in the frequency domain
  for (i32 i = 0; i < cTu; i++)
  {
    mRefTable[i] = cf32(0, 0);
  }

  for (i32 i = 1; i <= cK / 2; i++) // skip DC
  {
    mRefTable[0   + i] = cmplx_from_phase(get_phi(i));
    mRefTable[cTu - i] = cmplx_from_phase(get_phi(-i));
  }
}

static const i8 h0[32] = { 0, 2, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 2, 2, 1, 1, 0, 2, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 2, 2, 1, 1 };
static const i8 h1[32] = { 0, 3, 2, 3, 0, 1, 3, 0, 2, 1, 2, 3, 2, 3, 3, 0, 0, 3, 2, 3, 0, 1, 3, 0, 2, 1, 2, 3, 2, 3, 3, 0 };
static const i8 h2[32] = { 0, 0, 0, 2, 0, 2, 1, 3, 2, 2, 0, 2, 2, 0, 1, 3, 0, 0, 0, 2, 0, 2, 1, 3, 2, 2, 0, 2, 2, 0, 1, 3 };
static const i8 h3[32] = { 0, 1, 2, 1, 0, 3, 3, 2, 2, 3, 2, 1, 2, 1, 3, 2, 0, 1, 2, 1, 0, 3, 3, 2, 2, 3, 2, 1, 2, 1, 3, 2 };

i32 PhaseTable::h_table(const i32 i, const i32 j) const
{
  assert(j < 32);
  switch (i)
  {
  case 0:  return h0[j];
  case 1:  return h1[j];
  case 2:  return h2[j];
  case 3:  return h3[j];
  default: assert(0);
  }
  return 0;
}

f32 PhaseTable::get_phi(const i32 k) const
{
  for (i32 j = 0; mpCurrentTable[j].kmin != -1000; j++)
  {
    if (mpCurrentTable[j].kmin <= k && k <= mpCurrentTable[j].kmax)
    {
      const i32 k_prime = mpCurrentTable[j].kmin;
      const i32 i = mpCurrentTable[j].i;
      const i32 n = mpCurrentTable[j].n;
      return F_M_PI_2 * (f32)(h_table(i, k - k_prime) + n);
    }
  }
  return 0;
}
