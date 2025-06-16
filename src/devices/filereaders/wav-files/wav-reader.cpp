/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
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

#include  "wav-reader.h"
#include  "wavfiles.h"
#include  "device-exceptions.h"
#include  <sys/time.h>
#include  <cinttypes>

#define  BUFFERSIZE  32768

static inline i64 getMyTime()
{
  struct timeval tv;

  gettimeofday(&tv, nullptr);
  return ((i64)tv.tv_sec * 1000000 + (i64)tv.tv_usec);
}

WavReader::WavReader(WavFileHandler * mr, SNDFILE * filePointer, RingBuffer<cf32> * theBuffer)
  : mpFilePointer(filePointer)
  , mpBuffer(theBuffer)
  , mpParent(mr)
{
  mFileLength = sf_seek(filePointer, 0, SEEK_END);
  sf_seek(filePointer, 0, SEEK_SET);
  mPeriod = (32768 * 1000) / (2048);  // full IQs read
  qInfo() << "FileLength =" << mFileLength << ", period =" << mPeriod;
  mRunning.store(false);
  mContinuous.store(mr->cbLoopFile->isChecked());
  start();
}

WavReader::~WavReader()
{
  stop_reader();
}

void WavReader::stop_reader()
{
  if (mRunning.load())
  {
    mRunning.store(false);
    while (isRunning())
    {
      usleep(200);
    }
  }
}

void WavReader::run()
{
  constexpr i32 bufferSize = 32768;
  auto * const bi = make_vla(cf32, bufferSize);

  connect(this, &WavReader::signal_set_progress, mpParent, &WavFileHandler::slot_set_progress);
  
  sf_seek(mpFilePointer, 0, SEEK_SET);

  mRunning.store(true);

  i32 cnt = 0;
  i64 nextStop = getMyTime();

  try
  {
    while (mRunning.load())
    {
      while (mpBuffer->get_ring_buffer_write_available() < bufferSize)
      {
        if (!mRunning.load())
        {
          throw (33);
        }
        usleep(100);
      }

      if (++cnt >= 20)
      {
        const i32 xx = sf_seek(mpFilePointer, 0, SEEK_CUR);
        const f32 progress = (f32)xx / (f32)mFileLength;
        signal_set_progress((i32)(progress * 100), (f32)xx / 2048000);
        cnt = 0;
      }

      nextStop += mPeriod;

      const i32 n = (i32)sf_readf_float(mpFilePointer, (f32 *)bi, bufferSize);

      if (n < bufferSize)
      {
        qInfo("End of file reached");

        sf_seek(mpFilePointer, 0, SEEK_SET);
        for (i32 i = n; i < bufferSize; i++)
        {
          bi[i] = std::complex<f32>(0, 0);
        }
        if (!mContinuous.load())
	      break;
      }

      mpBuffer->put_data_into_ring_buffer(bi, bufferSize);

      if (nextStop - getMyTime() > 0)
      {
        usleep(nextStop - getMyTime());
      }
    }
  }
  catch (i32 e)
  {
  }

  qInfo("Task for replay ended");
}

bool WavReader::handle_continuous_button()
{
  mContinuous.store(!mContinuous.load());
  return mContinuous.load();
}

