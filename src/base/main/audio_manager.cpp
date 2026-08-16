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
#include "configuration.h"
#include "techdata.h"
#include "setting_helper.h"
#include "glob_defs.h"
#include <QSlider>
#include <QThread>
#include <cassert>
#include "level_meter.h"

AudioManager::AudioManager(const SResourceConfig & cfg, QObject * parent)
  : QObject(parent)
  , mpAudioBufferFromDecoder(cfg.pAudioBufferFromDecoder)
  , mpAudioBufferToOutput(cfg.pAudioBufferToOutput)
  , mpFrameBuffer(cfg.pFrameBuffer)
  , mpConfig(cfg.pConfig)
  , mpTechDataWidget(cfg.pTechDataWidget)
  , mpLevelMeterLeft(cfg.pLevelMeterLeft)
  , mpLevelMeterRight(cfg.pLevelMeterRight)
  , mpSliderVolume(cfg.pSliderVolume)
  , mpOpenFileDialog(cfg.pOpenFileDialog)
{
  mAudioOutputThread = new QThread(this);
  mAudioOutputThread->setObjectName("audioOutThr");

  mpAudioPipeline = new AudioPipeline(mpAudioBufferFromDecoder, mpAudioBufferToOutput, mpFrameBuffer);
  mpAudioPipeline->moveToThread(mAudioOutputThread);

  connect(mpAudioPipeline, &AudioPipeline::signal_audio_devices_list, this, &AudioManager::slot_load_audio_device_list);
  connect(mpAudioPipeline, &AudioPipeline::signal_show_audio_peak_level, this, &AudioManager::slot_show_audio_peak_level);
  connect(mpAudioPipeline, &AudioPipeline::signal_audio_data_available, mpTechDataWidget, &TechData::slot_audio_data_available, Qt::QueuedConnection);
  connect(mpAudioPipeline, &AudioPipeline::signal_sbr_used, this, &AudioManager::signal_sbr_used);
  connect(mpAudioPipeline, &AudioPipeline::signal_ps_used, this, &AudioManager::signal_ps_used);
  connect(mpAudioPipeline, &AudioPipeline::signal_output_sample_rate, this, &AudioManager::signal_output_sample_rate);
  connect(mpAudioPipeline, &AudioPipeline::signal_audio_buffer_filled_state, this, &AudioManager::signal_audio_buffer_filled_state);
  connect(mpAudioPipeline, &AudioPipeline::signal_sample_rate_and_audio_flags, this, &AudioManager::slot_show_sample_rate_and_audio_flags);
  connect(mpAudioPipeline, &AudioPipeline::signal_audio_dump_state_changed, this, &AudioManager::slot_audio_dump_state_changed);
  connect(mpAudioPipeline, &AudioPipeline::signal_frame_dump_state_changed, this, &AudioManager::slot_frame_dump_state_changed);

  connect(mpSliderVolume, &QSlider::valueChanged, this, &AudioManager::slot_handle_volume_slider);
  connect(mAudioOutputThread, &QThread::finished, mpAudioPipeline, &QObject::deleteLater);

  mAudioOutputThread->start(QThread::HighPriority);

  slot_update_peak_level_delay(); // write startup value

  slot_load_audio_device_list(mpAudioPipeline->get_audio_output()->get_audio_device_list());
  mpConfig->cmbSoundOutput->show();

  if (const i32 k = Settings::Config::cmbSoundOutput.get_combobox_index(); k != -1)
  {
    mpConfig->cmbSoundOutput->setCurrentIndex(k);
    slot_set_audio_device(mpConfig->cmbSoundOutput->itemData(k).toByteArray());
  }
  else
  {
    slot_set_audio_device(QByteArray()); // activates the default audio device
  }

  // connect after first slot_update_peak_level_delay() call to inhibit redundant call
  connect(mpConfig->sbPeakLevelDelay, &QSpinBox::valueChanged, this, &AudioManager::slot_update_peak_level_delay);

  Settings::Main::sliderVolume.register_widget_and_update_ui_from_setting(mpSliderVolume, 100);
}

AudioManager::~AudioManager()
{
  if (mAudioOutputThread != nullptr)
  {
    mAudioOutputThread->quit();
    mAudioOutputThread->wait();
    delete mAudioOutputThread;
  }
}

