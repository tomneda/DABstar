//
// Created by tomneda on 2025-10-24.
//
#include "dabradio.h"
#include "ui_dabradio.h"
#include "techdata.h"
#include "setting-helper.h"
#include "audiooutputqt.h"
#include "audioiodevice.h"

void DabRadio::_initialize_audio_output()
{
  mpAudioOutput = new AudioOutputQt;
  connect(mpAudioOutput, &IAudioOutput::signal_audio_devices_list, this, &DabRadio::_slot_load_audio_device_list);
  connect(this, &DabRadio::signal_start_audio, mpAudioOutput, &IAudioOutput::slot_start, Qt::QueuedConnection);
  connect(this, &DabRadio::signal_switch_audio, mpAudioOutput, &IAudioOutput::slot_restart, Qt::QueuedConnection);
  connect(this, &DabRadio::signal_stop_audio, mpAudioOutput, &IAudioOutput::slot_stop, Qt::QueuedConnection);
  connect(this, &DabRadio::signal_set_audio_device, mpAudioOutput, &IAudioOutput::slot_set_audio_device, Qt::QueuedConnection);
  connect(this, &DabRadio::signal_audio_mute, mpAudioOutput, &IAudioOutput::slot_mute, Qt::QueuedConnection);
  connect(this, &DabRadio::signal_audio_buffer_filled_state, ui->progBarAudioBuffer, &QProgressBar::setValue);
  connect(ui->sliderVolume, &QSlider::valueChanged, this, &DabRadio::_slot_handle_volume_slider);
  connect(mpAudioOutput->get_audio_io_device(), &AudioIODevice::signal_show_audio_peak_level, this, &DabRadio::slot_show_audio_peak_level, Qt::QueuedConnection);
  connect(mpAudioOutput->get_audio_io_device(), &AudioIODevice::signal_audio_data_available, get_techdata_widget(), &TechData::slot_audio_data_available, Qt::QueuedConnection);

  Settings::Main::sliderVolume.register_widget_and_update_ui_from_setting(ui->sliderVolume, 100); // register after connection then mpAudioOutput get informed immediately

  mAudioOutputThread = new QThread(this);
  mAudioOutputThread->setObjectName("audioOutThr");
  mpAudioOutput->moveToThread(mAudioOutputThread);
  connect(mAudioOutputThread, &QThread::finished, mpAudioOutput, &QObject::deleteLater);
  mAudioOutputThread->start();

  _slot_load_audio_device_list(mpAudioOutput->get_audio_device_list());
  mConfig.cmbSoundOutput->show();

  if (const i32 k = Settings::Config::cmbSoundOutput.get_combobox_index();
      k != -1)
  {
    mConfig.cmbSoundOutput->setCurrentIndex(k);
    emit signal_set_audio_device(mConfig.cmbSoundOutput->itemData(k).toByteArray());
  }
  else
  {
    emit signal_set_audio_device(QByteArray());  // activates the default audio device
  }
}

void DabRadio::slot_show_audio_peak_level(const f32 iPeakLeft, const f32 iPeakRight)
{
  if (iPeakLeft  >= mPeakLeftDamped)  mPeakLeftDamped  = iPeakLeft;
  if (iPeakRight >= mPeakRightDamped) mPeakRightDamped = iPeakRight;
}

void DabRadio::_slot_audio_level_decay_timeout()
{
  ui->thermoPeakLevelLeft->setValue(mPeakLeftDamped);
  ui->thermoPeakLevelRight->setValue(mPeakRightDamped);

  constexpr float cMinPeakLevelValue   = -23.0f;
  constexpr float cPeakLevelDecayValue =   0.5f;

  mPeakLeftDamped  -= cPeakLevelDecayValue;
  mPeakRightDamped -= cPeakLevelDecayValue;

  if (mPeakLeftDamped  < cMinPeakLevelValue) mPeakLeftDamped  = cMinPeakLevelValue;
  if (mPeakRightDamped < cMinPeakLevelValue) mPeakRightDamped = cMinPeakLevelValue;
}

