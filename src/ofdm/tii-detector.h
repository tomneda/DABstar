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

#pragma once

#include "dab-constants.h"
#include  "dab-params.h"
#include "fft/fft-handler.h"

#define NUM_GROUPS   8
#define GROUPSIZE   24

struct STiiResult
{
  uint8_t mainId;
  uint8_t subId;
  float strength;
  float phase;
  bool norm;
};

class TiiDetector
{
public:
  TiiDetector(uint8_t dabMode);
  ~TiiDetector();
  void reset();
  void set_collisions(bool);
  void set_subid(uint8_t);
  void addBuffer(const std::vector<cmplx> &);
  std::vector<STiiResult> processNULL(int16_t);

private:
  const DabParams params;
  const int16_t T_u;
  const int16_t T_g;
  const int16_t K;
  bool collisions = false;
  bool carrier_delete = true;
  uint8_t selected_subid = 0;
  cmplx decodedBuffer[768];
  std::vector<cmplx> nullSymbolBuffer;
  FftHandler mFftHandler;

  void resetBuffer();
  void decode(std::vector<cmplx> &, cmplx *);
  void collapse(const cmplx *, cmplx *, cmplx *);
};
