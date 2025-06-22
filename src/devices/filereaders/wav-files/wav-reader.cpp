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

static inline i64 getMyTime()
{
  timeval tv;
  gettimeofday(&tv, nullptr);
  return ((i64)tv.tv_sec * 1000000LL + (i64)tv.tv_usec);
}

WavReader::WavReader(WavFileHandler * ipWavFH, SNDFILE * ipFile, RingBuffer<cf32> * ipRingBuffer, const i32 iSampleRate)
  : mpFile(ipFile)
  , mpRingBuffer(ipRingBuffer)
  , mpParent(ipWavFH)
  , mSampleRate(iSampleRate)
{
  mFileLength = sf_seek(ipFile, 0, SEEK_END);
  sf_seek(ipFile, 0, SEEK_SET);
  mContinuous.store(ipWavFH->cbLoopFile->isChecked());
  mCmplxBuffer.resize(cBufferSize);

  if (mSampleRate != INPUT_RATE)
  {
#ifdef USE_LIQUID_RESAMPLER

    unsigned int h_len = 13;    // filter semi-length (filter delay)
    const float r = (float)INPUT_RATE / (float)mSampleRate;               // resampling rate (output/input)
    const float bw = 0.4f;              // resampling filter bandwidth
    const float slsl = 60.0f;          // resampling filter sidelobe suppression level
    const unsigned int npfb = 32;       // number of filters in bank (timing resolution)

    mConvBuffer.resize(mCmplxBuffer.size() * r + 10);

    // create resampler
    mLiquidResampler = resamp_crcf_create(r, h_len, bw, slsl, npfb);
    resamp_crcf_print(mLiquidResampler);
#else
    // we process chunks of 1 msec
    mConvBufferSize = (i16)(mSampleRate / 1000);
    mConvBuffer.resize(mConvBufferSize + 1);
    mResampBuffer.resize(2048);
    mMapTable_int.resize(2048);
    mMapTable_float.resize(2048);

    for (i32 i = 0; i < 2048; i++)
    {
      const f32 inVal = (f32)mSampleRate / 1000.0f;
      mMapTable_int[i] = (i16)(std::floor((f32)i * (inVal / 2048.0f)));
      mMapTable_float[i] = (f32)i * (inVal / 2048.0f) - (f32)mMapTable_int[i];
      //qDebug() << i << mMapTable_int[i] << mMapTable_float[i];
    }
    mConvIndex = 0;
#endif

  }

  mPeriod_us = 32768LL * 1'000'000LL/*us*/ / mSampleRate;  // full IQs read
  qInfo() << "FileLength =" << mFileLength / 1024 << "kB, Period =" << mPeriod_us << "us, SampleRate =" << mSampleRate / 1e3 << "kSps";
}

WavReader::~WavReader()
{
  stop_reader();
#ifdef USE_LIQUID_RESAMPLER
  if (mSampleRate != INPUT_RATE)
  {
    resamp_crcf_destroy(mLiquidResampler);
  }
#endif
}

void WavReader::start_reader()
{
  mSetNewFilePos = -1;
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
  mSetNewFilePos = -1;
}

void WavReader::jump_to_relative_position_per_mill(i32 iPerMill)
{
  if (mRunning.load())
  {
    mSetNewFilePos = (i64)(mFileLength * iPerMill / 1000LL);
  }
}

void WavReader::run()
{
  connect(this, &WavReader::signal_set_progress, mpParent, &WavFileHandler::slot_set_progress);

  sf_seek(mpFile, 0, SEEK_SET);

  mRunning.store(true);

  i32 cnt = 0;
  i64 nextStop_us = getMyTime();

  while (mRunning.load())
  {
    while (mRunning.load() && mpRingBuffer->get_ring_buffer_write_available() < cBufferSize)
    {
      usleep(100);
    }

    if (mSetNewFilePos >= 0)
    {
      sf_seek(mpFile, mSetNewFilePos, SEEK_SET);
      mSetNewFilePos = -1;
      cnt = 10; // retrigger emit below
    }

    if (++cnt >= 10)  // about 6 times in a second
    {
      const i64 filePos = sf_seek(mpFile, 0, SEEK_CUR);
      emit signal_set_progress((i32)(1000 * filePos / mFileLength), (f32)filePos / (f32)mSampleRate);
      cnt = 0;
    }

    nextStop_us += mPeriod_us;

    const i32 n = (i32)sf_readf_float(mpFile, (f32 *)mCmplxBuffer.data(), cBufferSize);

    if (n < cBufferSize)
    {
      // qInfo("End of file reached");

      sf_seek(mpFile, 0, SEEK_SET);
      for (i32 i = n; i < cBufferSize; i++)
      {
        mCmplxBuffer[i] = std::complex<f32>(0, 0);
      }
      if (!mContinuous.load())
      {
        break;
      }
    }

    if (mSampleRate != INPUT_RATE)
    {
#ifdef USE_LIQUID_RESAMPLER
      u32 usedResultSamples = 0;
      resamp_crcf_execute_block(mLiquidResampler, mCmplxBuffer.data(), mCmplxBuffer.size(), mConvBuffer.data(), &usedResultSamples);
      assert(usedResultSamples <= mConvBuffer.size());
      mpRingBuffer->put_data_into_ring_buffer(mConvBuffer.data(), usedResultSamples);

#else
      for (u32 i = 0; i < cBufferSize; ++i)
      {
        mConvBuffer[mConvIndex++] = mCmplxBuffer[i];

        if (mConvIndex > mConvBufferSize)
        {
          for (i32 j = 0; j < 2048; j++)
          {
            const i16 inpBase = mMapTable_int[j];
            const f32 inpRatio = mMapTable_float[j];
            mResampBuffer[j] = mConvBuffer[inpBase + 1] * inpRatio + mConvBuffer[inpBase] * (1 - inpRatio);
          }
          mpRingBuffer->put_data_into_ring_buffer(mResampBuffer.data(), 2048);
          mConvBuffer[0] = mConvBuffer[mConvBufferSize];
          mConvIndex = 1;
        }
      }
#endif
    }
    else
    {
      mpRingBuffer->put_data_into_ring_buffer(mCmplxBuffer.data(), cBufferSize);
    }

    if (nextStop_us - getMyTime() > 0)
    {
      usleep(nextStop_us - getMyTime());
    }
  }

  qInfo("Task for replay ended");
}

bool WavReader::handle_continuous_button()
{
  mContinuous.store(!mContinuous.load());
  return mContinuous.load();
}

