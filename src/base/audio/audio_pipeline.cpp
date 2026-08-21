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
#include "audio_pipeline.h"
#include "audiooutputqt.h"
#include "audioiodevice.h"
#include "openfiledialog.h"
#include "glob_defs.h"
#include <cassert>
#include <QDebug>

AudioPipeline::AudioPipeline(RingBuffer<i16> * const ipAudioBufferFromDecoder,
                             RingBuffer<i16> * const ipAudioBufferToOutput,
                             RingBuffer<u8> * const ipFrameBuffer,
                             QObject * const parent)
  : QObject(parent)
  , mpAudioBufferFromDecoder(ipAudioBufferFromDecoder)
  , mpAudioBufferToOutput(ipAudioBufferToOutput)
  , mpFrameBuffer(ipFrameBuffer)
{
  mpAudioOutput = new AudioOutputQt;
  mpAudioOutput->setParent(this);

  connect(mpAudioOutput, &IAudioOutput::signal_audio_devices_list, this, &AudioPipeline::signal_audio_devices_list);
  connect(mpAudioOutput->get_audio_io_device(), &AudioIODevice::signal_show_audio_peak_level, this, &AudioPipeline::signal_show_audio_peak_level);
  connect(mpAudioOutput->get_audio_io_device(), &AudioIODevice::signal_audio_data_available, this, &AudioPipeline::signal_audio_data_available);
}

AudioPipeline::~AudioPipeline()
{
  _stop_audio_frame_dumping();
  _stop_audio_dumping();
}

void AudioPipeline::_setup_audio_output(const u32 iSampleRate)
{
  mpAudioBufferFromDecoder->flush_ring_buffer();
  mpAudioBufferToOutput->flush_ring_buffer();
  mpCurAudioFifo = &mAudioFifo;
  mpCurAudioFifo->sampleRate = iSampleRate;
  mpCurAudioFifo->pRingbuffer = mpAudioBufferToOutput;

  emit signal_output_sample_rate(iSampleRate / 1000);

  if (mPlaybackState == EPlaybackState::Running)
  {
    mpAudioOutput->slot_restart(mpCurAudioFifo);
  }
  else
  {
    mPlaybackState = EPlaybackState::Running;
    mpAudioOutput->slot_start(mpCurAudioFifo);
  }
}

void AudioPipeline::_check_and_adapt_sample_rate_mode()
{
  constexpr f32 cBufferSizePercentMin  = 20;
  constexpr f32 cBufferSizePercentMax  = 95; // we can afford more buffer to the top as the audio buffer is twice in size
  constexpr f32 cBufferSizePercentUsed = 60;
  constexpr f32 cBufferSizePercentStartSize = cBufferSizePercentMin + 20;
  mAudioQualFillState = mAudioBufferFillFiltered < cBufferSizePercentMin
                          ? -1 // filled too low
                          : (mAudioBufferFillFiltered > cBufferSizePercentMax
                               ? 1 // filled too high
                               : 0);

  switch (mSampleAdaptMode)
  {
  case ESampleAdaptMode::NoChange:
    if      (mAudioQualFillState == -1) mSampleAdaptMode = ESampleAdaptMode::AddSamples;
    else if (mAudioQualFillState == +1) mSampleAdaptMode = ESampleAdaptMode::RemoveSamples;
    break;
  case ESampleAdaptMode::RemoveSamples:
    if (mAudioBufferFillFiltered <= cBufferSizePercentUsed) mSampleAdaptMode = ESampleAdaptMode::NoChange;
    break;
  case ESampleAdaptMode::AddSamples:
    if (mAudioBufferFillFiltered >= cBufferSizePercentUsed) mSampleAdaptMode = ESampleAdaptMode::NoChange;
    break;
  case ESampleAdaptMode::Idle:  // avoid rate adaptions while startup
    if (mAudioBufferFillFiltered >= cBufferSizePercentStartSize) mSampleAdaptMode = ESampleAdaptMode::NoChange;
    break;
  }
}