void AudioManager::set_channel_running(const bool isRunning)
{
  mIsChannelRunning = isRunning;
  QMetaObject::invokeMethod(mpAudioPipeline, "slot_set_channel_running", Qt::QueuedConnection, Q_ARG(bool, isRunning));
}

void AudioManager::set_scanning(const bool isScanning)
{
  mIsScanning = isScanning;
  QMetaObject::invokeMethod(mpAudioPipeline, "slot_set_scanning", Qt::QueuedConnection, Q_ARG(bool, isScanning));
}

void AudioManager::set_service_label(const QString & label)
{
  mServiceLabel = label;
  QMetaObject::invokeMethod(mpAudioPipeline, "slot_set_service_label", Qt::QueuedConnection, Q_ARG(QString, label));
}

void AudioManager::set_audio_frame_type(const EAudioFrameType type)
{
  mAudioFrameType = type;
  QMetaObject::invokeMethod(mpAudioPipeline, "slot_set_audio_frame_type", Qt::QueuedConnection, Q_ARG(AudioPipeline::EAudioFrameType, type));
}

void AudioManager::reset_audio_fifo()
{
  QMetaObject::invokeMethod(mpAudioPipeline, "slot_reset_audio_fifo", Qt::QueuedConnection);
}

void AudioManager::stop_audio_output()
{
  QMetaObject::invokeMethod(mpAudioPipeline, "slot_stop_audio", Qt::QueuedConnection);
}

void AudioManager::stop_all_dumping()
{
  mAudioDumpRunning = false;
  mFrameDumpRunning = false;
  QMetaObject::invokeMethod(mpAudioPipeline, "slot_stop_all_dumping", Qt::QueuedConnection);
  mpTechDataWidget->set_audio_dump_button_emphasized(false);
  mpTechDataWidget->set_frame_dump_button_emphasized(false);
  mpTechDataWidget->audiodumpButton->setText("Dump WAV");
  if (mAudioFrameType == EAudioFrameType::AAC)
    mpTechDataWidget->framedumpButton->setText("Dump AAC");
  else
    mpTechDataWidget->framedumpButton->setText("Dump MP2");
}

void AudioManager::slot_show_sample_rate_and_audio_flags(const i32 iSampleRate, const bool iSbrUsed, const bool iPsUsed) const
{
  if (!mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->slot_show_sample_rate_and_audio_flags(iSampleRate, iSbrUsed, iPsUsed);
  }
}

QString AudioManager::_seconds_to_timestring(const u32 iTimer) const
{
  return QString::asprintf("%d:%02d:%02d", iTimer / 3600, (iTimer / 60) % 60, iTimer % 60);
}

void AudioManager::update_dump_timers() const
{
  if (mFrameDumpRunning)
  {
    mpTechDataWidget->framedumpButton->setText(_seconds_to_timestring(mFrameDumpTimer++));
  }
  if (mAudioDumpRunning)
  {
    mpTechDataWidget->audiodumpButton->setText(_seconds_to_timestring(mAudioDumpTimer++));
  }
}

void AudioManager::_update_level_meter(LevelMeter * const ipMeter, const f32 iPeak, const f32 iRms) const
{
  const f32 minValue = ipMeter->get_lower_bound();
  const f32 maxValue = ipMeter->get_upper_bound();
  const f32 range = maxValue - minValue;
  if (range <= 0) return;

  f32 relPosRms  = (iRms  - minValue) / range;
  f32 relPosPeak = (iPeak - minValue) / range;

  const f32 relPos0dB = (0.0f - minValue) / range;

  relPosRms  = std::clamp<f32>(relPosRms,  0.0f, relPos0dB);
  relPosPeak = std::clamp<f32>(relPosPeak, 0.0f, relPos0dB);
  relPosRms  = std::min(relPosRms, relPosPeak);

  constexpr f32 eps = 1.0f / 255.0f;
  const f32 rmsNext  = std::min(relPosRms,  relPosPeak - eps);
  const f32 peakNext = std::min(relPosPeak, 1.0f - eps);

  // Each segment: dark start → bright end (gradient within segment).
  // Boundary is visible as a hard bright→dark jump between segments.
  ipMeter->set_color_stops({
    { 0.0f,       0x993300 },  // dark rust            (RMS start)
    { relPosRms,  0xDD7700 },  // bright orange        (RMS end)
    { rmsNext,    0xBB8800 },  // dark amber           (Peak start — boundary)
    { relPosPeak, 0xDDCC00 },  // bright yellow        (Peak end)
    { peakNext,   0x781414 },  // dark red              (Overflow start — boundary)
    { 1.0f,       0xFF2828 },  // bright red            (Overflow end)
  });

  ipMeter->set_value(iPeak);
}

