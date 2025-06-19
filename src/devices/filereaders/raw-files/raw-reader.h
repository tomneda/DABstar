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
#ifndef  RAW_READER_H
#define  RAW_READER_H

#include  <QThread>
#include  "dab-constants.h"
#include  "ringbuffer.h"
#include  <atomic>

class RawFileHandler;

class RawReader : public QThread
{
Q_OBJECT

public:
  RawReader(RawFileHandler *, FILE *, RingBuffer<cf32> *);
  ~RawReader();

  void start_reader();
  void stop_reader();
  void jump_to_relative_position_per_mill(i32 iPerMill);
  bool handle_continuous_button();

private:
  static constexpr i32 cBufferSize = 32768;

  virtual void run();

  FILE * const mpFile;
  RingBuffer<cf32> * const mpRingBuffer;
  RawFileHandler * const mParent;
  std::atomic<bool> mRunning = false;
  std::atomic<bool> continuous = false;
  std::atomic<i64> mSetNewFilePos = -1;
  i64 mFileLength = 0;

  std::array<f32, 256> mMapTable;
  std::vector<u8> mByteBuffer;
  std::vector<cf32> mCmplxBuffer;

signals:
  void signal_set_progress(i32, f32);
};

#endif

