/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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
 *
 * 	The eep handling
 */
#include <vector>
#include "dab-constants.h"
#include "eep-protection.h"
#include "protTables.h"

/**
  * \brief eep_deconvolve
  * equal error protection, bitRate and protLevel
  * define the puncturing table
  */
EepProtection::EepProtection(const i16 iBitRate, const i16 iProtLevel)
  : Protection(iBitRate)
{
  i16 viterbiCounter = 0;
  i16 L1 = 0, L2 = 0;
  const i8 * PI1 = nullptr;
  const i8 * PI2 = nullptr;
  const i16 protLevel = iProtLevel & 0x3;
  const i16 option = (iProtLevel & (1 << 2)) >> 2;

  if (option == 0) // A profiles, see 11.3.2 table 18
  {
    const i16 n = bitRate / 8;
    assert(bitRate % 8 == 0);

    switch (protLevel)
    {
    case 0:      // protection level A-1, code rate 2/8
      L1 = 6 * n - 3;
      L2 = 3;
      PI1 = get_PI_codes(24);
      PI2 = get_PI_codes(23);
      break;

    case 1:      // protection level A-2, code rate 3/8
      if (n == 1)
      {
        L1 = 5;
        L2 = 1;
        PI1 = get_PI_codes(13);
        PI2 = get_PI_codes(12);
      }
      else
      {
        L1 = 2 * n - 3;
        L2 = 4 * n + 3;
        PI1 = get_PI_codes(14);
        PI2 = get_PI_codes(13);
      }
      break;

    case 2:      // protection level A-3, code rate 4/8
      L1 = 6 * n - 3;
      L2 = 3;
      PI1 = get_PI_codes(8);
      PI2 = get_PI_codes(7);
      break;

    case 3:      // protection level A-4, code rate 6/8
      L1 = 4 * n - 3;
      L2 = 2 * n + 3;
      PI1 = get_PI_codes(3);
      PI2 = get_PI_codes(2);
      break;
    default: assert(0);
    }
  }
  else if (option == 1) // B profiles, see 11.3.2 table 19
  {
    const i16 n = bitRate / 32;
    assert(bitRate % 32 == 0);

    L1 = 24 * n - 3; // common for all B protection levels
    L2 = 3;

    switch (protLevel)
    {
    case 0:          // protection level B-1, code rate 4/9
      PI1 = get_PI_codes(10);
      PI2 = get_PI_codes(9);
      break;

    case 1:          // protection level B-2, code rate 4/7
      PI1 = get_PI_codes(6);
      PI2 = get_PI_codes(5);
      break;

    case 2:          // protection level B-3, code rate 4/6
      PI1 = get_PI_codes(4);
      PI2 = get_PI_codes(3);
      break;

    case 3:          // protection level B-4, code rate 4/5
      PI1 = get_PI_codes(2);
      PI2 = get_PI_codes(1);
      break;
    default: assert(0);
    }
  }
  else assert(0);

  const i8 * PI_X = get_PI_codes(8);

  // According to the standard we process the logical frame with a pair of tuples (L1, PI1), (L2, PI2)
  _extract_viterbi_block_addresses(viterbiCounter, L1, PI1);
  _extract_viterbi_block_addresses(viterbiCounter, L2, PI2);

  // We had a final block of 24 bits with puncturing, according to PI_X
  // This block constitutes the 6 * 4 bits of the register itself.
  for (i32 i = 0; i < 24; i++)
  {
    if (PI_X[i] != 0)
    {
      assert(viterbiCounter < (signed)viterbiBlock.size());
      viterbiBlockAddresses.emplace_back(&viterbiBlock[viterbiCounter]);
    }
    viterbiCounter++;
  }
}

void EepProtection::_extract_viterbi_block_addresses(i16 & ioViterbiCounter, const i16 iLx, const i8 * const ipPIx)
{
  for (i32 i = 0; i < iLx; i++)
  {
    for (i32 j = 0; j < 128; j++)
    {
      if (ipPIx[j % 32] != 0)
      {
        assert(ioViterbiCounter < (signed)viterbiBlock.size());
        viterbiBlockAddresses.emplace_back(&viterbiBlock[ioViterbiCounter]);
      }
      ioViterbiCounter++;
    }
  }
}
