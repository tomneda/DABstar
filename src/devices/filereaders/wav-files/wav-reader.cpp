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
  timeval tv;
  gettimeofday(&tv, nullptr);
  return ((i64)tv.tv_sec * 1000000 + (i64)tv.tv_usec);
}

WavReader::WavReader(WavFileHandler * ipWavFH, SNDFILE * ipFile, RingBuffer<cf32> * ipRingBuffer, const i32 iSampleRate)
  : mpFile(ipFile)
  , mpBuffer(ipRingBuffer)
  , mpParent(ipWavFH)
  , mSampleRate(iSampleRate)
{
  mFileLength = sf_seek(ipFile, 0, SEEK_END);
  sf_seek(ipFile, 0, SEEK_SET);
  mContinuous.store(ipWavFH->cbLoopFile->isChecked());

  if (mSampleRate != INPUT_RATE)
  {
    // we process chunks of 1 msec
    mConvBufferSize = mSampleRate / 1000;
    mConvBuffer.resize(mConvBufferSize + 1);
    for (i32 i = 0; i < 2048; i++)
    {
      const f32 inVal = f32(iSampleRate) / 1000.0f;
      mMapTable_int[i] = (i16)(std::floor((f32)i * (inVal / 2048.0f)));
      mMapTable_float[i] = (f32)i * (inVal / 2048.0f) - (f32)mMapTable_int[i];
      //qDebug() << i << mMapTable_int[i] << mMapTable_float[i];
    }
    mConvIndex = 0;
  }

  mPeriod_us = 32768LL * 1'000'000LL/*us*/ / mSampleRate;  // full IQs read
  qInfo() << "FileLength =" << mFileLength / 1024 << "kB, Period =" << mPeriod_us << "us, SampleRate =" << mSampleRate / 1e3 << "kSps";
}

WavReader::~WavReader()
{
  stop_reader();
}

void WavReader::start_reader()
{
  QThread::start();
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
  std::array<cf32, bufferSize> bi;

  connect(this, &WavReader::signal_set_progress, mpParent, &WavFileHandler::slot_set_progress);
  
  sf_seek(mpFile, 0, SEEK_SET);

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

      if (++cnt >= 20)  // about 3 times in a second
      {
        const i32 xx = (i32)sf_seek(mpFile, 0, SEEK_CUR);
        const f32 progress = (f32)xx / (f32)mFileLength;
        signal_set_progress((i32)(progress * 100), (f32)xx / 2500000.0f);
        cnt = 0;
      }

      nextStop += mPeriod_us;

      const i32 n = (i32)sf_readf_float(mpFile, (f32 *)bi.data(), bufferSize);

      if (n < bufferSize)
      {
        // qInfo("End of file reached");

        sf_seek(mpFile, 0, SEEK_SET);
        for (i32 i = n; i < bufferSize; i++)
        {
          bi[i] = std::complex<f32>(0, 0);
        }
        if (!mContinuous.load())
	      break;
      }

      if (mSampleRate != INPUT_RATE)
      {
        std::array<cf32, 2048> temp;

        for (u32 i = 0; i < bufferSize; ++i)
        {
          mConvBuffer[mConvIndex++] = bi[i];

          if (mConvIndex > mConvBufferSize)
          {
            for (i32 j = 0; j < 2048; j++)
            {
              const i16 inpBase = mMapTable_int[j];
              const f32 inpRatio = mMapTable_float[j];
              temp[j] = mConvBuffer[inpBase + 1] * inpRatio + mConvBuffer[inpBase] * (1 - inpRatio);
            }
            mpBuffer->put_data_into_ring_buffer(temp.data(), 2048);
            mConvBuffer[0] = mConvBuffer[mConvBufferSize];
            mConvIndex = 1;
          }
        }
      }
      else
      {
        mpBuffer->put_data_into_ring_buffer(bi.data(), bufferSize);
      }

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

