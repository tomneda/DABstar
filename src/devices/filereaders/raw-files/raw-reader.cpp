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
  : mpFile(ipFile)
  , mpRingBuffer(ipRingBuffer)
  , mParent(ipRFH)
{
  fseek(ipFile, 0, SEEK_END);
  mFileLength = ftell(ipFile);
  qCInfo(sLogRawReader) << "RAW file length:" << mFileLength;
  fseek(ipFile, 0, SEEK_SET);
  continuous.store(ipRFH->cbLoopFile->isChecked());

  mByteBuffer.resize(cBufferSize);
  mCmplxBuffer.resize(cBufferSize / 2);

  for(i32 i = 0; i < 256; i++)
  {
    // the offset 127.38f is due to the input data comes usually from a SDR stick which has its DC offset a bit shifted from ideal (from old-dab)
    mMapTable[i] = ((f32)i - 127.38f) / 128.0f;
  }
}

RawReader::~RawReader()
{
  stop_reader();
}

void RawReader::start_reader()
{
  mSetNewFilePos = -1;
  QThread::start();
}

void RawReader::stop_reader()
{
  if (mRunning.load())
  {
    mRunning = false;
    while (isRunning())
    {
      usleep(200);
    }
  }
  mSetNewFilePos = -1;
}

void RawReader::jump_to_relative_position_per_mill(i32 iPerMill)
{
  if (mRunning.load())
  {
    mSetNewFilePos = (i64)(mFileLength * iPerMill / 1000LL) & ~1LL; // ensure even value to maintain I/Q position
  }
}

void RawReader::run()
{
  connect(this, &RawReader::signal_set_progress, mParent, &RawFileHandler::slot_set_progress);

  fseek(mpFile, 0, SEEK_SET);

  mRunning.store(true);

  qCInfo(sLogRawReader) << "Start playing RAW file";

  i32 cnt = 0;
  i64 nextStop_us = get_cur_time_in_us();

  try
  {
    while (mRunning.load())
    {
      while (mpRingBuffer->get_ring_buffer_write_available() < cBufferSize + 10)
      {
        if (!mRunning.load())
        {
          throw (32);
        }
        usleep(100);
      }

      if (mSetNewFilePos >= 0)
      {
        fseek(mpFile, mSetNewFilePos, SEEK_SET);
        mSetNewFilePos = -1 ;
        cnt = 10; // retrigger emit below
      }

      if (++cnt >= 10)
      {
        const i64 filePos = ftell(mpFile);
        emit signal_set_progress((i32)(1000 * filePos / mFileLength), (f32)filePos / (2 * 2048000.0f));
        cnt = 0;
      }

      const i32 n = (i32)fread(mByteBuffer.data(), sizeof(u8), cBufferSize, mpFile);

      if (n < cBufferSize)
      {
        // qCInfo(sLogRawReader) << "Restart file";
        fseek(mpFile, 0, SEEK_SET);
        if (!continuous.load())
        {
          break;
        }
      }

      nextStop_us += (n * 1000) / (2 * 2048); // add runtime in us for n numbers of entries

      for (i32 i = 0; i < n / 2; i++)
      {
        mCmplxBuffer[i] = cf32(mMapTable[mByteBuffer[2 * i + 0]], mMapTable[mByteBuffer[2 * i + 1]]);
      }

      mpRingBuffer->put_data_into_ring_buffer(mCmplxBuffer.data(), n / 2);

      if (nextStop_us - get_cur_time_in_us() > 0)
      {
        usleep(nextStop_us - get_cur_time_in_us());
      }
    }
  }
  catch (i32 e)
  {
  }

  qCInfo(sLogRawReader) << "Playing RAW file stopped";
}

bool RawReader::handle_continuous_button()
{
  continuous.store(!continuous.load());
  return continuous.load();
}

