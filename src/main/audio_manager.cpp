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
#include "audio_manager.h"
#include "audiooutputqt.h"
#include "audioiodevice.h"
#include "configuration.h"
#include "techdata.h"
#include "setting-helper.h"
#include "glob_defs.h"
#include <QProgressBar>
#include <QSlider>
#include <QThread>
#include <QPushButton>
#include <cassert>
#include <cstdio>
#include <qwt_thermo.h>

static void seconds_to_timestring(char * text, const uint32_t timer)
{
  const int sec  = timer % 60;
  const int min  = (timer / 60) % 60;
  const int hour = timer / 3600;
  sprintf(text, "%d:%02d:%02d", hour, min, sec);
}

AudioManager::AudioManager(const SResourceConfig & cfg, QObject * parent)
  : QObject(parent)
  , mpAudioBufferFromDecoder(cfg.pAudioBufferFromDecoder)
  , mpAudioBufferToOutput(cfg.pAudioBufferToOutput)
  , mpFrameBuffer(cfg.pFrameBuffer)
  , mpConfig(cfg.pConfig)
  , mpTechDataWidget(cfg.pTechDataWidget)
  , mpProgBarAudioBuffer(cfg.pProgBarAudioBuffer)
  , mpThermoPeakLevelLeft(cfg.pThermoPeakLevelLeft)
  , mpThermoPeakLevelRight(cfg.pThermoPeakLevelRight)
  , mpSliderVolume(cfg.pSliderVolume)
  , mpOpenFileDialog(cfg.pOpenFileDialog)
{
  mpAudioOutput = new AudioOutputQt;

  mAudioOutputThread = new QThread(this);
  mAudioOutputThread->setObjectName("audioOutThr");
  mpAudioOutput->moveToThread(mAudioOutputThread);

  // do the connections after the thread is moved
  connect(mpAudioOutput, &IAudioOutput::signal_audio_devices_list, this, &AudioManager::slot_load_audio_device_list);
  connect(this, &AudioManager::signal_start_audio, mpAudioOutput, &IAudioOutput::slot_start, Qt::QueuedConnection);
  connect(this, &AudioManager::signal_switch_audio, mpAudioOutput, &IAudioOutput::slot_restart, Qt::QueuedConnection);
  connect(this, &AudioManager::signal_stop_audio, mpAudioOutput, &IAudioOutput::slot_stop, Qt::QueuedConnection);
  connect(this, &AudioManager::signal_set_audio_device, mpAudioOutput, &IAudioOutput::slot_set_audio_device, Qt::QueuedConnection);
  connect(this, &AudioManager::signal_audio_mute, mpAudioOutput, &IAudioOutput::slot_set_mute, Qt::QueuedConnection);
  connect(this, &AudioManager::signal_audio_test_tone, mpAudioOutput, &IAudioOutput::slot_set_test_tone);
  connect(this, &AudioManager::signal_audio_peak_level_delay, mpAudioOutput, &IAudioOutput::slot_set_peak_level_delay);
  connect(this, &AudioManager::signal_audio_buffer_filled_state, mpProgBarAudioBuffer, &QProgressBar::setValue);
  connect(mpSliderVolume, &QSlider::valueChanged, this, &AudioManager::slot_handle_volume_slider);
  connect(mpAudioOutput->get_audio_io_device(), &AudioIODevice::signal_show_audio_peak_level, this, &AudioManager::slot_show_audio_peak_level, Qt::QueuedConnection);
  connect(mpAudioOutput->get_audio_io_device(), &AudioIODevice::signal_audio_data_available, mpTechDataWidget, &TechData::slot_audio_data_available, Qt::QueuedConnection);
  connect(mpConfig->sbPeakLevelDelay, &QSpinBox::valueChanged, this, &AudioManager::slot_update_peak_level_delay);

  Settings::Main::sliderVolume.register_widget_and_update_ui_from_setting(mpSliderVolume, 100);

  connect(mAudioOutputThread, &QThread::finished, mpAudioOutput, &QObject::deleteLater);

  mAudioOutputThread->start();

  slot_update_peak_level_delay(); // write startup value

  slot_load_audio_device_list(mpAudioOutput->get_audio_device_list());
  mpConfig->cmbSoundOutput->show();

  if (const i32 k = Settings::Config::cmbSoundOutput.get_combobox_index(); k != -1)
  {
    mpConfig->cmbSoundOutput->setCurrentIndex(k);
    emit signal_set_audio_device(mpConfig->cmbSoundOutput->itemData(k).toByteArray());
  }
  else
  {
    emit signal_set_audio_device(QByteArray()); // activates the default audio device
  }

  mAudioLevelDecayTimer.setInterval(50);
  mAudioLevelDecayTimer.setSingleShot(false);
  connect(&mAudioLevelDecayTimer, &QTimer::timeout, this, &AudioManager::_slot_audio_level_decay_timeout);
  mAudioLevelDecayTimer.start();
}

AudioManager::~AudioManager()
{
  if (mAudioOutputThread != nullptr)
  {
    mAudioOutputThread->quit(); // this deletes mpAudioOutput
    mAudioOutputThread->wait();
    delete mAudioOutputThread;
  }
}

