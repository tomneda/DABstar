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
#ifndef  WAV_FILES_H
#define  WAV_FILES_H

#include  <QObject>
#include  <QString>
#include  <QFrame>
#include  <sndfile.h>
#include  <atomic>
#include  "dab-constants.h"
#include  "device-handler.h"
#include  "ringbuffer.h"

#include  "filereader-widget.h"
#include  "wav-reader.h"

class WavFileHandler final : public QObject, public IDeviceHandler, public FileReaderWidget
{
Q_OBJECT
public:
  explicit WavFileHandler(const QString & iFilename);
  ~WavFileHandler() override;

  int32_t getSamples(cmplx *, int32_t) override;
  int32_t Samples() override;
  bool restartReader(int32_t) override;
  void stopReader() override;
  void setVFOFrequency(int32_t) override;
  int getVFOFrequency() override;
  void show() override;
  void hide() override;
  bool isHidden() override;
  bool isFileInput() override;
  void resetBuffer() override;
  int16_t bitDepth() override;
  QString deviceName() override;

private:
  QFrame myFrame;
  QString fileName;
  RingBuffer<cmplx> _I_Buffer;
  int32_t bufferSize = 0;
  SNDFILE * filePointer = nullptr;
  WavReader * readerTask = nullptr;
  std::atomic<bool> running = false;

public slots:
  void setProgress(int, float);
  void slot_handle_cb_loop_file(const bool iChecked);
};

#endif

