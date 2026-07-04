/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013, 2014, 2015, 2016, 2017, 2018, 2019
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
#include  "timesyncer.h"
#include  "sample_reader.h"
#include  <QDebug>

TimeSyncer::TimeSyncer(SampleReader * mr)
  : mpSampleReader(mr)
{
}

TimeSyncer::EState TimeSyncer::read_samples_until_end_of_level_drop()
{
  f32 cLevel = 0;
  std::array<f32, cSyncBufferSize> envBuffer; // init not necessary
  constexpr i32 syncBufferMask = cSyncBufferSize - 1;

  mSyncBufferIndex = 0;

  // collect level information for the first cLevelSearchSize in a buffer
  for (i32 i = 0; i < cLevelSearchSize; i++)
  {
    const cf32 sample = mpSampleReader->get_sample(0);
    envBuffer[mSyncBufferIndex] = std::abs(sample);
    cLevel += envBuffer[mSyncBufferIndex];
    ++mSyncBufferIndex;
  }

  // Search for a level drop in the signal within max TF samples
  i32 counter = 0;
  while (cLevel / cLevelSearchSize > 0.55f * mpSampleReader->get_sLevel())
  {
    const cf32 sample = mpSampleReader->get_sample(0);
    envBuffer[mSyncBufferIndex] = std::abs(sample);
    cLevel += envBuffer[mSyncBufferIndex] - envBuffer[(u32)(mSyncBufferIndex - cLevelSearchSize) & syncBufferMask];
    mSyncBufferIndex = (mSyncBufferIndex + 1) & syncBufferMask;
    ++counter;

    if (counter > cTF) // no DIP found within one frame?
    {
      return EState::NO_DIP_FOUND;
    }
  }

  // We now start looking for the end of the null period.
  counter = 0;
  while (cLevel / cLevelSearchSize < 0.75f * mpSampleReader->get_sLevel())
  {
    const cf32 sample = mpSampleReader->get_sample(0);
    envBuffer[mSyncBufferIndex] = std::abs(sample);
    cLevel += envBuffer[mSyncBufferIndex] - envBuffer[(u32)(mSyncBufferIndex - cLevelSearchSize) & syncBufferMask];
    mSyncBufferIndex = (mSyncBufferIndex + 1) & syncBufferMask;
    ++counter;

    if (counter > cTn + cLevelSearchSize + 20) // no rising edge found within null period? Add an empirical value to make sync more reliable in some cases
    {
      return EState::NO_END_OF_DIP_FOUND;
    }
  }

  return EState::TIMESYNC_ESTABLISHED;
}
