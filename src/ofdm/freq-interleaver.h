/*
 *    Copyright (C) 2013 .. 2020
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
#ifndef  FREQ_INTERLEAVER_H
#define  FREQ_INTERLEAVER_H

#include  <cstdint>
#include  <vector>
#include  "dab-constants.h"
#include  "dab-params.h"

/**
  *	\class FreqInterleaver
  *	Implements frequency interleaving according to section 14.6
  *	of the DAB standard
  */
class FreqInterleaver
{
public:
  explicit FreqInterleaver(uint8_t iDabMode);
  ~FreqInterleaver() = default;

  //	according to the standard, the map is a function from
  //	0 .. 1535 -> -768 .. 768 (with exclusion of {0})
  [[nodiscard]] inline int16_t map_k_to_fft_bin(int16_t k) const { return mPermTable[k]; }

private:
  const DabParams::SDabPar mDabPar;
  std::vector<int16_t> mPermTable;

  void createMapper(int16_t V1, int16_t lwb, int16_t upb);
};

#endif

