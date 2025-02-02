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
  RawReader(RawFileHandler *, FILE *, RingBuffer<cmplx> *);
  ~RawReader();

  void stopReader();

private:
  static constexpr int32_t BUFFERSIZE = 32768;

  virtual void run();

  RawFileHandler * const mParent;
  FILE * const mpFile;
  RingBuffer<cmplx> * const mpRingBuffer;
  std::atomic<bool> mRunning;
  std::vector<uint8_t> mByteBuffer;
  std::vector<cmplx> mCmplxBuffer;
  int64_t mFileLength;
  std::array<float, 256> mMapTable;

signals:
  void signal_set_progress(int, float);
};

#endif

