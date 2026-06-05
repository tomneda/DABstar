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
#include "test_tone.h"
#include "ringbuffer.h"
#include "delay_line.h"
#include <QTimer>
#include <QIODevice>

class DabRadio;
struct SAudioFifo;

class AudioIODevice final : public QIODevice
{
  Q_OBJECT

public:
  explicit AudioIODevice(QObject * iParent = nullptr);

  void start();
  void stop();
  void set_buffer(SAudioFifo * iBuffer);
  void set_mute_state(bool iMuteActive);
  void set_test_tone(bool iActive);
  void set_peak_level_delay(i32 iDelaySteps);
  bool is_muted() const { return mPlaybackState == EPlaybackState::Muted; }

  qint64 readData(char * opData, qint64 maxlen) override;
  qint64 writeData(const char * data, qint64 len) override;
  qint64 bytesAvailable() const override;
  qint64 size() const override;

private:
  static constexpr f32 cFadeTimeMs =  60.0f;
  static constexpr f32 cFadeMinDb  = -40.0f;
  const            f32 cFadeMinLin = std::pow(10.0f, cFadeMinDb / 20.0f); // do not use constexpr as it is not supported by all compilers for std::pow()
  static constexpr i32 cNumPeakLevelPerSecond = 20;

  enum class EPlaybackState { Muted = 0, Playing = 1 };

  SAudioFifo * mpInFifo = nullptr;
  EPlaybackState mPlaybackState = EPlaybackState::Muted;
  u32 mSampleRateKHz = 0;
  bool mDoStop = false;
  RingBuffer<i16> * const mpTechDataBuffer;
  RingBuffer<f32> * const mpLevelMeterBuffer;

  std::atomic<bool> mMuteFlag = false;
  std::atomic<bool> mStopFlag = false;

  // peak level meter
  static constexpr f32 cDecayFactor = 0.89125094; // == std::pow(10.0f, -1.0f / 20.0f);
  union SStereoPeakLevel { std::array<f32, 4> buffer;  struct { f32 peakLeft = -40.0f, peakRight = -40.0f; f32 rmsLeft = -40.0f, rmsRight = -40.0f; }; };
  DelayLine<SStereoPeakLevel> mDelayLine{SStereoPeakLevel()};
  u32 mPeakLevelCurSampleCnt = 0;
  u32 mPeakLevelSampleCntBothChannels = 0;
  i16 mAbsPeakLeft = 0;
  i16 mAbsPeakRight = 0;
  f32 mMeanSqLeft = 0;
  f32 mMeanSqRight = 0;
  f32 mRmsAlpha = 1;
  SStereoPeakLevel mLastSpl{};  // last received level, re-used on timer underflow (Windows)

  QTimer * mpTimerPeakLevel = nullptr;

  TestTone mTestTone{};

  void _fade(i32 iNumStereoSamples, f32 coe, f32 gain, i16 * dataPtr) const;
  void _fade_in_audio_samples(i16 * opData, i32 iNumStereoSamples) const;
  void _fade_out_audio_samples(i16 * opData, i32 iNumStereoSamples) const;
  void _eval_peak_audio_level(const i16 * ipData, i32 iNumSamples);

private slots:
  void _slot_timer_level_meter();

signals:
  void signal_show_audio_peak_level(f32 iPeakLeftDb, f32 iPeakRightDb, f32 iRmsLeftDb, f32 iRmsRightDb) const;
  void signal_audio_data_available(i32 iNumSamples, i32 iSampleRate);
};
