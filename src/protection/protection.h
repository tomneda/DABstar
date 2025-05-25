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
 *	Simple base class for combining uep and eep deconvolvers
 */
#ifndef  PROTECTION_H
#define  PROTECTION_H

#include  <cstdint>
#include  <vector>
#include  "viterbi-spiral.h"

//extern u8 PI_X[];

class Protection : public ViterbiSpiral
{
public:
  explicit Protection(i16);
  virtual ~Protection() = default;

  virtual bool deconvolve(const i16 *, i32, u8 *);
  
protected:
  i16 bitRate;
  i32 outSize;
  std::vector<u8> indexTable;
  std::vector<i16> viterbiBlock;
};

#endif

