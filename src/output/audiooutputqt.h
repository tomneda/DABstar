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

class AudioIODevice;
class QAudioSink;
class RadioInterface;

template <typename T>
class DelayLine
{
public:
  explicit DelayLine(const T & iDefault)
    : mDefault(iDefault)
  {
    set_delay_steps(0); // reserve memory for at least one sample
  }

  void set_delay_steps(const uint32_t iSteps)
  {
    mDataPtrIdx = 0;
    mDelayBuffer.resize(iSteps + 1, mDefault);
  }

  const T & get_set_value(const T & iVal)
  {
    mDelayBuffer[mDataPtrIdx] = iVal;
    mDataPtrIdx = (mDataPtrIdx + 1) % mDelayBuffer.size();
    return mDelayBuffer[mDataPtrIdx];
  }

private:
  uint32_t mDataPtrIdx = 0;
  std::vector<T> mDelayBuffer;
  T mDefault;
};


class AudioOutputQt final : public AudioOutput
{
  Q_OBJECT

public:
  explicit AudioOutputQt(RadioInterface * ipRI, QObject * parent = nullptr);
  ~AudioOutputQt() override;

private:
  AudioIODevice * mpIoDevice = nullptr;
  QAudioSink * mpAudioSink = nullptr;
  SAudioFifo * mpCurrentFifo = nullptr;
  SAudioFifo * mpRestartFifo = nullptr;
  float mLinearVolume = 1.0f;

  void _do_stop() const;
  void _do_restart(SAudioFifo * buffer);

public slots:
  void slot_start(SAudioFifo * iBuffer) override;
  void slot_restart(SAudioFifo * iBuffer) override;
  void slot_stop() override;
  void slot_mute(bool iMuteActive) override;
  void slot_setVolume(int iLogVolVal) override;
  void slot_set_audio_device(const QByteArray & iDeviceId) override;

private slots:
  void _slot_state_changed(QAudio::State iNewState);
};


class AudioIODevice final : public QIODevice
{
  Q_OBJECT
public:
  explicit AudioIODevice(RadioInterface * ipRI, QObject * iParent = nullptr);

  void start();
  void stop();
  void set_buffer(SAudioFifo * iBuffer);
  void set_mute_state(bool iMuteActive);
  bool is_muted() const { return mPlaybackState == EPlaybackState::Muted; }

  qint64 readData(char * opData, qint64 maxlen) override;
  qint64 writeData(const char * data, qint64 len) override;

private:
  SAudioFifo * mpInFifo = nullptr;
  EPlaybackState mPlaybackState = EPlaybackState::Muted;
  //uint8_t mBytesPerFrame = 0;
  uint32_t mSampleRateKHz = 0;
  bool mDoStop = false;

  std::atomic<bool> mMuteFlag = false;
  std::atomic<bool> mStopFlag = false;

  // peak level meter
  // DelayLine<cmplx> delayLine{cmplx(-40.0f, -40.0f)};
  uint32_t mPeakLevelCurSampleCnt = 0;
  uint32_t mPeakLevelSampleCntBothChannels = 0;
  int16_t mAbsPeakLeft = 0;
  int16_t mAbsPeakRight = 0;

  // void _extract_audio_data_from_fifo(char * opData, int64_t iBytesToRead) const;
  void _fade(int32_t iNumStereoSamples, float coe, float gain, int16_t * dataPtr) const;
  void _fade_in_audio_samples(int16_t * opData, int32_t iNumStereoSamples) const;
  void _fade_out_audio_samples(int16_t * opData, int32_t iNumStereoSamples) const;
  void _eval_peak_audio_level(const int16_t * ipData, uint32_t iNumSamples);

signals:
  void signal_show_audio_peak_level(float, float) const;
};

#endif // AUDIOOUTPUTQT_H
