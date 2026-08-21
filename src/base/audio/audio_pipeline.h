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
#include "wav_writer.h"
#include "audiofifo.h"
#include <QObject>
#include <QAudioDevice>
#include <QString>
#include <vector>

class IAudioOutput;

class AudioPipeline : public QObject
{
  Q_OBJECT

public:
  enum class EAudioFrameType { None, MP2, AAC };

  AudioPipeline(RingBuffer<i16> * ipAudioBufferFromDecoder,
                RingBuffer<i16> * ipAudioBufferToOutput,
                RingBuffer<u8> * ipFrameBuffer,
                QObject * parent = nullptr);
  ~AudioPipeline() override;

  const IAudioOutput * get_audio_output() const { return mpAudioOutput; }

  // Buffer underflow/overflow handling
  static constexpr i32 cInsertSamplesToPatchSampleRelation = 100 * 2 /*stereo*/;

public slots:
  // Slot called directly from decoders (FaadDecoder, FdkAAC, Mp2Processor)
  void slot_new_audio(i32 iNumSamples, u32 iAudioSampleRate, u32 iAudioFlags);

  // Slot called directly from frame processors (Mp4Processor, Mp2Processor)
  void slot_new_aac_mp2_frame();

  // Control slots (invoked from AudioManager on GUI thread)
  void slot_start_audio_dump(const QString & iFileName);
  void slot_stop_audio_dump();
  void slot_start_frame_dump(const QString & iFileName);
  void slot_stop_frame_dump();
  void slot_stop_all_dumping();
  void slot_stop_audio();
  void slot_set_channel_running(bool isRunning);
  void slot_set_scanning(bool isScanning);
  void slot_set_service_label(const QString & label);
  void slot_set_audio_frame_type(AudioPipeline::EAudioFrameType type);
  void slot_reset_audio_fifo();

  // Forwarded audio output control slots
  void slot_set_volume(i32 iSliderValue);
  void slot_set_mute(bool iMuted);
  void slot_set_test_tone(bool iActive);
  void slot_set_audio_device(const QByteArray & iDeviceId);
  void slot_set_peak_level_delay(i32 iDelaySteps);

signals:
  void signal_audio_devices_list(const QList<QAudioDevice> & iDeviceList);
  void signal_show_audio_peak_level(f32 iPeakLeftDb, f32 iPeakRightDb, f32 iRmsLeftDb, f32 iRmsRightDb);
  void signal_audio_data_available(i32 numSamples, i32 sampleRate);
  void signal_audio_buffer_filled_state(i32 percent, i32 qualFillState, i32 corrDir);
  void signal_output_sample_rate(u32 kSps);
  void signal_sample_rate_and_audio_flags(i32 sampleRate, bool sbrUsed, bool psUsed);
  void signal_audio_dump_state_changed(bool isDumping);
  void signal_frame_dump_state_changed(bool isDumping);

private:
  void _setup_audio_output(u32 iSampleRate);
  void _check_and_adapt_sample_rate_mode();
  void _push_rate_adaptive_samples_to_audio_buffer(i32 iAvailableBytes);
  void _start_audio_dumping(const QString & iFileName);
  void _stop_audio_dumping();
  void _start_frame_dumping(const QString & iFileName);
  void _stop_audio_frame_dumping();

  RingBuffer<i16> * const mpAudioBufferFromDecoder;
  RingBuffer<i16> * const mpAudioBufferToOutput;
  RingBuffer<u8> * const mpFrameBuffer;

  IAudioOutput * mpAudioOutput = nullptr;
  WavWriter mWavWriter;
  SAudioFifo mAudioFifo{};
  SAudioFifo * mpCurAudioFifo = nullptr;
  FILE * mpAudioFrameDumper = nullptr;

  bool mIsChannelRunning = false;
  bool mIsScanning = false;
  QString mServiceLabel;

  enum class EPlaybackState { Stopped, WaitForInit, Running };
  enum class EAudioDumpState { Stopped, WaitForInit, Running };
  EPlaybackState mPlaybackState = EPlaybackState::Stopped;
  EAudioDumpState mAudioDumpState = EAudioDumpState::Stopped;
  EAudioFrameType mAudioFrameType = EAudioFrameType::None;

  enum class ESampleAdaptMode { Idle, NoChange, RemoveSamples, AddSamples };
  ESampleAdaptMode mSampleAdaptMode = ESampleAdaptMode::Idle;
  i32 mRemainingSampleCount = 0;

  std::vector<i16> mAudioTempBuffer;
  QString mAudioWavDumpFileName;

  f32 mAudioBufferFillFiltered = 0.0f;
  i32 mAudioQualFillState = 0;
  i32 mAudioFrameCnt = 0;
};