void DabRadio::_setup_audio_output(const u32 iSampleRate)
{
  mpAudioBufferFromDecoder->flush_ring_buffer();
  mpAudioBufferToOutput->flush_ring_buffer();
  mpCurAudioFifo = &mAudioFifo;
  mpCurAudioFifo->sampleRate = iSampleRate;
  mpCurAudioFifo->pRingbuffer = mpAudioBufferToOutput;

  _set_status_info_status(mStatusInfo.OutSampRate, (u32)(iSampleRate / 1000));

  if (mPlaybackState == EPlaybackState::Running)
  {
    emit signal_switch_audio(mpCurAudioFifo);
  }
  else
  {
    mPlaybackState = EPlaybackState::Running;
    emit signal_start_audio(mpCurAudioFifo);
  }
  mResetRingBufferCnt = 0;
}

void DabRadio::_slot_load_audio_device_list(const QList<QAudioDevice> & iDeviceList) const
{
  const QSignalBlocker blocker(mConfig.cmbSoundOutput); // block signals as the settings would be updated always with the first written entry

  mConfig.cmbSoundOutput->clear();
  for (const QAudioDevice &device : iDeviceList)
  {
    mConfig.cmbSoundOutput->addItem(device.description(), QVariant::fromValue(device.id()));
  }
}

void DabRadio::_slot_handle_volume_slider(const i32 iSliderValue)
{
  // if muting is currently active, unmute audio with touching the volume slider
  if (mMutingActive)
  {
    _slot_update_mute_state(false);
  }

  assert(mpAudioOutput != nullptr);
  mpAudioOutput->slot_setVolume(iSliderValue);
}

//
// In order to not overload with an enormous amount of
// signals, we trigger this function at most 10 times a second
//
void DabRadio::slot_new_audio(const i32 iAmount, const u32 iAudioSampleRate, const u32 iAudioFlags)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (mAudioFrameCnt++ > 10)
  {
    mAudioFrameCnt = 0;

    const bool sbrUsed = ((iAudioFlags & DabRadio::AFL_SBR_USED) != 0);
    const bool psUsed  = ((iAudioFlags & DabRadio::AFL_PS_USED) != 0);
    _set_status_info_status(mStatusInfo.SBR, sbrUsed);
    _set_status_info_status(mStatusInfo.PS, psUsed);

    if (!mpTechDataWidget->isHidden())
    {
      mpTechDataWidget->slot_show_sample_rate_and_audio_flags((i32)iAudioSampleRate, sbrUsed, psUsed);
    }
  }

  if (mpCurAudioFifo == nullptr || mpCurAudioFifo->sampleRate != iAudioSampleRate)
  {
    mAudioBufferFillFiltered = 0.0f; // set back the audio buffer progress bar
    _setup_audio_output(iAudioSampleRate);
  }
  assert(mpCurAudioFifo != nullptr); // is mAudioBuffer2 really assigned?

  if (mAudioDumpState == EAudioDumpState::WaitForInit)
  {
    if (mWavWriter.init(mAudioWavDumpFileName, iAudioSampleRate, 2))
    {
      mAudioDumpState = EAudioDumpState::Running;
    }
    else
    {
      stop_audio_dumping();
      qCritical("RadioInterface::slot_new_audio: Failed to initialize audio dump state");
    }
  }

  assert((iAmount & 1) == 0); // assert that there are always a stereo pair of data

  const i32 availableBytes = mpAudioBufferFromDecoder->get_ring_buffer_read_available();

  if (availableBytes >= iAmount) // should always be true when this slot method is called
  {
    mAudioTempBuffer.resize(availableBytes); // after max. size set, there will be no further memory reallocation
    mean_filter_adaptive(mAudioBufferFillFiltered, mpAudioBufferToOutput->get_fill_state_in_percent(), 50.0f, 1.0f, 0.05f);

    // mAudioTempBuffer contains a stereo pair [0][1]...[n-2][n-1]
    mpAudioBufferFromDecoder->get_data_from_ring_buffer(mAudioTempBuffer.data(), availableBytes);

    Q_ASSERT(mpCurAudioFifo != nullptr);

    // the audio output buffer got flooded, try to begin from new (set to 50% would also be ok, but needs interface in RingBuffer)
    if (availableBytes > mpAudioBufferToOutput->get_ring_buffer_write_available())
    {
      mpAudioBufferToOutput->flush_ring_buffer();
      qDebug("RadioInterface::slot_new_audio: Audio output buffer is full, try to start from new");
    }

    mpAudioBufferToOutput->put_data_into_ring_buffer(mAudioTempBuffer.data(), availableBytes);

    if (mAudioDumpState == EAudioDumpState::Running)
    {
      mWavWriter.write(mAudioTempBuffer.data(), availableBytes);
    }

    // ugly, but palette of progressbar can only be set in the same thread of the value setting, save time calling this only once
    if (!mProgBarAudioBufferFullColorSet)
    {
      mProgBarAudioBufferFullColorSet = true;
      QPalette p = ui->progBarAudioBuffer->palette();
      p.setColor(QPalette::Highlight, 0xEF8B2A);
      ui->progBarAudioBuffer->setPalette(p);
    }
  }

  emit signal_audio_buffer_filled_state((i32)mAudioBufferFillFiltered);
}

