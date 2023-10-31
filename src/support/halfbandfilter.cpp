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
#include "halfbandfilter.h"
#include <liquid/liquid.h>
#include <cassert>

HalfBandFilter::HalfBandFilter(uint32_t iDecimationPow)
{
  mLevelVec.resize(iDecimationPow);

  for (auto & level : mLevelVec)
  {
    level.qResamp2 = resamp2_crcf_create(7, 0.0f, 60.0f);
    resamp2_crcf_print(level.qResamp2);
    level.InBufferIdx = 0;
  }
}

HalfBandFilter::~HalfBandFilter()
{
  for (auto & level : mLevelVec)
  {
    resamp2_crcf_destroy(level.qResamp2);
  }
}

bool HalfBandFilter::decimate(const cmplx & iX, cmplx & oY, uint32_t iLevel)
{
  assert(iLevel < mLevelVec.size());
  SLevel & level = mLevelVec[iLevel];
  level.InBuffer[level.InBufferIdx] = iX;
  ++level.InBufferIdx;

  if (level.InBufferIdx >= 2)
  {
    level.InBufferIdx = 0;

    cmplx y;
    resamp2_crcf_decim_execute(level.qResamp2, level.InBuffer.data(), &y);

    if (iLevel + 1 >= mLevelVec.size())
    {
      oY = y;
      return true;
    }

    return decimate(y, oY, iLevel + 1);
  }
  return false;
}
