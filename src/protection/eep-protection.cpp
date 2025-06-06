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
#include  <vector>
#include  "dab-constants.h"
#include  "eep-protection.h"
#include  "protTables.h"

/**
  *	\brief eep_deconvolve
  *	equal error protection, bitRate and protLevel
  *	define the puncturing table
  */
EepProtection::EepProtection(i16 bitRate, i16 protLevel) :
  Protection(bitRate)
{
  i16 viterbiCounter = 0;
  i16 L1 = 0, L2 = 0;
  const i8 * PI1, * PI2, * PI_X;

  if ((protLevel & (1 << 2)) == 0)
  {  // set A profiles
    switch (protLevel & 03)
    {
    case 0:      // actually level 1
      L1 = 6 * bitRate / 8 - 3;
      L2 = 3;
      PI1 = get_PCodes(24 - 1);
      PI2 = get_PCodes(23 - 1);
      break;

    case 1:      // actually level 2
      if (bitRate == 8)
      {
        L1 = 5;
        L2 = 1;
        PI1 = get_PCodes(13 - 1);
        PI2 = get_PCodes(12 - 1);
      }
      else
      {
        L1 = 2 * bitRate / 8 - 3;
        L2 = 4 * bitRate / 8 + 3;
        PI1 = get_PCodes(14 - 1);
        PI2 = get_PCodes(13 - 1);
      }
      break;

    case 2:      // actually level 3
      L1 = 6 * bitRate / 8 - 3;
      L2 = 3;
      PI1 = get_PCodes(8 - 1);
      PI2 = get_PCodes(7 - 1);
      break;

    case 3:      // actually level 4
      L1 = 4 * bitRate / 8 - 3;
      L2 = 2 * bitRate / 8 + 3;
      PI1 = get_PCodes(3 - 1);
      PI2 = get_PCodes(2 - 1);
      break;
    }
  }
  else if ((protLevel & (1 << 2)) != 0)
  {    // B series
    switch ((protLevel & 03))
    {
    case 3:          // actually level 4
      L1 = 24 * bitRate / 32 - 3;
      L2 = 3;
      PI1 = get_PCodes(2 - 1);
      PI2 = get_PCodes(1 - 1);
      break;

    case 2:          // actually level 3
      L1 = 24 * bitRate / 32 - 3;
      L2 = 3;
      PI1 = get_PCodes(4 - 1);
      PI2 = get_PCodes(3 - 1);
      break;

    case 1:          // actually level 2
      L1 = 24 * bitRate / 32 - 3;
      L2 = 3;
      PI1 = get_PCodes(6 - 1);
      PI2 = get_PCodes(5 - 1);
      break;

    case 0:          // actually level 1
      L1 = 24 * bitRate / 32 - 3;
      L2 = 3;
      PI1 = get_PCodes(10 - 1);
      PI2 = get_PCodes(9 - 1);
      break;
    }
  }
  PI_X = get_PCodes(8 - 1);

  memset(indexTable.data(), 0, (outSize * 4 + 24) * sizeof(u8));
  //
  //	according to the standard we process the logical frame
  //	with a pair of tuples
  //	(L1, PI1), (L2, PI2)
  //
  for (i32 i = 0; i < L1; i++)
  {
    for (i32 j = 0; j < 128; j++)
    {
      if (PI1[j % 32] != 0)
      {
        indexTable[viterbiCounter] = true;
      }
      viterbiCounter++;
    }
  }

  for (i32 i = 0; i < L2; i++)
  {
    for (i32 j = 0; j < 128; j++)
    {
      if (PI2[j % 32] != 0)
      {
        indexTable[viterbiCounter] = true;
      }
      viterbiCounter++;
    }
  }
  //	we had a final block of 24 bits  with puncturing according to PI_X
  //	This block constitues the 6 * 4 bits of the register itself.
  for (i32 i = 0; i < 24; i++)
  {
    if (PI_X[i] != 0)
    {
      indexTable[viterbiCounter] = true;
    }
    viterbiCounter++;
  }
}
