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

#include "audiooutputqt.h"
#include <QDebug>
#include <QLoggingCategory>
#include <QThread>
#include <QAudioSink>
#include <QAudioDevice>

Q_LOGGING_CATEGORY(sLCAudioOutput, "AudioOutput", QtInfoMsg)

AudioOutputQt::AudioOutputQt(QObject * parent)
  : AudioOutput(parent)
{
  mpIoDevice = new AudioIODevice();
}

AudioOutputQt::~AudioOutputQt()
{
  if (mpAudioSink != nullptr)
  {
    mpAudioSink->stop();
    delete mpAudioSink;
  }
  mpIoDevice->close();
  delete mpIoDevice;
}

void AudioOutputQt::slot_start(SAudioFifo * const iBuffer)
{
  QAudioFormat format;
  format.setSampleRate((int)iBuffer->sampleRate);
  format.setSampleFormat(QAudioFormat::Int16);
  format.setChannelCount(iBuffer->numChannels);
  format.setChannelConfig(iBuffer->numChannels > 1 ? QAudioFormat::ChannelConfigStereo : QAudioFormat::ChannelConfigMono);

  if (!mCurrentAudioDevice.isFormatSupported(format))
  {
    qWarning() << "Default format not supported - trying to use preferred";
    format = mCurrentAudioDevice.preferredFormat();
  }

  if (mpAudioSink != nullptr)
  {
    delete mpAudioSink;
    mpAudioSink = nullptr;
  }

  // create audio sink
  mpAudioSink = new QAudioSink(mCurrentAudioDevice, format, this);

  // set buffer size to 2* AUDIO_FIFO_CHUNK_MS ms
  // this is causing problem on Windows
  //m_audioSink->setBufferSize(2 * AUDIO_FIFO_CHUNK_MS * sRate/1000 * numCh * sizeof(int16_t));

  connect(mpAudioSink, &QAudioSink::stateChanged, this, &AudioOutputQt::_slot_state_changed);

  mpAudioSink->setVolume(mLinearVolume);
  mpCurrentFifo = iBuffer;

  // start IO device
  mpIoDevice->close();
  mpIoDevice->set_buffer(mpCurrentFifo);
  mpIoDevice->start();
  mpAudioSink->start(mpIoDevice);
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
  mCurrentAudioDevice = mpDevices->defaultAudioOutput();
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

void AudioOutputQt::_do_stop()
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
  qDebug() << Q_FUNC_INFO << iNewState;
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
      // if (m_audioSink->error() == QAudio::Error::NoError)
      {
        qCWarning(sLCAudioOutput) << "Audio going to Idle state unexpectedly, trying to restart, error code:" << mpAudioSink->error();
        //qCWarning(audioOutput) << "Audio going to Idle state unexpectedly, trying to restart...";
        _do_restart(mpCurrentFifo);
      }
      // else
      // {   // some error -> doing stop
      //   qCWarning(audioOutput) << "Audio going to Idle state unexpectedly, error code:" << m_audioSink->error();
      //   doStop();
      //   emit signal_audio_output_error();
      // }
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


AudioIODevice::AudioIODevice(QObject * const iParent)
  : QIODevice(iParent)
{
}

void AudioIODevice::set_buffer(SAudioFifo * const iBuffer)
{
  mpInFifo = iBuffer;

  mSampleRateKHz = iBuffer->sampleRate / 1000;
  mNumChannels = iBuffer->numChannels;
  mBytesPerFrame = mNumChannels * sizeof(int16_t);

  // mute ramp is exponential
  // value are precalculated to save MIPS in runtime
  // unmute ramp is then calculated as 2.0 - m_muteFactor in runtime
  // m_muteFactor is calculated for change from 0dB to AUDIOOUTPUT_FADE_MIN_DB in AUDIOOUTPUT_FADE_TIME_MS
  mMuteFactor = powf(10.0f, AUDIOOUTPUT_FADE_MIN_DB / (20.0f * AUDIOOUTPUT_FADE_TIME_MS * (float)mSampleRateKHz));
}

void AudioIODevice::start()
{
  mStopFlag = false;
  mDoStop = false;
  mPlaybackState = EPlaybackState::Muted;
  open(QIODevice::ReadOnly);

  // for testing of error
  // QTimer::singleShot(10000, this, [this](){ mDoStop = true; } );
}

void AudioIODevice::stop()
{
  mStopFlag = true;     // set stop bit
}

void AudioIODevice::_extract_audio_data_from_fifo(char * opData, const int64_t iBytesToRead) const
{
  if (const int64_t bytesToEnd = AUDIO_FIFO_SIZE - mpInFifo->tail;
      bytesToEnd < iBytesToRead)
  {
    memcpy(opData, mpInFifo->buffer + mpInFifo->tail, bytesToEnd);
    memcpy(opData + bytesToEnd, mpInFifo->buffer, (iBytesToRead - bytesToEnd));
    mpInFifo->tail = iBytesToRead - bytesToEnd;
  }
  else
  {
    memcpy(opData, mpInFifo->buffer + mpInFifo->tail, iBytesToRead);
    mpInFifo->tail += iBytesToRead;
  }

  // mpInFifo->mutex.lock();
  mpInFifo->count -= iBytesToRead;
  // mpInFifo->countChanged.wakeAll();
  // mpInFifo->mutex.unlock();
}

void AudioIODevice::_fade(const int64_t iNumSamples, const float coe, float gain, int16_t * dataPtr) const
{
  for (int_fast32_t n = 0; n < iNumSamples; ++n)
  {
    gain *= coe;
    // printf("%.3f ", gain);
    for (uint_fast8_t c = 0; c < mNumChannels; ++c)
    {
      *dataPtr = (int16_t)qRound(gain * (float)(*dataPtr));
      dataPtr++;
    }
  }
  // printf("\n");
}

void AudioIODevice::_fade_in_audio_samples(char * opData, int64_t iNumSamples) const
{
  qCInfo(sLCAudioOutput) << "Unmuting audio";
  const int64_t numFadedSamples = (int64_t)(AUDIOOUTPUT_FADE_TIME_MS * (float)mSampleRateKHz);
  const float coe = 2.0f - (iNumSamples < numFadedSamples ? powf(10.0f, AUDIOOUTPUT_FADE_MIN_DB / (20.0f * (float)iNumSamples)) : mMuteFactor);

  if (iNumSamples >= numFadedSamples )
  {
    iNumSamples = numFadedSamples;
  }

  qCInfo(sLCAudioOutput) << "iNumSamples" << iNumSamples;
  
  float gain = AUDIOOUTPUT_FADE_MIN_LIN;
  int16_t * dataPtr = (int16_t*)opData;

  _fade(iNumSamples, coe, gain, dataPtr);
}

void AudioIODevice::_fade_out_audio_samples(char * opData, const int64_t iBytesToRead, const int64_t iNumSamples) const
{
  qCInfo(sLCAudioOutput, "Muting... [available %u samples]", static_cast<unsigned int>(iNumSamples));

  const int64_t numFadedSamples = (int64_t)(AUDIOOUTPUT_FADE_TIME_MS * (float)mSampleRateKHz);

  if (iNumSamples < numFadedSamples)
  {
    float gain = 1.0;
    const float coe = powf(10, AUDIOOUTPUT_FADE_MIN_DB / (20.0 * iNumSamples));

    int16_t * dataPtr = (int16_t*)opData;

    _fade(iNumSamples, coe, gain, dataPtr);

    // for (int_fast32_t n = 0; n < iNumSamples; ++n)
    // {
    //   gain = gain * coe;  // before by purpose
    //   for (uint_fast8_t c = 0; c < mNumChannels; ++c)
    //   {
    //     *dataPtr = (int16_t)qRound(gain * (float)(*dataPtr));
    //     dataPtr++;
    //   }
    // }
  }
  else
  {
    float gain = 1.0;
    float coe = mMuteFactor;

    int16_t * dataPtr = (int16_t*)opData;

    _fade(numFadedSamples, coe, gain, dataPtr);

    // for (uint_fast32_t n = 0; n < AUDIOOUTPUT_FADE_TIME_MS * mSampleRateKHz; ++n)
    // {
    //   gain = gain * coe;  // before by purpose
    //   for (uint_fast8_t c = 0; c < mNumChannels; ++c)
    //   {
    //     *dataPtr = (int16_t)qRound(gain * (float)(*dataPtr));
    //     dataPtr++;
    //   }
    // }
    memset(dataPtr + numFadedSamples * mNumChannels, 0, iBytesToRead - numFadedSamples * mBytesPerFrame);
  }
}

qint64 AudioIODevice::readData(char * data, qint64 len)
{
  if (mDoStop || (0 == len))
  {
    return 0;
  }

  // read samples from input buffer
  // mpInFifo->mutex.lock();
  const int64_t count = mpInFifo->count;
  // mpInFifo->mutex.unlock();

  bool muteRequest = mMuteFlag || mStopFlag;
  mDoStop = mStopFlag;

  const int64_t bytesToRead = len;
  int64_t numSamples = len / mBytesPerFrame;

  //qDebug() << Q_FUNC_INFO << len << count;

  if (mPlaybackState == EPlaybackState::Muted)
  {
    // muted
    // condition to unmute is enough samples
    //if (count > 500 * mSampleRateKHz * mBytesPerFrame)    // 800ms of signal
    if (count > (int64_t)(AUDIO_FIFO_SIZE / 2))
    {
      // enough samples => reading data from input fifo
      if (muteRequest)
      {
        // staying muted -> setting output buffer to 0
        memset(data, 0, bytesToRead);

        // shifting buffer pointers
        mpInFifo->tail = (mpInFifo->tail + bytesToRead) % AUDIO_FIFO_SIZE;
        // mpInFifo->mutex.lock();
        mpInFifo->count -= bytesToRead;
        // mpInFifo->countChanged.wakeAll();
        // mpInFifo->mutex.unlock();

        // done
        return bytesToRead;
      }

      // at this point we have enough sample to unmute and there is no request => preparing data
      _extract_audio_data_from_fifo(data, bytesToRead);

      // unmute request
      muteRequest = false;
    }
    else
    {   // not enough samples ==> inserting silence
      qCDebug(sLCAudioOutput, "Muted: Inserting silence [%u ms]", static_cast<unsigned int>(bytesToRead / (mBytesPerFrame * mSampleRateKHz)));

      memset(data, 0, bytesToRead);

      // done
      return bytesToRead;
    }
  }
  else
  {
    // (AudioOutputPlaybackState::Muted != m_playbackState)
    // cannot be anything else than Muted or Playing ==> playing
    // condition to mute is not enough samples || muteFlag
    if (count < bytesToRead)
    {
      // not enough samples -> reading what we have and filling rest with zeros
      // minimum mute time is 1ms (m_sampleRate_kHz samples) , if less then hard mute
      if (mSampleRateKHz * mBytesPerFrame > count)
      {
        // nothing to play
        qCInfo(sLCAudioOutput, "Hard mute [no samples available]");
        memset(data, 0, bytesToRead);
        mPlaybackState = EPlaybackState::Muted;
        return bytesToRead;
      }

      // set rest of the samples to be 0
      memset(data + count, 0, bytesToRead - count);
      _extract_audio_data_from_fifo(data, count);

      numSamples = count / mBytesPerFrame;

      // request to apply mute ramp
      muteRequest = true;  // mute
    }
    else
    {
      // enough sample available -> reading samples
      _extract_audio_data_from_fifo(data, bytesToRead);

      if (!muteRequest)
      {
        // done
        return bytesToRead;
      }
    }
  }

  // at this point we have buffer that needs to be muted or unmuted
  // it is indicated by playbackState variable

  // unmute
  if (false == muteRequest)
  {
    // unmute can be requested only when there is enough samples
    _fade_in_audio_samples(data, numSamples);
    mPlaybackState = EPlaybackState::Playing; // playing
  }
  else
  {
    // mute can be requested when there is not enough samples or from HMI
    _fade_out_audio_samples(data, bytesToRead, numSamples);
    mPlaybackState = EPlaybackState::Muted; // muted
  }

  return bytesToRead;
}

qint64 AudioIODevice::writeData(const char * data, qint64 len)
{
  Q_UNUSED(data);
  Q_UNUSED(len);

  return 0;
}

void AudioIODevice::set_mute_state(bool iMuteActive)
{
  mMuteFlag = iMuteActive;
}