void AudioManager::stop_audio_output()
{
  emit signal_stop_audio();
  mPlaybackState = EPlaybackState::Stopped;
  mpCurAudioFifo = nullptr;
}

void AudioManager::stop_all_dumping()
{
  _stop_audio_frame_dumping();
  _stop_audio_dumping();
}

void AudioManager::update_dump_timers()
{
  if (mpAudioFrameDumper != nullptr)
  {
    char text[10];
    seconds_to_timestring(text, mFrameDumpTimer++);
    mpTechDataWidget->framedumpButton->setText(text);
  }
  if (mAudioDumpState == EAudioDumpState::Running)
  {
    char text[10];
    seconds_to_timestring(text, mAudioDumpTimer++);
    mpTechDataWidget->audiodumpButton->setText(text);
  }
}

void AudioManager::slot_show_audio_peak_level(const f32 iPeakLeft, const f32 iPeakRight)
{
  if (iPeakLeft  >= mPeakLeftDamped)  mPeakLeftDamped  = iPeakLeft;
  if (iPeakRight >= mPeakRightDamped) mPeakRightDamped = iPeakRight;
}

void AudioManager::_slot_audio_level_decay_timeout()
{
  mpThermoPeakLevelLeft->setValue(mPeakLeftDamped);
  mpThermoPeakLevelRight->setValue(mPeakRightDamped);

  constexpr float cMinPeakLevelValue   = -23.0f;
  constexpr float cPeakLevelDecayValue =   0.5f;

  mPeakLeftDamped  -= cPeakLevelDecayValue;
  mPeakRightDamped -= cPeakLevelDecayValue;

  if (mPeakLeftDamped  < cMinPeakLevelValue) mPeakLeftDamped  = cMinPeakLevelValue;
  if (mPeakRightDamped < cMinPeakLevelValue) mPeakRightDamped = cMinPeakLevelValue;
}

void AudioManager::_setup_audio_output(const u32 iSampleRate)
{
  mpAudioBufferFromDecoder->flush_ring_buffer();
  mpAudioBufferToOutput->flush_ring_buffer();
  mpCurAudioFifo = &mAudioFifo;
  mpCurAudioFifo->sampleRate = iSampleRate;
  mpCurAudioFifo->pRingbuffer = mpAudioBufferToOutput;

  emit signal_output_sample_rate(iSampleRate / 1000);

  if (mPlaybackState == EPlaybackState::Running)
  {
    emit signal_switch_audio(mpCurAudioFifo);
  }
  else
  {
    mPlaybackState = EPlaybackState::Running;
    emit signal_start_audio(mpCurAudioFifo);
  }
}

void AudioManager::slot_load_audio_device_list(const QList<QAudioDevice> & iDeviceList) const
{
  const QSignalBlocker blocker(mpConfig->cmbSoundOutput);

  mpConfig->cmbSoundOutput->clear();
  for (const QAudioDevice & device : iDeviceList)
  {
    mpConfig->cmbSoundOutput->addItem(device.description(), QVariant::fromValue(device.id()));
  }
}

void AudioManager::slot_handle_volume_slider(const i32 iSliderValue)
{
  if (mMutingActive)
  {
    emit signal_unmute_requested();
  }

  assert(mpAudioOutput != nullptr);
  mpAudioOutput->slot_set_volume(iSliderValue);
}

void AudioManager::slot_set_mute(const bool iMuted)
{
  mMutingActive = iMuted;
  emit signal_audio_mute(iMuted);
}

void AudioManager::slot_set_test_tone(const bool iActive)
{
  emit signal_audio_test_tone(iActive);
}

void AudioManager::slot_set_audio_device(const QByteArray & iDeviceId)
{
  emit signal_set_audio_device(iDeviceId);
}

void AudioManager::slot_update_peak_level_delay(i32 /*iDelaySteps = -1*/)
{
  emit signal_audio_peak_level_delay(mpConfig->sbPeakLevelDelay->value());
}

