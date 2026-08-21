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

#include "dab_constants.h"
#include "ringbuffer.h"
#include "audio_pipeline.h"
#include "openfiledialog.h"
#include <QObject>
#include <QAudioDevice>
#include <vector>

class LevelMeter;
class TechData;
class Configuration;
class QThread;
class QSlider;

class AudioManager : public QObject
{
  Q_OBJECT
public:
  using EAudioFrameType = AudioPipeline::EAudioFrameType;

  struct SResourceConfig
  {
    RingBuffer<i16> * pAudioBufferFromDecoder;
    RingBuffer<i16> * pAudioBufferToOutput;
    RingBuffer<u8> * pFrameBuffer;
    Configuration * pConfig;
    TechData * pTechDataWidget;
    LevelMeter * pLevelMeterLeft;
    LevelMeter * pLevelMeterRight;
    QSlider * pSliderVolume;
    OpenFileDialog * pOpenFileDialog;
  };

  explicit AudioManager(const SResourceConfig & cfg, QObject * parent = nullptr);
  ~AudioManager() override;

  AudioPipeline * get_audio_pipeline() const { return mpAudioPipeline; }
  RingBuffer<i16> * get_audio_buffer_from_decoder() const { return mpAudioBufferFromDecoder; }

  // State update methods called by DabRadio
  void set_channel_running(bool isRunning);
  void set_scanning(bool isScanning);
  void set_service_label(const QString & label);
  void set_audio_frame_type(EAudioFrameType type);
  void reset_audio_fifo();

  void stop_audio_output();   // called when channel/app stops
  void stop_all_dumping();    // stop WAV and frame dump (called when channel stops)
  void update_dump_timers() const;  // called every second from DabRadio's display timer

private:
  // Non-owned resources (passed in constructor)
  RingBuffer<i16> * const mpAudioBufferFromDecoder;
  RingBuffer<i16> * const mpAudioBufferToOutput;
  RingBuffer<u8> * const mpFrameBuffer;
  Configuration * const mpConfig;
  TechData * const mpTechDataWidget;
  LevelMeter * const mpLevelMeterLeft;
  LevelMeter * const mpLevelMeterRight;
  QSlider * const mpSliderVolume;
  OpenFileDialog * const mpOpenFileDialog;

  // Owned worker and thread
  AudioPipeline * mpAudioPipeline = nullptr;
  QThread * mAudioOutputThread = nullptr;

  // State
  bool mIsChannelRunning = false;
  bool mIsScanning = false;
  bool mMutingActive = false;
  bool mAudioDumpRunning = false;
  bool mFrameDumpRunning = false;
  QString mServiceLabel;
  EAudioFrameType mAudioFrameType = EAudioFrameType::None;

  mutable uint32_t mAudioDumpTimer = 0;
  mutable uint32_t mFrameDumpTimer = 0;

  void _update_level_meter(LevelMeter * ipMeter, const f32 iPeak, const f32 iRms) const;
  QString _seconds_to_timestring(const u32 iTimer) const;

public slots:
  // Connected from AudioPipeline
  void slot_show_audio_peak_level(f32 iPeakLeftDb, f32 iPeakRightDb, f32 iRmsLeftDb, f32 iRmsRightDb);
  void slot_show_sample_rate_and_audio_flags(i32 iSampleRate, bool iSbrUsed, bool iPsUsed);
  void slot_audio_dump_state_changed(bool iIsDumping);
  void slot_frame_dump_state_changed(bool iIsDumping);
  void slot_load_audio_device_list(const QList<QAudioDevice> & iDeviceList) const;

  // Connected from TechData buttons
  void slot_handle_audio_dump_button();
  void slot_handle_frame_dump_button();

  // Connected from UI elements
  void slot_handle_volume_slider(i32 iSliderValue);
  void slot_set_mute(bool iMuted);
  void slot_set_test_tone(bool iActive);
  void slot_set_audio_device(const QByteArray & iDeviceId);
  void slot_update_peak_level_delay(i32 iDelaySteps = -1);

signals:
  void signal_audio_buffer_filled_state(i32 percent, i32 qualFillState, i32 corrDir);

  // Status info updates to DabRadio (DabRadio updates its status labels)
  void signal_sbr_and_ps_used(bool sbrUsed, bool psUsed);
  void signal_output_sample_rate(u32 kSps);

  // Mute state: emitted when volume slider unmutes
  void signal_unmute_requested();
};
