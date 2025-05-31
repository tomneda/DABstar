/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
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
//
//	Pretty straightforward package for galois computing,
//	up to 8 bits symsize

#include  "galois.h"

Galois::Galois(u16 symsize, u16 gfpoly)
{
  u16 sr;
  u16 i;

  this->mm = symsize;
  this->gfpoly = gfpoly;
  this->codeLength = (1 << mm) - 1;
  this->d_q = 1 << mm;
  this->alpha_to.resize(codeLength + 1);
  this->index_of.resize(codeLength + 1);
  /*	Generate Galois field lookup tables */
  index_of[0] = codeLength;  /* log (zero) = -inf */
  alpha_to[codeLength] = 0;  /* alpha**-inf = 0 */

  sr = 1;
  for (i = 0; i < codeLength; i++)
  {
    index_of[sr] = i;
    alpha_to[i] = sr;
    sr <<= 1;
    if (sr & (1 << symsize))
    {
      sr ^= gfpoly;
    }
    sr &= codeLength;
  }
}

i32 Galois::modnn(i32 x)
{
  while (x >= codeLength)
  {
    x -= codeLength;
    x = (x >> mm) + (x & codeLength);
  }
  return x;
}

u16 Galois::add_poly(u16 a, u16 b)
{
  return a ^ b;
}

u16 Galois::poly2power(u16 a)
{
  return index_of[a];
}

u16 Galois::power2poly(u16 a)
{
  return alpha_to[a];
}

u16 Galois::add_power(u16 a, u16 b)
{
  return index_of[alpha_to[a] ^ alpha_to[b]];
}

u16 Galois::multiply_power(u16 a, u16 b)
{
  return modnn(a + b);
}

u16 Galois::multiply_poly(u16 a, u16 b)
{
  if ((a == 0) || (b == 0))
  {
    return 0;
  }
  return alpha_to[multiply_power(index_of[a], index_of[b])];
}

u16 Galois::divide_power(u16 a, u16 b)
{
  return modnn(d_q - 1 + a - b);
}

u16 Galois::divide_poly(u16 a, u16 b)
{
  if (a == 0)
  {
    return 0;
  }
  return alpha_to[divide_power(index_of[a], index_of[b])];
}

u16 Galois::inverse_poly(u16 a)
{
  return alpha_to[inverse_power(index_of[a])];
}

u16 Galois::inverse_power(u16 a)
{
  return d_q - 1 - a;
}

u16 Galois::pow_poly(u16 a, u16 n)
{
  return alpha_to[pow_power(index_of[a], n)];
}

u16 Galois::pow_power(u16 a, u16 n)
{
  return (a == 0) ? 0 : (a * n) % (d_q - 1);
}

