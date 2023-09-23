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
 *
 *	Simple base class for combining uep and eep deconvolvers
 */
#include  <vector>
#include  "protection.h"

Protection::Protection(int16_t iBitRate) :
  ViterbiSpiral(24 * iBitRate, true),
  bitRate(iBitRate),
  outSize(24 * iBitRate),
  indexTable(outSize * 4 + 24),
  viterbiBlock(outSize * 4 + 24)
{
}

bool Protection::deconvolve(const int16_t * iV, int32_t size, uint8_t * outBuffer)
{
  int16_t inputCounter = 0;
  //	clear the bits in the viterbiBlock,
  //	only the non-punctured ones are set
  memset(viterbiBlock.data(), 0, (outSize * 4 + 24) * sizeof(int16_t));
  //	The actual deconvolution is done by the viterbi decoder

  for (int i = 0; i < outSize * 4 + 24; i++)
  {
    if (indexTable[i])
    {
      viterbiBlock[i] = iV[inputCounter++];
    }
  }

  ViterbiSpiral::deconvolve(viterbiBlock.data(), outBuffer);
  return true;
}
