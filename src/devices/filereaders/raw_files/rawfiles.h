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
#pragma once

#include "dab_constants.h"
#include "device_handler_if.h"
#include "device_notifier_if.h"
#include "ringbuffer.h"
#include "filereader_widget.h"
#include <memory>
#include <QString>
#include <QFrame>
#include <atomic>

class RawReader;

class RawFileHandler final : public IDeviceNotifier, public IDeviceHandler, public FileReaderWidget
{
Q_OBJECT
public:

  explicit RawFileHandler(const QString & iFilename);
  ~RawFileHandler() override;

  i32 getSamples(cf32 *, i32) override;
  i32 Samples() override;
  bool restartReader(i32) override;
  void stopReader() override;
  void show() override;
  void hide() override;
  bool isHidden() override;
  QWidget * get_widget() override { return &mFrame; }
  bool isFileInput() override;
  void setVFOFrequency(i32) override;
  i32 getVFOFrequency() override;
  void resetBuffer() override;
  QString deviceName() override;
  f32 get_file_duration_s() const override { return mFileDuration_s; }
  void jump_to_file_pos_per_mill(i32 iPerMill) override;
  void set_file_paused(bool iPaused) override;

private:
  QFrame mFrame;
  RingBuffer<cf32> mRingBuffer;
  FILE * mpFile = nullptr;
  std::unique_ptr<RawReader> mpRawReader;
  std::atomic<bool> mIsRunning = false;
  f32 mFileDuration_s = 0.0f;
  bool mPaused = false;
};


