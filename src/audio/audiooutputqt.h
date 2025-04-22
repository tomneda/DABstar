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

#ifndef AUDIOOUTPUTQT_H
#define AUDIOOUTPUTQT_H

#include "audiooutput.h"
#include "audiofifo.h"
#include "glob_defs.h"
#include <QObject>
#include <QIODevice>
#include <QTimer>

class AudioIODevice;
class QAudioSink;
class QMediaDevices;
class IDabRadio;


class AudioOutputQt final : public IAudioOutput
{
  Q_OBJECT

public:
  explicit AudioOutputQt(IDabRadio * ipRI, QObject * parent = nullptr);
  ~AudioOutputQt() override;

  QList<QAudioDevice> get_audio_device_list() const override;

private:
  QScopedPointer<QMediaDevices>	mpMediaDevices;
  QScopedPointer<AudioIODevice> mpIoDevice;
  QScopedPointer<QAudioSink> mpAudioSink;
  QList<QAudioDevice> mOutputDevices;
  QAudioDevice mCurrentAudioDevice;

  SAudioFifo * mpCurrentFifo = nullptr;
  SAudioFifo * mpRestartFifo = nullptr;
  float mLinearVolume = 1.0f;
  QAudioFormat mAudioFormat;

  void _do_stop() const;
  void _do_restart(SAudioFifo * buffer);
  void _print_audio_device_formats(const QAudioDevice & ad) const;

public slots:
  void slot_start(SAudioFifo * iBuffer) override;
  void slot_restart(SAudioFifo * iBuffer) override;
  void slot_stop() override;
  void slot_mute(bool iMuteActive) override;
  void slot_setVolume(int iLogVolVal) override;
  void slot_set_audio_device(const QByteArray & iDeviceId) override;

private slots:
  void _slot_state_changed(QAudio::State iNewState);
  void _slot_update_audio_devices();
};

#endif // AUDIOOUTPUTQT_H
