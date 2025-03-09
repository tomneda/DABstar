//
// Created by work on 2025-03-09.
//

#ifndef AUDIO_IO_DEVICE_H
#define AUDIO_IO_DEVICE_H

#include "glob_defs.h"
#include "ringbuffer.h"
#include <QTimer>
#include <QIODevice>

class RadioInterface;
struct SAudioFifo;

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
  qint64 bytesAvailable() const override;
  qint64 size() const override;
  // bool isSequential() const override { return true; }

private:
  static constexpr float AUDIOOUTPUT_FADE_TIME_MS =  60.0f;
  static constexpr float AUDIOOUTPUT_FADE_MIN_DB  = -40.0f;
  static constexpr float AUDIOOUTPUT_FADE_MIN_LIN =  std::pow(10.0f, AUDIOOUTPUT_FADE_MIN_DB / 20.0f);

  enum class EPlaybackState { Muted = 0, Playing = 1 };

  static constexpr int32_t cNumPeakLevelPerSecond = 20;
  SAudioFifo * mpInFifo = nullptr;
  EPlaybackState mPlaybackState = EPlaybackState::Muted;
  //uint8_t mBytesPerFrame = 0;
  uint32_t mSampleRateKHz = 0;
  bool mDoStop = false;
  RingBuffer<int16_t> * const mpTechDataBuffer;
  RingBuffer<cmplx16> * const mpStereoPeakLevelRingBuffer;

  std::atomic<bool> mMuteFlag = false;
  std::atomic<bool> mStopFlag = false;

  // peak level meter
  // DelayLine<cmplx> delayLine{cmplx(-40.0f, -40.0f)};
  uint32_t mPeakLevelCurSampleCnt = 0;
  uint32_t mPeakLevelSampleCntBothChannels = 0;
  int16_t mAbsPeakLeft = 0;
  int16_t mAbsPeakRight = 0;
  QTimer mTimerPeakLevel;
  // union SStereoPeakLevel { struct { float Left, Right; }; cmplx Cmplx; };

  // void _extract_audio_data_from_fifo(char * opData, int64_t iBytesToRead) const;
  void _fade(int32_t iNumStereoSamples, float coe, float gain, int16_t * dataPtr) const;
  void _fade_in_audio_samples(int16_t * opData, int32_t iNumStereoSamples) const;
  void _fade_out_audio_samples(int16_t * opData, int32_t iNumStereoSamples) const;
  void _eval_peak_audio_level(const int16_t * ipData, uint32_t iNumSamples);

private slots:
  void _slot_peak_level_timeout();

  signals:
    void signal_show_audio_peak_level(float, float) const;
  void signal_audio_data_available(int iNumSamples, int iSampleRate);
};


#endif //AUDIO_IO_DEVICE_H
