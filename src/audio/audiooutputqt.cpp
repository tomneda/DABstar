/*
 * This file original taken from AbracaDABra and is massively adapted by Thomas Neder
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

#include "audiooutputqt.h"
#include "dabradio_if.h"
#include "audioiodevice.h"
#include <QDebug>
#include <QLoggingCategory>
#include <QThread>
#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>

// Q_LOGGING_CATEGORY(sLogAudioOutput, "AudioOutput", QtDebugMsg)
Q_LOGGING_CATEGORY(sLogAudioOutput, "AudioOutput", QtInfoMsg)

AudioOutputQt::AudioOutputQt(DabRadio * const ipRI, QObject * parent)
{
  mpIoDevice.reset(new AudioIODevice(ipRI));
  mpMediaDevices.reset(new QMediaDevices(this));
  connect(mpMediaDevices.get(), &QMediaDevices::audioOutputsChanged, this, &AudioOutputQt::_slot_update_audio_devices);
}

AudioOutputQt::~AudioOutputQt()
{
  if (mpAudioSink)
  {
    mpAudioSink->stop();
  }

  if (mpIoDevice)
  {
    mpIoDevice->stop();
    mpIoDevice->close();
  }
}

void AudioOutputQt::slot_start(SAudioFifo * const iBuffer)
{
  mAudioFormat.setSampleRate((int)iBuffer->sampleRate);
  mAudioFormat.setSampleFormat(QAudioFormat::Int16);
  mAudioFormat.setChannelCount(2);
  mAudioFormat.setChannelConfig(QAudioFormat::ChannelConfigStereo);

  if (!mCurrentAudioDevice.isFormatSupported(mAudioFormat))
  {
    qWarning(sLogAudioOutput) << "QAudioDevice thinks that the needed audio format is not supported, hope we have luck nevertheless (otherwise use Qt <= 6.8.1)";
    qCInfo(sLogAudioOutput) << "Needed audio format:" << mAudioFormat;
    _print_audio_device_formats(mCurrentAudioDevice);
  }

  mpAudioSink.reset(new QAudioSink(mCurrentAudioDevice, mAudioFormat, this));

  connect(mpAudioSink.get(), &QAudioSink::stateChanged, this, &AudioOutputQt::_slot_state_changed);

  mpAudioSink->setVolume(mLinearVolume);
  mpCurrentFifo = iBuffer;

  // start IO device
  mpIoDevice->close();
  mpIoDevice->set_buffer(mpCurrentFifo);
  mpIoDevice->start();
  mpAudioSink->start(mpIoDevice.get());
}

void AudioOutputQt::_print_audio_device_formats(const QAudioDevice & ad) const
{
  qCInfo(sLogAudioOutput) << "Supported audio format:";
  qCInfo(sLogAudioOutput) << "  Sample formats:" << ad.supportedSampleFormats();
  qCInfo(sLogAudioOutput) << "  Sample rate range:" << ad.minimumSampleRate() << "-" << ad.maximumSampleRate();
  qCInfo(sLogAudioOutput) << "  Channel count range:" << ad.minimumChannelCount() << "-" << ad.maximumChannelCount();
  qCInfo(sLogAudioOutput) << "  Channel configuration:" << ad.channelConfiguration();
}

void AudioOutputQt::slot_restart(SAudioFifo * iBuffer)
{
  if (mpAudioSink != nullptr)
  {
    if (!mpIoDevice->is_muted())
    {
      // delay stop until audio is muted
      mpRestartFifo = iBuffer;
      mpIoDevice->stop();
      return;
    }

    // were are already muted - doRestart now
    _do_restart(iBuffer);
  }
}

void AudioOutputQt::slot_mute(bool iMuteActive)
{
  mpIoDevice->set_mute_state(iMuteActive);
}

void AudioOutputQt::slot_setVolume(const int iLogVolVal)
{
  mLinearVolume = QAudio::convertVolume((float)iLogVolVal / 100.0f, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
  if (mpAudioSink != nullptr)
  {
    mpAudioSink->setVolume(mLinearVolume);
  }
}

void AudioOutputQt::slot_set_audio_device(const QByteArray & iDeviceId)
{
  if (!iDeviceId.isEmpty())
  {
    if (iDeviceId == mCurrentAudioDevice.id())
    {
      return; // current device already set
    }

    const QList<QAudioDevice> list = get_audio_device_list();

    for (const auto & dev : list)
    {
      if (dev.id() == iDeviceId)
      {
        mCurrentAudioDevice = dev;
        emit signal_audio_device_changed(mCurrentAudioDevice.id());
        // change output device to newly selected
        slot_restart(mpCurrentFifo);
        return;
      }
    }
  }

  // device not found => choosing default
  mCurrentAudioDevice = mpMediaDevices->defaultAudioOutput();
  emit signal_audio_device_changed(mCurrentAudioDevice.id());
  // change output device to newly selected
  slot_restart(mpCurrentFifo);
}

void AudioOutputQt::slot_stop()
{
  if (mpAudioSink != nullptr)
  {
    if (!mpIoDevice->is_muted())
    {
      // delay stop until audio is muted
      mpIoDevice->stop();
      return;
    }

    // were are already muted - doStop now
    _do_stop();
  }
}

void AudioOutputQt::_do_stop() const
{
  mpAudioSink->stop();
  mpIoDevice->close();
}

void AudioOutputQt::_do_restart(SAudioFifo * buffer)
{
  mpRestartFifo = nullptr;
  mpAudioSink->stop();
  emit signal_audio_output_restart();
  slot_start(buffer);
}

void AudioOutputQt::_slot_state_changed(const QAudio::State iNewState)
{
  // qDebug() << Q_FUNC_INFO << iNewState;
  switch (iNewState)
  {
  case QAudio::ActiveState:
    // do nothing
    break;
  case QAudio::IdleState:
    // no more data
    if (mpIoDevice->is_muted())
    {
      // this is correct state when stop is requested
      if (mpRestartFifo != nullptr)
      {
        // restart was requested
        _do_restart(mpRestartFifo);
      }
      else
      {
        // stop was requested
        _do_stop();
      }
    }
    else
    {
      if (mpAudioSink->error() == QAudio::Error::NoError)
      {
        qCWarning(sLogAudioOutput) << "Audio going to Idle state unexpectedly, trying to restart...";
        _do_restart(mpCurrentFifo);
      }
      else // some error -> doing stop
      {
        qCWarning(sLogAudioOutput) << "Audio going to Idle state unexpectedly, trying to restart, error code:" << mpAudioSink->error();
        _do_restart(mpCurrentFifo);
        // _do_stop();
        emit signal_audio_output_error();
      }
    }
    break;
  case QAudio::StoppedState:
    // Stopped for other reasons
    break;

  default:
    // ... other cases as appropriate
    break;
  }
}

QList<QAudioDevice> AudioOutputQt::get_audio_device_list() const
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

void AudioOutputQt::_slot_update_audio_devices()
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