void DabRadio::_slot_handle_audio_dump_button()
{
  if (!mIsRunning.load() || mIsScanning.load())
  {
    return;
  }

  switch (mAudioDumpState)
  {
  case EAudioDumpState::Stopped:
    start_audio_dumping();
    break;
  case EAudioDumpState::WaitForInit:
  case EAudioDumpState::Running:
    stop_audio_dumping();
    break;
  }
}

void DabRadio::start_audio_dumping()
{
  if (mAudioFrameType == EAudioFrameType::None ||
      mAudioDumpState != EAudioDumpState::Stopped)
  {
    return;
  }

  mAudioWavDumpFileName = mOpenFileDialog.get_audio_dump_file_name(mChannel.curPrimaryService.serviceLabel);

  if (mAudioWavDumpFileName.isEmpty())
  {
    return;
  }
  LOG("Audio dump starts ", ui->serviceLabel->text());
  _emphasize_pushbutton(mpTechDataWidget->audiodumpButton, true);
  mAudioDumpState = EAudioDumpState::WaitForInit;
}

void DabRadio::stop_audio_dumping()
{
  if (mAudioDumpState == EAudioDumpState::Stopped)
  {
    return;
  }

  LOG("Audio dump stops", "");
  mAudioDumpState = EAudioDumpState::Stopped;
  mWavWriter.close();
  _emphasize_pushbutton(mpTechDataWidget->audiodumpButton, false);
}

void DabRadio::_slot_handle_frame_dump_button()
{
  if (!mIsRunning.load() || mIsScanning.load())
  {
    return;
  }

  if (mpAudioFrameDumper != nullptr)
  {
    stop_audio_frame_dumping();
  }
  else
  {
    start_audio_frame_dumping();
  }
}

void DabRadio::start_audio_frame_dumping()
{
  if (mAudioFrameType == EAudioFrameType::None)
  {
    return;
  }

  mpAudioFrameDumper = mOpenFileDialog.open_frame_dump_file_ptr(mChannel.curPrimaryService.serviceLabel, mAudioFrameType == EAudioFrameType::AAC);

  if (mpAudioFrameDumper == nullptr)
  {
    return;
  }

  _emphasize_pushbutton(mpTechDataWidget->framedumpButton, true);
}

void DabRadio::stop_audio_frame_dumping()
{
  if (mpAudioFrameDumper == nullptr)
  {
    return;
  }

  fclose(mpAudioFrameDumper);
  _emphasize_pushbutton(mpTechDataWidget->framedumpButton, false);
  mpAudioFrameDumper = nullptr;
}

void DabRadio::slot_new_aac_mp2_frame()
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (mpAudioFrameDumper == nullptr)
  {
    mpFrameBuffer->flush_ring_buffer(); // we do not need the collected AAC or MP2 streams
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

      if (mpAudioFrameDumper != nullptr) // ask again here because it could get invalid (could still happen short time)
      {
        fwrite(buffer.data(), dataSizeRead, 1, mpAudioFrameDumper);
      }
    }
  }
}

