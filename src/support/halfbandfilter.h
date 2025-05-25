/*
 * Copyright (c) 2023 by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef HALFBANDFILTER_H
#define HALFBANDFILTER_H

#include "glob_defs.h"
#include <cstdlib>
#include <vector>
#include <array>

struct resamp2_crcf_s;

class HalfBandFilter
{
public:
  explicit HalfBandFilter(u32 iDecimationPow = 1);
  ~HalfBandFilter();

  bool decimate(const cf32 & iX, cf32 & oY, u32 iLevel = 0);

private:
  struct SLevel
  {
    resamp2_crcf_s * qResamp2;
    std::array<cf32, 2> InBuffer;
    u32 InBufferIdx = 0;
  };
  std::vector<SLevel> mLevelVec;
};

#endif // HALFBANDFILTER_H
