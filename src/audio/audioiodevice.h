//
// Created by work on 2025-03-09.
//

#ifndef AUDIOIODEVICE_H
#define AUDIOIODEVICE_H

#include "glob_defs.h"
#include "ringbuffer.h"
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
  bool is_muted() const { return mPlaybackState == EPlaybackState::Muted; }

  qint64 readData(char * opData, qint64 maxlen) override;
  qint64 writeData(const char * data, qint64 len) override;
  qint64 bytesAvailable() const override;
  qint64 size() const override;
  // bool isSequential() const override { return true; }

private:
  static constexpr f32 cFadeTimeMs =  60.0f;
  static constexpr f32 cFadeMinDb  = -40.0f;
#ifdef __clang__
  static constexpr f32 cFadeMinLin =  0.01f; // see issue https://github.com/tomneda/DABstar/issues/99
#else
  static constexpr f32 cFadeMinLin = std::pow(10.0f, cFadeMinDb / 20.0f);
#endif
  static constexpr i32 cNumPeakLevelPerSecond = 20;

  enum class EPlaybackState { Muted = 0, Playing = 1 };

  SAudioFifo * mpInFifo = nullptr;
  EPlaybackState mPlaybackState = EPlaybackState::Muted;
  u32 mSampleRateKHz = 0;
  bool mDoStop = false;
  RingBuffer<i16> * const mpTechDataBuffer;

  std::atomic<bool> mMuteFlag = false;
  std::atomic<bool> mStopFlag = false;

  // peak level meter
  // DelayLine<cf32> delayLine{cf32(-40.0f, -40.0f)};
  u32 mPeakLevelCurSampleCnt = 0;
  u32 mPeakLevelSampleCntBothChannels = 0;
  i16 mAbsPeakLeft = 0;
  i16 mAbsPeakRight = 0;
  QTimer mTimerPeakLevel;
  // union SStereoPeakLevel { struct { f32 Left, Right; }; cf32 Cmplx; };

  // void _extract_audio_data_from_fifo(char * opData, i64 iBytesToRead) const;
  void _fade(i32 iNumStereoSamples, f32 coe, f32 gain, i16 * dataPtr) const;
  void _fade_in_audio_samples(i16 * opData, i32 iNumStereoSamples) const;
  void _fade_out_audio_samples(i16 * opData, i32 iNumStereoSamples) const;
  void _eval_peak_audio_level(const i16 * ipData, i32 iNumSamples);

signals:
  void signal_show_audio_peak_level(f32, f32) const;
  void signal_audio_data_available(i32 iNumSamples, i32 iSampleRate);
};


#endif // AUDIOIODEVICE_H
