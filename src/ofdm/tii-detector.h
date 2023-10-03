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

#ifndef  TII_DETECTOR_H
#define  TII_DETECTOR_H

#include "dab-constants.h"
#include  <cstdint>
#include  "dab-params.h"
#include  <vector>

class TiiDetector
{
public:
  TiiDetector(uint8_t dabMode, int16_t);
  ~TiiDetector() = default;
  void reset();
  void setMode(bool);
  void addBuffer(std::vector<cmplx>);  // copy of vector is intended
  uint16_t processNULL();

private:
  const int16_t mDepth;
  const DabParams mParams;
  const int16_t mT_u;
  const int16_t mK;
  bool mDetectMode_new = false;
  std::array<uint8_t, 256> mInvTable;
  std::vector<cmplx> mBuffer;
  std::vector<float> mWindow;

  void collapse(cmplx *, float *);
};

#endif

