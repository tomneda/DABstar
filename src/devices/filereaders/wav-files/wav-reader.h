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
#ifndef	__WAV_READER__
#define	__WAV_READER__

#include	<QThread>
#include	<sndfile.h>
#include	"dab-constants.h"
#include	"ringbuffer.h"
#include	<atomic>

class WavFileHandler;

class WavReader : public QThread
{
  Q_OBJECT

public:
  WavReader(WavFileHandler *, SNDFILE *, RingBuffer<cf32> *, i32 iSampleRate);
  ~WavReader();

  void start_reader();
  void stop_reader();
  void jump_to_relative_position_per_mill(i32 iPerMill);
  bool handle_continuous_button();

private:
  static constexpr i32 cBufferSize = 32768;

  SNDFILE * const mpFile;
  RingBuffer<cf32> * const mpRingBuffer;
  WavFileHandler * const mpParent;
  const i32 mSampleRate;
  std::atomic<bool> mRunning = false;
  std::atomic<bool> mContinuous = false;
  std::atomic<i64> mSetNewFilePos = -1;
  i64 mFileLength = 0;
  i64 mPeriod_us = 0;

  i16 mConvBufferSize;
  i16 mConvIndex = 0;
  std::vector<cf32> mCmplxBuffer;
  std::vector<cf32> mConvBuffer;
  std::vector<cf32> mResampBuffer;
  std::vector<i16> mMapTable_int;
  std::vector<f32> mMapTable_float;


  void run() override;

signals:
  void signal_set_progress(i32, f32);
};

#endif
