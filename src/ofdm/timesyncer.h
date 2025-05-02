/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014.. 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB 
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
#ifndef  TIMESYNCER_H
#define  TIMESYNCER_H

#include  "dab-constants.h"

class SampleReader;

class TimeSyncer
{
public:
  enum class EState
  {
    TIMESYNC_ESTABLISHED, NO_DIP_FOUND, NO_END_OF_DIP_FOUND
  };

  TimeSyncer(SampleReader * mr);
  ~TimeSyncer() = default;

  EState read_samples_until_end_of_level_drop();

private:
  static constexpr int32_t cSyncBufferSize = 4096;
  static constexpr int32_t cLevelSearchSize = 50;

  SampleReader * const mpSampleReader;
  int32_t mSyncBufferIndex = 0;
};

#endif

