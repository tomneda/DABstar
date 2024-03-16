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
#include  "sample-reader.h"

constexpr int32_t C_LEVEL_SIZE = 50;

TimeSyncer::TimeSyncer(SampleReader * mr)
  : mpSampleReader(mr)
{
}

TimeSyncer::EState TimeSyncer::read_samples_until_end_of_level_drop(const int32_t iT_null, const int32_t iT_F)
{
  float cLevel = 0;
  int counter = 0;
  auto * const envBuffer = make_vla(float, mcSyncBufferSize);
  const int syncBufferMask = mcSyncBufferSize - 1;
  int i;

  mSyncBufferIndex = 0;

  for (i = 0; i < C_LEVEL_SIZE; i++)
  {
    const cmplx sample = mpSampleReader->getSample(0);
    envBuffer[mSyncBufferIndex] = fast_abs(sample);
    cLevel += envBuffer[mSyncBufferIndex];
    ++mSyncBufferIndex;
  }

  //SyncOnNull:
  counter = 0;
  while (cLevel / C_LEVEL_SIZE > 0.55 * mpSampleReader->get_sLevel())
  {
    const cmplx sample = mpSampleReader->getSample(0);
    envBuffer[mSyncBufferIndex] = fast_abs(sample);
    cLevel += envBuffer[mSyncBufferIndex] - envBuffer[(mSyncBufferIndex - C_LEVEL_SIZE) & syncBufferMask];
    mSyncBufferIndex = (mSyncBufferIndex + 1) & syncBufferMask;
    ++counter;
    if (counter > iT_F)
    { // hopeless
      return EState::NO_DIP_FOUND;
    }
  }
  /**
    *     It seemed we found a dip that started app 65/100 * 50 samples earlier.
    *     We now start looking for the end of the null period.
    */
  counter = 0;
  //SyncOnEndNull:
  while (cLevel / C_LEVEL_SIZE < 0.75 * mpSampleReader->get_sLevel())
  {
    cmplx sample = mpSampleReader->getSample(0);
    envBuffer[mSyncBufferIndex] = fast_abs(sample);
    //      update the levels
    cLevel += envBuffer[mSyncBufferIndex] - envBuffer[(mSyncBufferIndex - C_LEVEL_SIZE) & syncBufferMask];
    mSyncBufferIndex = (mSyncBufferIndex + 1) & syncBufferMask;
    ++counter;
    if (counter > iT_null + 50)
    { // hopeless
      return EState::NO_END_OF_DIP_FOUND;
    }
  }

  return EState::TIMESYNC_ESTABLISHED;
}
