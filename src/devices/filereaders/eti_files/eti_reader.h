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

#pragma once

#include "glob_defs.h"
#include "ringbuffer.h"
#include "eti_source_if.h"
#include <QThread>
#include <atomic>
#include <cstdio>

class EtiFileHandler;

// Reads raw ETI(NI) frames (6144 bytes each) from a file, synchronizes on the FSYNC word and puts complete frames
// with a pace of 24 ms per frame into the ring buffer.
class EtiReader : public QThread
{
Q_OBJECT

public:
  EtiReader(EtiFileHandler* ipParent, FILE* ipFile, RingBuffer<u8>* ipRingBuffer);
  ~EtiReader() override;

  void start_reader();
  void stop_reader();
  void jump_to_relative_position_per_mill(i32 iPerMill);
  void set_paused(bool iPaused) { mPaused.store(iPaused); }

private:
  void run() override;
  bool _read_next_frame(i64& oFrameStartPos);

  FILE* const mpFile;
  RingBuffer<u8>* const mpRingBuffer;
  EtiFileHandler* const mpParent;
  std::atomic<bool> mRunning = false;
  std::atomic<i64> mSetNewFilePos = -1;
  std::atomic<bool> mPaused = false;
  i64 mFileLength = 0;

  TEtiFrame mFrame{};
  i32 mFilled = 0;

signals:
  void signal_set_progress(i32, f32);
  void signal_file_looped();
};
