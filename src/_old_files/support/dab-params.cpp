/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013, 2014, 2015, 2016, 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB (formerly SDR-J, JSDR).
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
#include <cassert>
#include "dab-params.h"

/*static*/ const DabParams::TArrDabPar DabParams::msDabPar {{
//  L    K     T_n   T_F     T_s   T_u   T_g  cDiff CIFs
  { 0,   0,    0,    0,      0,    0,    0,   0,    0 },  // dummy, not used
  { 76,  1536, 2656, 196608, 2552, 2048, 504, 1000, 4 },  // Mode 1 (default)
  { 76,  384,  664,  49152,  638,  512,  126, 4000, 1 },  // Mode 2
  { 153, 192,  345,  49152,  319,  256,  63,  2000, 1 },  // Mode 3
  { 76,  768,  1328, 98304,  1276, 1024, 252, 2000, 2 }   // Mode 4
} };

DabParams::DabParams(uint8_t iDabMode)
: mDabMode(iDabMode)
{
  assert(iDabMode > 0);
  assert(iDabMode < msDabPar.size());
}