void AudioPipeline::_push_rate_adaptive_samples_to_audio_buffer(const i32 iAvailableBytes)
{
  if (mSampleAdaptMode == ESampleAdaptMode::NoChange || mSampleAdaptMode == ESampleAdaptMode::Idle)
  {
    mpAudioBufferToOutput->put_data_into_ring_buffer(mAudioTempBuffer.data(), iAvailableBytes);
  }
  else // sample rate adaptions mode
  {
    i32 locSampleCount = 0;
    i32 remainingSamples = 0;

    // are there remaining samples from previous round?
    if (mRemainingSampleCount > 0)
    {
      mpAudioBufferToOutput->put_data_into_ring_buffer(mAudioTempBuffer.data(), mRemainingSampleCount);
      locSampleCount += mRemainingSampleCount;
    }

    if (mSampleAdaptMode == ESampleAdaptMode::AddSamples)
    {
      while (iAvailableBytes - locSampleCount >= cInsertSamplesToPatchSampleRelation)
      {
        mpAudioBufferToOutput->put_data_into_ring_buffer(mAudioTempBuffer.data() + locSampleCount, 2); // pre-fill extra stereo sample
        mpAudioBufferToOutput->put_data_into_ring_buffer(mAudioTempBuffer.data() + locSampleCount, cInsertSamplesToPatchSampleRelation);
        locSampleCount += cInsertSamplesToPatchSampleRelation;
      }

      remainingSamples = iAvailableBytes - locSampleCount;

      if (remainingSamples >= 2)
      {
        mpAudioBufferToOutput->put_data_into_ring_buffer(mAudioTempBuffer.data() + locSampleCount, 2); // pre-fill extra stereo sample
        mpAudioBufferToOutput->put_data_into_ring_buffer(mAudioTempBuffer.data() + locSampleCount, remainingSamples);
      }
    }
    else if (mSampleAdaptMode == ESampleAdaptMode::RemoveSamples)
    {
      while (iAvailableBytes - locSampleCount >= cInsertSamplesToPatchSampleRelation)
      {
        mpAudioBufferToOutput->put_data_into_ring_buffer(mAudioTempBuffer.data() + locSampleCount + 2, cInsertSamplesToPatchSampleRelation - 2); // left out a stereo sample
        locSampleCount += cInsertSamplesToPatchSampleRelation;
      }

      remainingSamples = iAvailableBytes - locSampleCount;

      if (remainingSamples >= 4)
      {
        mpAudioBufferToOutput->put_data_into_ring_buffer(mAudioTempBuffer.data() + locSampleCount + 2, remainingSamples - 2); // left out a stereo sample
      }
    }

    mRemainingSampleCount = cInsertSamplesToPatchSampleRelation - remainingSamples;
  }
}

void AudioPipeline::slot_new_audio(const i32 iNumSamples, const u32 iAudioSampleRate, const u32 iAudioFlags)
{
  if (!mIsChannelRunning)
  {
    return;
  }

  if (mAudioFrameCnt++ > 10)
  {
    mAudioFrameCnt = 0;

    const bool sbrUsed = ((iAudioFlags & 0x1) != 0); // DabRadio::AFL_SBR_USED
    const bool psUsed = ((iAudioFlags & 0x2) != 0); // DabRadio::AFL_PS_USED
    emit signal_sample_rate_and_audio_flags((i32)iAudioSampleRate, sbrUsed, psUsed);
  }

  if (mpCurAudioFifo == nullptr || mpCurAudioFifo->sampleRate != iAudioSampleRate)
  {
    mRemainingSampleCount = 0;
    mAudioBufferFillFiltered = 0.0f;
    mSampleAdaptMode = ESampleAdaptMode::Idle;
    _setup_audio_output(iAudioSampleRate);
  }
  assert(mpCurAudioFifo != nullptr);

  if (mAudioDumpState == EAudioDumpState::WaitForInit)
  {
    if (mWavWriter.init(mAudioWavDumpFileName, iAudioSampleRate, 2))
    {
      mAudioDumpState = EAudioDumpState::Running;
      emit signal_audio_dump_state_changed(true);
    }
    else
    {
      _stop_audio_dumping();
      emit signal_audio_dump_state_changed(false);
      qCritical("AudioPipeline::slot_new_audio: Failed to initialize audio dump state");
    }
  }

  assert((iNumSamples & 1) == 0); // we have always stereo samples, so an even number of samples

  const i32 availableSamples = mpAudioBufferFromDecoder->get_ring_buffer_read_available();

  if (availableSamples >= iNumSamples)
  {
    mAudioTempBuffer.resize(availableSamples);
    const f32 audioBufferFillStatePercent = mpAudioBufferToOutput->get_fill_state_in_percent() * 2; // buffer is double sized as normal used
    mean_filter(mAudioBufferFillFiltered, audioBufferFillStatePercent, (mSampleAdaptMode == ESampleAdaptMode::Idle ? 1.0f : 0.2f));

    _check_and_adapt_sample_rate_mode();

    mpAudioBufferFromDecoder->get_data_from_ring_buffer(mAudioTempBuffer.data(), availableSamples);

    Q_ASSERT(mpCurAudioFifo != nullptr);

    if (availableSamples > mpAudioBufferToOutput->get_ring_buffer_write_available()) // the buffer is double sized as normal used, so this is the final hard top limit
    {
      mpAudioBufferToOutput->flush_ring_buffer();
      qWarning("AudioPipeline::slot_new_audio: Audio output buffer is full, try to start from new");
    }

    _push_rate_adaptive_samples_to_audio_buffer(availableSamples);

    if (mAudioDumpState == EAudioDumpState::Running)
    {
      mWavWriter.write(mAudioTempBuffer.data(), availableSamples);
    }
  }

  i32 corrDir = 0;
  if (mSampleAdaptMode == ESampleAdaptMode::AddSamples) corrDir = +1; // audio buffer will grow
  else if (mSampleAdaptMode == ESampleAdaptMode::RemoveSamples) corrDir = -1; // audio buffer will shrink

  emit signal_audio_buffer_filled_state((i32)mAudioBufferFillFiltered, mAudioQualFillState, corrDir);
}

