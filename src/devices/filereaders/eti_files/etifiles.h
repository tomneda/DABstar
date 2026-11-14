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
#include "device_handler_if.h"
#include "device_notifier_if.h"
#include "eti_source_if.h"
#include "ringbuffer.h"
#include "filereader_widget.h"
#include <memory>
#include <atomic>
#include <QString>
#include <QFrame>

class EtiReader;

// File reader for raw ETI(NI) files (ETSI EN 300 799, 6144 bytes per frame) like written by DABstar, dablin or eti-cmdline.
// It delivers no IQ samples but whole ETI frames via IEtiSource.
class EtiFileHandler final : public IDeviceNotifier, public IDeviceHandler, public IEtiSource, public FileReaderWidget
{
Q_OBJECT
public:
  explicit EtiFileHandler(const QString & iFilename);
  ~EtiFileHandler() override;

  // IDeviceHandler
  i32 getSamples(cf32 *, i32) override { return 0; } // no samples available
  i32 Samples() override { return 0; }
  bool restartReader(i32) override;
  void stopReader() override;
  void show() override;
  void hide() override;
  bool isHidden() override;
  QWidget * get_widget() override { return &mFrame; }
  bool isFileInput() override { return true; }
  void setVFOFrequency(i32) override {}
  i32 getVFOFrequency() override { return 0; }
  void resetBuffer() override;
  QString deviceName() override { return "EtiFile"; }
  f32 get_file_duration_s() const override { return mFileDuration_s; }
  void jump_to_file_pos_per_mill(i32 iPerMill) override;
  void set_file_paused(bool iPaused) override;
  IEtiSource * get_eti_source() override { return this; }

  // IEtiSource
  bool get_eti_frame(TEtiFrame & oFrame, i32 iTimeout_ms) override;

private:
  QFrame mFrame;
  RingBuffer<u8> mRingBuffer;
  FILE * mpFile = nullptr;
  std::unique_ptr<EtiReader> mpEtiReader;
  std::atomic<bool> mIsRunning = false;
  f32 mFileDuration_s = 0.0f;
  bool mPaused = false;
};
