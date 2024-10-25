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

#include "audiooutput.h"
#include <QAudioSink>

AudioOutput::AudioOutput(QObject * parent)
{
  mpDevices = new QMediaDevices(this);
  connect(mpDevices, &QMediaDevices::audioOutputsChanged, this, &AudioOutput::_slot_update_audio_devices);
}

QList<QAudioDevice> AudioOutput::get_audio_device_list() const
{
  QList<QAudioDevice> list;
  const QAudioDevice & defaultDeviceInfo = QMediaDevices::defaultAudioOutput();
  list.append(defaultDeviceInfo);

  for (auto & deviceInfo : QMediaDevices::audioOutputs())
  {
    if (deviceInfo != defaultDeviceInfo)
    {
      list.append(deviceInfo);
    }
  }
  return list;
}

void AudioOutput::_slot_update_audio_devices()
{
  QList<QAudioDevice> list = get_audio_device_list();

  emit signal_audio_devices_list(list);

  bool currentDeviceFound = false;
  for (auto & dev : list)
  {
    if (dev.id() == mCurrentAudioDevice.id())
    {
      currentDeviceFound = true;
      break;
    }
  }

  if (!currentDeviceFound)
  {
    // current device no longer exists => default is used
    mCurrentAudioDevice = QMediaDevices::defaultAudioOutput();
  }
  emit signal_audio_device_changed(mCurrentAudioDevice.id());
}


