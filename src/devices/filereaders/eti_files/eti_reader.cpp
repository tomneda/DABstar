/*
 * Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
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

#include "eti_reader.h"
#include "etifiles.h"
#include <QLoggingCategory>
#include <cstring>
#include <sys/time.h>

#ifndef _WIN32
  #include <unistd.h>
#endif

Q_LOGGING_CATEGORY(sLogEtiReader, "EtiReader", QtInfoMsg)

static constexpr i64 cFrameDuration_us = 24000;

static inline i64 get_cur_time_in_us()
{
  timeval tv;
  gettimeofday(&tv, nullptr);
  return ((i64)tv.tv_sec * 1000000 + (i64)tv.tv_usec);
}

static inline bool is_fsync(const u8 * const ipData)
{
  const u32 fsync = (u32)ipData[0] << 16 | (u32)ipData[1] << 8 | (u32)ipData[2];
  return fsync == 0x073AB6 || fsync == 0xF8C549;
}

EtiReader::EtiReader(EtiFileHandler * const ipParent, FILE * const ipFile, RingBuffer<u8> * const ipRingBuffer)
  : mpFile(ipFile)
  , mpRingBuffer(ipRingBuffer)
  , mpParent(ipParent)
{
  fseek(ipFile, 0, SEEK_END);
  mFileLength = ftell(ipFile);
  fseek(ipFile, 0, SEEK_SET);
  qCInfo(sLogEtiReader) << "ETI file length:" << mFileLength << "bytes," << mFileLength / cEtiFrameSize << "frames";
}

EtiReader::~EtiReader()
{
  stop_reader();
}

void EtiReader::start_reader()
{
  mSetNewFilePos = -1;
  QThread::start();
}

void EtiReader::stop_reader()
{
  if (mRunning.load())
  {
    mRunning = false;
    while (isRunning())
    {
      usleep(1000);
    }
  }
  mSetNewFilePos = -1;
}

void EtiReader::jump_to_relative_position_per_mill(const i32 iPerMill)
{
  if (mRunning.load())
  {
    // the position has not to be aligned to a frame start, the sync search will find the next frame
    mSetNewFilePos = (i64)(mFileLength * iPerMill / 1000LL);
  }
}

// Fills mFrame with a complete frame starting with ERR + FSYNC. The search for FSYNC is done like in dablin.
// Returns false on end of file.
bool EtiReader::_read_next_frame(i64 & oFrameStartPos)
{
  while (mRunning.load())
  {
    const size_t n = fread(&mFrame[mFilled], 1, cEtiFrameSize - mFilled, mpFile);
    mFilled += (i32)n;

    if (mFilled < cEtiFrameSize)
    {
      return false; // end of file reached, the incomplete frame is discarded
    }

    // FSYNC is located at offset 1 (after the ERR byte)
    i32 offset = 0;
    while (offset < cEtiFrameSize - 3 && !is_fsync(&mFrame[offset + 1]))
    {
      ++offset;
    }

    if (offset == 0)
    {
      oFrameStartPos = ftell(mpFile) - cEtiFrameSize;
      mFilled = 0;
      return true;
    }

    // not (yet) in sync, discard the buffer begin and read further
    memmove(&mFrame[0], &mFrame[offset], cEtiFrameSize - offset);
    mFilled -= offset;
  }

  return false;
}

void EtiReader::run()
{
  connect(this, &EtiReader::signal_set_progress, mpParent, &EtiFileHandler::signal_file_progress);
  connect(this, &EtiReader::signal_file_looped, mpParent, &EtiFileHandler::signal_file_looped);

  fseek(mpFile, 0, SEEK_SET);
  mFilled = 0;

  mRunning.store(true);

  qCInfo(sLogEtiReader) << "Start playing ETI file";

  i32 cnt = 0;
  i64 nextStop_us = get_cur_time_in_us();

  while (mRunning.load())
  {
    while (mRunning.load() && mpRingBuffer->get_ring_buffer_write_available() < cEtiFrameSize)
    {
      usleep(1000);
    }

    if (mSetNewFilePos >= 0)
    {
      fseek(mpFile, mSetNewFilePos, SEEK_SET);
      mSetNewFilePos = -1;
      mFilled = 0;
      cnt = 10; // retrigger emit below
    }

    if (mPaused.load())
    {
      usleep(10000);
      nextStop_us = get_cur_time_in_us(); // do not catch up the paused time
      continue;
    }

    i64 frameStartPos = 0;

    if (!_read_next_frame(frameStartPos))
    {
      if (!mRunning.load())
      {
        break;
      }
      fseek(mpFile, 0, SEEK_SET);
      mFilled = 0;
      emit signal_file_looped();
      continue;
    }

    if (++cnt >= 10)
    {
      const f32 elapsed_s = (f32)frameStartPos / (f32)cEtiFrameSize * ((f32)cFrameDuration_us / 1e6f);
      emit signal_set_progress((i32)(1000 * frameStartPos / mFileLength), elapsed_s);
      cnt = 0;
    }

    mpRingBuffer->put_data_into_ring_buffer(mFrame.data(), cEtiFrameSize);

    nextStop_us += cFrameDuration_us;
    const i64 waitTime_us = nextStop_us - get_cur_time_in_us();

    if (waitTime_us > 0)
    {
      usleep(waitTime_us);
    }
    else if (waitTime_us < -10 * cFrameDuration_us)
    {
      nextStop_us = get_cur_time_in_us(); // do not try to catch up a too long delay
    }
  }

  qCInfo(sLogEtiReader) << "Playing ETI file stopped";
}