void AudioManager::new_audio(const i32 iAmount, const u32 iAudioSampleRate, const u32 iAudioFlags)
{
  if (!mIsChannelRunning)
  {
    return;
  }

  if (mAudioFrameCnt++ > 10)
  {
    mAudioFrameCnt = 0;

    const bool sbrUsed = ((iAudioFlags & 0x1) != 0); // DabRadio::AFL_SBR_USED
    const bool psUsed  = ((iAudioFlags & 0x2) != 0); // DabRadio::AFL_PS_USED
    emit signal_sbr_used(sbrUsed);
    emit signal_ps_used(psUsed);

    if (!mpTechDataWidget->isHidden())
    {
      mpTechDataWidget->slot_show_sample_rate_and_audio_flags((i32)iAudioSampleRate, sbrUsed, psUsed);
    }
  }

  if (mpCurAudioFifo == nullptr || mpCurAudioFifo->sampleRate != iAudioSampleRate)
  {
    mAudioBufferFillFiltered = 0.0f;
    _setup_audio_output(iAudioSampleRate);
  }
  assert(mpCurAudioFifo != nullptr);

  if (mAudioDumpState == EAudioDumpState::WaitForInit)
  {
    if (mWavWriter.init(mAudioWavDumpFileName, iAudioSampleRate, 2))
    {
      mAudioDumpState = EAudioDumpState::Running;
    }
    else
    {
      _stop_audio_dumping();
      qCritical("AudioManager::new_audio: Failed to initialize audio dump state");
    }
  }

  assert((iAmount & 1) == 0);

  const i32 availableBytes = mpAudioBufferFromDecoder->get_ring_buffer_read_available();

  if (availableBytes >= iAmount)
  {
    mAudioTempBuffer.resize(availableBytes);
    mean_filter(mAudioBufferFillFiltered, mpAudioBufferToOutput->get_fill_state_in_percent(), 0.2f);

    mpAudioBufferFromDecoder->get_data_from_ring_buffer(mAudioTempBuffer.data(), availableBytes);

    Q_ASSERT(mpCurAudioFifo != nullptr);

    if (availableBytes > mpAudioBufferToOutput->get_ring_buffer_write_available())
    {
      mpAudioBufferToOutput->flush_ring_buffer();
      qWarning("AudioManager::new_audio: Audio output buffer is full, try to start from new");
    }

    mpAudioBufferToOutput->put_data_into_ring_buffer(mAudioTempBuffer.data(), availableBytes);

    if (mAudioDumpState == EAudioDumpState::Running)
    {
      mWavWriter.write(mAudioTempBuffer.data(), availableBytes);
    }

    if (!mProgBarAudioBufferFullColorSet)
    {
      mProgBarAudioBufferFullColorSet = true;
      mpProgBarAudioBuffer->setStyleSheet("QProgressBar { color: #555555; } QProgressBar::chunk { background-color: #EF8B2A; }");
    }
  }

  emit signal_audio_buffer_filled_state((i32)mAudioBufferFillFiltered);
}

void AudioManager::slot_handle_audio_dump_button()
{
  if (!mIsChannelRunning || mIsScanning)
  {
    return;
  }

  switch (mAudioDumpState)
  {
  case EAudioDumpState::Stopped:
    _start_audio_dumping();
    break;
  case EAudioDumpState::WaitForInit:
  case EAudioDumpState::Running:
    _stop_audio_dumping();
    break;
  }
}

void AudioManager::_start_audio_dumping()
{
  if (mAudioFrameType == EAudioFrameType::None || mAudioDumpState != EAudioDumpState::Stopped)
  {
    return;
  }

  mAudioWavDumpFileName = mpOpenFileDialog->get_audio_dump_file_name(mServiceLabel);

  if (mAudioWavDumpFileName.isEmpty())
  {
    return;
  }

  _emphasize_pushbutton(mpTechDataWidget->audiodumpButton, true);
  mAudioDumpState = EAudioDumpState::WaitForInit;
  mAudioDumpTimer = 0;
}

void AudioManager::_stop_audio_dumping()
{
  if (mAudioDumpState == EAudioDumpState::Stopped)
  {
    return;
  }

  mAudioDumpState = EAudioDumpState::Stopped;
  mWavWriter.close();
  _emphasize_pushbutton(mpTechDataWidget->audiodumpButton, false);
  mpTechDataWidget->audiodumpButton->setText("Dump WAV");
}

void AudioManager::slot_handle_frame_dump_button()
{
  if (!mIsChannelRunning || mIsScanning)
  {
    return;
  }

  if (mpAudioFrameDumper != nullptr)
  {
    _stop_audio_frame_dumping();
  }
  else
  {
    _start_audio_frame_dumping();
  }
}

void AudioManager::_start_audio_frame_dumping()
{
  if (mAudioFrameType == EAudioFrameType::None)
  {
    return;
  }

  mpAudioFrameDumper = mpOpenFileDialog->open_frame_dump_file_ptr(mServiceLabel, mAudioFrameType == EAudioFrameType::AAC);

  if (mpAudioFrameDumper == nullptr)
  {
    return;
  }

  _emphasize_pushbutton(mpTechDataWidget->framedumpButton, true);
  mFrameDumpTimer = 0;
}

void AudioManager::_stop_audio_frame_dumping()
{
  if (mpAudioFrameDumper == nullptr)
  {
    return;
  }

  fclose(mpAudioFrameDumper);
  _emphasize_pushbutton(mpTechDataWidget->framedumpButton, false);
  mpAudioFrameDumper = nullptr;

  if (mAudioFrameType == EAudioFrameType::AAC)
    mpTechDataWidget->framedumpButton->setText("Dump AAC");
  else
    mpTechDataWidget->framedumpButton->setText("Dump MP2");
}

void AudioManager::new_aac_mp2_frame() const
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

void AudioManager::_emphasize_pushbutton(QPushButton * const ipPB, const bool iEmphasize) const
{
  ipPB->setStyleSheet(iEmphasize ? "background-color: #AE2B05; color: #EEEE00; font-weight: bold;" : "");
}