void AudioPipeline::slot_new_aac_mp2_frame()
{
  if (!mIsChannelRunning)
  {
    return;
  }

  if (mpAudioFrameDumper == nullptr)
  {
    mpFrameBuffer->flush_ring_buffer();
  }
  else
  {
    std::array<u8, 4096> buffer;

    i32 dataAvail = mpFrameBuffer->get_ring_buffer_read_available();

    while (dataAvail > 0)
    {
      const i32 dataSizeRead = std::min(dataAvail, (i32)buffer.size());
      dataAvail -= dataSizeRead;

      mpFrameBuffer->get_data_from_ring_buffer(buffer.data(), dataSizeRead);
      fwrite(buffer.data(), dataSizeRead, 1, mpAudioFrameDumper);
    }
  }
}

void AudioPipeline::_start_audio_dumping(const QString & iFileName)
{
  if (mAudioFrameType == EAudioFrameType::None || mAudioDumpState != EAudioDumpState::Stopped || iFileName.isEmpty())
  {
    return;
  }

  mAudioWavDumpFileName = iFileName;
  mAudioDumpState = EAudioDumpState::WaitForInit;
  emit signal_audio_dump_state_changed(true);
}

void AudioPipeline::_stop_audio_dumping()
{
  if (mAudioDumpState == EAudioDumpState::Stopped)
  {
    return;
  }

  mAudioDumpState = EAudioDumpState::Stopped;
  mWavWriter.close();
  emit signal_audio_dump_state_changed(false);
}

void AudioPipeline::_start_frame_dumping(const QString & iFileName)
{
  if (mAudioFrameType == EAudioFrameType::None || iFileName.isEmpty())
  {
    return;
  }

  if (mpAudioFrameDumper != nullptr)
  {
    fclose(mpAudioFrameDumper);
    mpAudioFrameDumper = nullptr;
  }

  mpAudioFrameDumper = OpenFileDialog::open_file(iFileName, "w+b");
  if (mpAudioFrameDumper == nullptr)
  {
    qWarning() << "AudioPipeline: Cannot open frame dump file" << iFileName;
    emit signal_frame_dump_state_changed(false);
    return;
  }

  emit signal_frame_dump_state_changed(true);
}

void AudioPipeline::_stop_audio_frame_dumping()
{
  if (mpAudioFrameDumper == nullptr)
  {
    return;
  }

  fclose(mpAudioFrameDumper);
  mpAudioFrameDumper = nullptr;
  emit signal_frame_dump_state_changed(false);
}

void AudioPipeline::slot_start_audio_dump(const QString & iFileName)
{
  _start_audio_dumping(iFileName);
}

void AudioPipeline::slot_stop_audio_dump()
{
  _stop_audio_dumping();
}

void AudioPipeline::slot_start_frame_dump(const QString & iFileName)
{
  _start_frame_dumping(iFileName);
}

void AudioPipeline::slot_stop_frame_dump()
{
  _stop_audio_frame_dumping();
}

void AudioPipeline::slot_stop_all_dumping()
{
  _stop_audio_frame_dumping();
  _stop_audio_dumping();
}

void AudioPipeline::slot_stop_audio()
{
  mpAudioOutput->slot_stop();
  mPlaybackState = EPlaybackState::Stopped;
  mpCurAudioFifo = nullptr;
}

void AudioPipeline::slot_set_channel_running(const bool isRunning)
{
  mIsChannelRunning = isRunning;
}

void AudioPipeline::slot_set_scanning(const bool isScanning)
{
  mIsScanning = isScanning;
}

void AudioPipeline::slot_set_service_label(const QString & label)
{
  mServiceLabel = label;
}

void AudioPipeline::slot_set_audio_frame_type(const AudioPipeline::EAudioFrameType type)
{
  mAudioFrameType = type;
}

void AudioPipeline::slot_reset_audio_fifo()
{
  mpCurAudioFifo = nullptr;
}

void AudioPipeline::slot_set_volume(const i32 iSliderValue)
{
  assert(mpAudioOutput != nullptr);
  mpAudioOutput->slot_set_volume(iSliderValue);
}

void AudioPipeline::slot_set_mute(const bool iMuted)
{
  assert(mpAudioOutput != nullptr);
  mpAudioOutput->slot_set_mute(iMuted);
}

void AudioPipeline::slot_set_test_tone(const bool iActive)
{
  assert(mpAudioOutput != nullptr);
  mpAudioOutput->slot_set_test_tone(iActive);
}

void AudioPipeline::slot_set_audio_device(const QByteArray & iDeviceId)
{
  assert(mpAudioOutput != nullptr);
  mpAudioOutput->slot_set_audio_device(iDeviceId);
}

void AudioPipeline::slot_set_peak_level_delay(const i32 iDelaySteps)
{
  assert(mpAudioOutput != nullptr);
  mpAudioOutput->slot_set_peak_level_delay(iDelaySteps);
}