void AudioManager::slot_show_audio_peak_level(const f32 iPeakLeftDb, const f32 iPeakRightDb, const f32 iRmsLeftDb, const f32 iRmsRightDb)
{
  // each 50ms...
  _update_level_meter(mpLevelMeterLeft, iPeakLeftDb, iRmsLeftDb);
  _update_level_meter(mpLevelMeterRight, iPeakRightDb, iRmsRightDb);
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

  QMetaObject::invokeMethod(mpAudioPipeline, "slot_set_volume", Qt::QueuedConnection, Q_ARG(i32, iSliderValue));
}

void AudioManager::slot_set_mute(const bool iMuted)
{
  mMutingActive = iMuted;
  QMetaObject::invokeMethod(mpAudioPipeline, "slot_set_mute", Qt::QueuedConnection, Q_ARG(bool, iMuted));
}

void AudioManager::slot_set_test_tone(const bool iActive)
{
  QMetaObject::invokeMethod(mpAudioPipeline, "slot_set_test_tone", Qt::QueuedConnection, Q_ARG(bool, iActive));
}

void AudioManager::slot_set_audio_device(const QByteArray & iDeviceId)
{
  QMetaObject::invokeMethod(mpAudioPipeline, "slot_set_audio_device", Qt::QueuedConnection, Q_ARG(QByteArray, iDeviceId));
}

void AudioManager::slot_update_peak_level_delay(i32 /*iDelaySteps = -1*/)
{
  const i32 val = mpConfig->sbPeakLevelDelay->value();
  QMetaObject::invokeMethod(mpAudioPipeline, "slot_set_peak_level_delay", Qt::QueuedConnection, Q_ARG(i32, val));
}

void AudioManager::slot_handle_audio_dump_button()
{
  if (!mIsChannelRunning || mIsScanning)
  {
    return;
  }

  if (mAudioDumpRunning)
  {
    QMetaObject::invokeMethod(mpAudioPipeline, "slot_stop_audio_dump", Qt::QueuedConnection);
  }
  else
  {
    if (mAudioFrameType == EAudioFrameType::None)
    {
      return;
    }

    const QString fileName = mpOpenFileDialog->get_audio_dump_file_name(mServiceLabel);
    if (fileName.isEmpty())
    {
      return;
    }

    mAudioDumpTimer = 0;
    QMetaObject::invokeMethod(mpAudioPipeline, "slot_start_audio_dump", Qt::QueuedConnection, Q_ARG(QString, fileName));
  }
}

void AudioManager::slot_audio_dump_state_changed(const bool iIsDumping)
{
  mAudioDumpRunning = iIsDumping;
  mpTechDataWidget->set_audio_dump_button_emphasized(iIsDumping);
  if (!iIsDumping)
  {
    mpTechDataWidget->audiodumpButton->setText("Dump WAV");
  }
}

void AudioManager::slot_handle_frame_dump_button()
{
  if (!mIsChannelRunning || mIsScanning)
  {
    return;
  }

  if (mFrameDumpRunning)
  {
    QMetaObject::invokeMethod(mpAudioPipeline, "slot_stop_frame_dump", Qt::QueuedConnection);
  }
  else
  {
    if (mAudioFrameType == EAudioFrameType::None)
    {
      return;
    }

    const QString fileName = mpOpenFileDialog->get_frame_dump_file_name(mServiceLabel, mAudioFrameType == EAudioFrameType::AAC);
    if (fileName.isEmpty())
    {
      return;
    }

    mFrameDumpTimer = 0;
    QMetaObject::invokeMethod(mpAudioPipeline, "slot_start_frame_dump", Qt::QueuedConnection, Q_ARG(QString, fileName));
  }
}

void AudioManager::slot_frame_dump_state_changed(const bool iIsDumping)
{
  mFrameDumpRunning = iIsDumping;
  mpTechDataWidget->set_frame_dump_button_emphasized(iIsDumping);
  if (!iIsDumping)
  {
    if (mAudioFrameType == EAudioFrameType::AAC)
      mpTechDataWidget->framedumpButton->setText("Dump AAC");
    else
      mpTechDataWidget->framedumpButton->setText("Dump MP2");
  }
}
