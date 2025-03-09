/*
 * This file original taken from AbracaDABra and is adapted by Thomas Neder
 * (https://github.com/tomneda)
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QObject>
// #include <QMediaDevices>
#include <QAudioDevice>

#include "audiofifo.h"

class RadioInterface;

class IAudioOutput : public QObject
{
  Q_OBJECT

public:
  ~IAudioOutput() override = default;

  virtual QList<QAudioDevice> get_audio_device_list() const = 0;

public slots:
  virtual void slot_start(SAudioFifo * iBuffer) = 0;
  virtual void slot_restart(SAudioFifo * iBuffer) = 0;
  virtual void slot_stop() = 0;
  virtual void slot_mute(bool on) = 0;
  virtual void slot_setVolume(int value) = 0;
  virtual void slot_set_audio_device(const QByteArray & deviceId) = 0;

signals:
  void signal_audio_output_error();
  void signal_audio_output_restart();
  void signal_audio_devices_list(QList<QAudioDevice> deviceList);
  void signal_audio_device_changed(const QByteArray & id);
};

#endif // AUDIOOUTPUT_H
