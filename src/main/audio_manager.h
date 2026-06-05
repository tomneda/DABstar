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

#include "dab-constants.h"
#include "ringbuffer.h"
#include "wav_writer.h"
#include "audiofifo.h"
#include "openfiledialog.h"
#include <QObject>
#include <QTimer>
#include <QAudioDevice>
#include <vector>

class IAudioOutput;
class TechData;
class Configuration;
class QThread;
class QPushButton;
class QProgressBar;
class QSlider;
class LevelMeter;

class AudioManager : public QObject
{
  Q_OBJECT
public:
  enum class EPlaybackState { Stopped, WaitForInit, Running };
  enum class EAudioDumpState { Stopped, WaitForInit, Running };
  enum class EAudioFrameType { None, MP2, AAC };

  struct SResourceConfig
  {
    RingBuffer<i16> * pAudioBufferFromDecoder;
    RingBuffer<i16> * pAudioBufferToOutput;
    RingBuffer<u8> * pFrameBuffer;
    Configuration * pConfig;
    TechData * pTechDataWidget;
    QProgressBar * pProgBarAudioBuffer;
    LevelMeter * pLevelMeterLeft;
    LevelMeter * pLevelMeterRight;
    QSlider * pSliderVolume;
    OpenFileDialog * pOpenFileDialog;
  };

  explicit AudioManager(const SResourceConfig & cfg, QObject * parent = nullptr);
  ~AudioManager() override;

  // State update methods called by DabRadio
  void set_channel_running(bool isRunning) { mIsChannelRunning = isRunning; }
  void set_scanning(bool isScanning) { mIsScanning = isScanning; }
  void set_service_label(const QString & label) { mServiceLabel = label; }
  void set_audio_frame_type(EAudioFrameType type) { mAudioFrameType = type; }

  RingBuffer<i16> * get_audio_buffer_from_decoder() const { return mpAudioBufferFromDecoder; }
  void reset_audio_fifo() { mpCurAudioFifo = nullptr; }

  void stop_audio_output();   // called when channel/app stops
  void stop_all_dumping();    // stop WAV and frame dump (called when channel stops)
  void update_dump_timers() const;  // called every second from DabRadio's display timer

  // Called from DabRadio wrappers (not slots — invoked directly)
  void new_audio(i32 iAmount, u32 iAudioSampleRate, u32 iAudioFlags);
  void new_aac_mp2_frame() const;

public slots:
  // Connected from AudioIODevice (internally)
  void slot_show_audio_peak_level(f32 iPeakLeftDb, f32 iPeakRightDb, f32 iRmsLeftDb, f32 iRmsRightDb);
  // Connected from TechData buttons
  void slot_handle_audio_dump_button();
  void slot_handle_frame_dump_button();
  // Connected from UI elements
  void slot_handle_volume_slider(i32 iSliderValue);
  void slot_set_mute(bool iMuted);
  void slot_set_test_tone(bool iActive);
  void slot_set_audio_device(const QByteArray & iDeviceId);
  // Connected from IAudioOutput
  void slot_load_audio_device_list(const QList<QAudioDevice> & iDeviceList) const;
  // Connected from config spinbox
  void slot_update_peak_level_delay(i32 iDelaySteps = -1);

private:
  void _update_level_meter(LevelMeter * ipMeter, const f32 iPeak, const f32 iRms) const;

signals:
  // Forwarded to IAudioOutput (queued - IAudioOutput is in a different thread)
  void signal_start_audio(SAudioFifo * buffer);
  void signal_switch_audio(SAudioFifo * buffer);
  void signal_stop_audio();
  void signal_audio_mute(bool iMuted);
  void signal_set_audio_device(const QByteArray & deviceId);
  void signal_audio_test_tone(bool active);
  void signal_audio_peak_level_delay(i32 delaySteps);
  void signal_audio_buffer_filled_state(i32 percent);

  // Status info updates to DabRadio (DabRadio updates its status labels)
  void signal_sbr_used(bool used);
  void signal_ps_used(bool used);
  void signal_output_sample_rate(u32 kSps);

  // Mute state: emitted when volume slider unmutes
  void signal_unmute_requested();

private:
  // Non-owned resources (passed in constructor)
  RingBuffer<i16> * const mpAudioBufferFromDecoder;
  RingBuffer<i16> * const mpAudioBufferToOutput;
  RingBuffer<u8> * const mpFrameBuffer;
  Configuration * const mpConfig;
  TechData * const mpTechDataWidget;
  QProgressBar * const mpProgBarAudioBuffer;
  LevelMeter * const mpLevelMeterLeft;
  LevelMeter * const mpLevelMeterRight;
  QSlider * const mpSliderVolume;
  OpenFileDialog * const mpOpenFileDialog;

  // Owned objects
  IAudioOutput * mpAudioOutput = nullptr;
  QThread * mAudioOutputThread = nullptr;
  WavWriter mWavWriter;
  SAudioFifo mAudioFifo{};
  SAudioFifo * mpCurAudioFifo = nullptr;
  FILE * mpAudioFrameDumper = nullptr;

  // State received from DabRadio
  bool mIsChannelRunning = false;
  bool mIsScanning = false;
  bool mMutingActive = false;
  QString mServiceLabel;

  // Internal audio state
  EPlaybackState mPlaybackState = EPlaybackState::Stopped;
  EAudioDumpState mAudioDumpState = EAudioDumpState::Stopped;
  EAudioFrameType mAudioFrameType = EAudioFrameType::None;

  std::vector<i16> mAudioTempBuffer;
  QString mAudioWavDumpFileName;

  f32 mAudioBufferFillFiltered = 0.0f;
  i32 mAudioFrameCnt = 0;
  bool mProgBarAudioBufferFullColorSet = false;
  mutable uint32_t mAudioDumpTimer = 0;
  mutable uint32_t mFrameDumpTimer = 0;

  void _setup_audio_output(u32 iSampleRate);
  void _start_audio_dumping();
  void _stop_audio_dumping();
  void _start_audio_frame_dumping();
  void _stop_audio_frame_dumping();
  void _emphasize_pushbutton(QPushButton * ipPB, bool iEmphasize) const;
  QString _seconds_to_timestring(const u32 iTimer) const;
};
