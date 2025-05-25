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

#include "raw-reader.h"
#include "rawfiles.h"
#include <QLoggingCategory>
#include <sys/time.h>
#include <cinttypes>

#ifdef _WIN32
#else
  #include  <unistd.h>
#endif

Q_LOGGING_CATEGORY(sLogRawReader, "RawReader", QtInfoMsg)

static inline i64 get_cur_time_in_us()
{
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return ((i64)tv.tv_sec * 1000000 + (i64)tv.tv_usec);
}

RawReader::RawReader(RawFileHandler * ipRFH, FILE * ipFile, RingBuffer<cf32> * ipRingBuffer)
  : mParent(ipRFH)
  , mpFile(ipFile)
  , mpRingBuffer(ipRingBuffer)
{
  mByteBuffer.resize(BUFFERSIZE);
  mCmplxBuffer.resize(BUFFERSIZE / 2);

  for(int i = 0; i < 256; i++)
  {
    // the offset 127.38f is due to the input data comes usually from a SDR stick which has its DC offset a bit shifted from ideal (from old-dab)
    mMapTable[i] = ((f32)i - 127.38f) / 128.0f;
  }

  fseek(ipFile, 0, SEEK_END);
  mFileLength = ftell(ipFile);
  qCInfo(sLogRawReader) << "RAW file length:" << mFileLength;
  fseek(ipFile, 0, SEEK_SET);
  mRunning.store(false);
  continuous.store(ipRFH->cbLoopFile->isChecked());
  start();
}

RawReader::~RawReader()
{
  stopReader();
}

void RawReader::stopReader()
{
  if (mRunning)
  {
    mRunning = false;
    while (isRunning())
    {
      usleep(200);
    }
  }
}

void RawReader::run()
{
  connect(this, &RawReader::signal_set_progress, mParent, &RawFileHandler::setProgress);

  fseek(mpFile, 0, SEEK_SET);
  mRunning.store(true);

  qCInfo(sLogRawReader) << "Start playing RAW file";

  int cnt = 0;
  i64 nextStopus = get_cur_time_in_us();

  try
  {
    while (mRunning.load())
    {
      while (mpRingBuffer->get_ring_buffer_write_available() < BUFFERSIZE + 10)
      {
        if (!mRunning.load())
        {
          throw (32);
        }
        usleep(100);
      }

      if (++cnt >= 20)
      {
        int xx = ftell(mpFile);
        f32 progress = (f32)xx / mFileLength;
        signal_set_progress((int)(progress * 100), (f32)xx / (2 * 2048000));
        cnt = 0;
      }

      int n = fread(mByteBuffer.data(), sizeof(u8), BUFFERSIZE, mpFile);

      if (n < BUFFERSIZE)
      {
        qCInfo(sLogRawReader) << "Restart file";
        fseek(mpFile, 0, SEEK_SET);
        if (!continuous.load())
	      break;
      }

      nextStopus += (n * 1000) / (2 * 2048); // add runtime in us for n numbers of entries

      for (int i = 0; i < n / 2; i++)
      {
        mCmplxBuffer[i] = cf32(mMapTable[mByteBuffer[2 * i + 0]], mMapTable[mByteBuffer[2 * i + 1]]);
      }

      mpRingBuffer->put_data_into_ring_buffer(mCmplxBuffer.data(), n / 2);

      if (nextStopus - get_cur_time_in_us() > 0)
      {
        usleep(nextStopus - get_cur_time_in_us());
      }
    }
  }
  catch (int e)
  {
  }

  qCInfo(sLogRawReader) << "Playing RAW file stopped";
}

bool RawReader::handle_continuousButton()
{
  continuous.store(!continuous.load());
  return continuous.load();
}

