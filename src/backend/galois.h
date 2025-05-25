/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2010, 2011, 2012
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

#ifndef  GALOIS_H
#define  GALOIS_H

#include "glob_data_types.h"
#include <vector>

class Galois
{
private:
  u16 mm;    /* Bits per symbol */
  u16 gfpoly;
  u16 codeLength;  /* Symbols per block (= (1<<mm)-1) */
  u16 d_q;
  std::vector<u16> alpha_to;  /* log lookup table */
  std::vector<u16> index_of;  /* Antilog lookup table */

public:
  Galois(u16 mm, u16 poly);
  ~Galois() = default;

  int modnn(int);
  u16 add_poly(u16 a, u16 b);
  u16 add_power(u16 a, u16 b);
  u16 multiply_poly(u16 a, u16 b);  // a*b
  u16 multiply_power(u16 a, u16 b);
  u16 divide_poly(u16 a, u16 b);  // a/b
  u16 divide_power(u16 a, u16 b);
  u16 pow_poly(u16 a, u16 n);  // a^n
  u16 pow_power(u16 a, u16 n);
  u16 power2poly(u16 a);
  u16 poly2power(u16 a);
  u16 inverse_poly(u16 a);
  u16 inverse_power(u16 a);
};

#endif
