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
 *    This file is part of the Qt-DAB
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
#include  "freq-interleaver.h"

/**
  *	\brief createMapper
  *	create the mapping table  for the (de-)interleaver
  *	formulas according to section 14.6 (Frequency interleaving)
  *	of the DAB standard
  */

FreqInterleaver::FreqInterleaver()
{
  mPermTable.resize(cTu);

  createMapper(511, 256, 256 + cK); // we use DAB-Mode 1 only
  // Mode 2: createMapper(127,  64,  64 + cK);
  // Mode 3: createMapper(63,   32,  32 + cK);
  // Mode 4: createMapper(255, 128, 128 + cK);
}

void FreqInterleaver::createMapper(const i16 iV1, const i16 iLwb, const i16 iUpb)
{
  auto * const tmp = make_vla(i16, cTu);
  i16 index = 0;
  i16 i;

  tmp[0] = 0;
  for (i = 1; i < cTu; i++)
  {
    tmp[i] = (13 * tmp[i - 1] + iV1) % cTu;
  }
  for (i = 0; i < cTu; i++)
  {
    if (tmp[i] == cTu / 2)
    {
      continue;
    }
    if ((tmp[i] < iLwb) || (tmp[i] > iUpb))
    {
      continue;
    }
    //	we now have a table with values from lwb .. upb
    //
    mPermTable.at(index++) = tmp[i] - cTu / 2;
    //	we now have a table with values from lwb - T_u / 2 .. lwb + T_u / 2
  }
}
