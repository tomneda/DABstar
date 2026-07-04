//
// Created by tomneda on 2026-04-04.
//
#include "dabradio.h"
#include "spectrum_viewer.h"
#include "cir_viewer.h"
#include "service_list_handler.h"
#include "map_http_server.h"
#include "setting_helper.h"
#include "techdata.h"
#include "ui_dabradio.h"
#include "configuration.h"
#include "audio_manager.h"
#include "mot_slide_progress.h"


void DabRadio::_create_and_init_dab_processor()
{
  assert(mpDabProcessor == nullptr);
  mpDabProcessor.reset(new DabProcessor(this, mpInputDevice.get(), &mProcessParams));
  assert(mpDabProcessor != nullptr);

  _connect_dab_processor_signals();
  mpEpgMotHandler->set_dab_processor(mpDabProcessor.get());
  mpTiiManager->set_dab_processor(mpDabProcessor.get());

  mpDabProcessor->set_scan_mode(mIsScanning);
  mpDabProcessor->set_sync_on_strongest_peak(Settings::Config::cbUseStrongestPeak.read().toBool());
  mpDabProcessor->set_dc_avoidance_algorithm(!mIsFileMode && Settings::Config::cbUseDcAvoidance.read().toBool());
  mpDabProcessor->set_dc_and_iq_correction(Settings::Config::cbDoDcCorrOnly.read().toBool() || Settings::Config::cbDoDcAndIqCorr.read().toBool(),
                                           Settings::Config::cbDoDcAndIqCorr.read().toBool());
  mpDabProcessor->set_tii_collisions(Settings::Config::cbTiiCollisions.read().toBool());
  mpDabProcessor->set_tii_processing(true);
  mpDabProcessor->set_tii_threshold(Settings::Config::sbTiiThreshold.read().toInt());
  mpDabProcessor->set_tii_sub_id(Settings::Config::sbTiiSubId.read().toInt());
  mpDabProcessor->slot_soft_bit_gen_type((ESoftBitType)mpConfig->cmbSoftBitGen->currentIndex());
}

void DabRadio::_delete_dab_processor()
{
  // Be aware: DabProcessor (if existing) holds the pointer to the inputDevice, so it must be valid also while destruction of the DabProcessor
  assert(mpDabProcessor == nullptr || mpInputDevice != nullptr);
  _disconnect_dab_processor_signals();
  mpEpgMotHandler->set_dab_processor(nullptr);
  mpTiiManager->set_dab_processor(nullptr);
  mpDabProcessor.reset(); // deletes the DAB processor
}

void DabRadio::_clean_up_dab_processor_and_input_device()
{
  if (mIsChannelRunning)
  {
    _stop_channel(); // stops also the DAB processor (but not deleted it), mIsChannelRunning -> false
    mChannelDesc.clean_channel();
  }

  assert(!mIsChannelRunning);

  if (mpDabProcessor != nullptr)
  {
    _delete_dab_processor();
  }

  if (mpInputDevice != nullptr)
  {
    // Unload former device and give it a bit time to unload
    mpInputDevice.reset();
    usleep(250'000);
  }
}

// This is called either direct after device selection or via ensemble list in file mode
void DabRadio::_create_new_input_device_and_dab_processor(const QString & iDeviceNameOrFileName)
{
  _clean_up_dab_processor_and_input_device();

  // Here, a new device handler will created, a maybe former existing will be destroyed
  // If it is a file and the file is invalid, then it returns nullptr else well
  mpInputDevice = mpDeviceSelector->create_device(iDeviceNameOrFileName, mIsFileMode, mIsScanning);

  if (mpInputDevice == nullptr) // something went wrong with the device creation
  {
    if (!mIsScanning)
    {
      _write_warning_message(mpDeviceSelector->get_message());
    }
    return; // nothing will happen
  }

  mpTiiManager->hide_tii_display();

  if (mIsFileMode)
  {
    if (auto * const pDeviceAsQObject = dynamic_cast<QObject *>(mpInputDevice.get()))
    {
      connect(pDeviceAsQObject, SIGNAL(signal_file_looped()), this, SLOT(_slot_handle_file_looped()));
    }
  }

  _create_and_init_dab_processor();
  assert(mpDabProcessor != nullptr);

  emit signal_dab_processor_started(); // triggers the DAB processor rereading (new) settings

  if (Settings::CirViewer::varUiVisible.read().toBool())
  {
    mpDabProcessor->activate_cir_viewer(true);
    mpCirViewer->show();
  }
}

void DabRadio::_stop_services(const bool iStopAlsoGlobServices)
{
  mEnsListRetriggerTimer.stop();
  mScanSecurityTimer.stop();

  if (mpDabProcessor == nullptr)
  {
    return;
  }

  mpAudioManager->stop_all_dumping();

  mpTechDataWidget->cleanUp(); // clean up the technical widget
  _cleanup_ui(); // clean up this main widget

  mpAudioManager->stop_audio_output();

  if (iStopAlsoGlobServices)
  {
    mpDabProcessor->stop_all_services();
  }
  else
  {
    // TODO: keep "global" services like EPGs running, but currently clear all services in the MSC handler
    mpDabProcessor->stop_all_services();
  }

  mCurPrimaryAudioService = {};
  mCurPrimaryPacketService = {};
  mCurSecondaryServiceVec.clear();

  mpAudioManager->set_audio_frame_type(AudioManager::EAudioFrameType::None);
  mpEpgMotHandler->on_stop_services();
  mpMotSlideProgress->reset();

  mpEpgMotHandler->show_pause_slide();
  _clean_screen(mStatusInfo);
}

void DabRadio::_start_channel(const QString & iFIdOrCh, const u32 iSId)
{
  assert(mpInputDevice != nullptr);
  const i32 tunedFrequencyHz = (mIsFileMode ? mpInputDevice->getVFOFrequency() : mpEnsembleList->get_band_handler().get_frequency_Hz(iFIdOrCh));
  mChannelDesc.deferredData.nomFreqHz = tunedFrequencyHz;
  mpSpectrumViewer->show_nominal_frequency_MHz((f32)tunedFrequencyHz / 1'000'000.0f);
  mDipSyncState = EDipSyncState::NotSetYet;
  mpInputDevice->resetBuffer();
  mpInputDevice->restartReader(tunedFrequencyHz);

  mServiceList.clear();

  mpServiceListHandler->set_selector(iFIdOrCh, iSId); // this highlights the suitable row in the service list

  // Deletes the transmitter entries in the HTTP map
  if (mpHttpHandler != nullptr)
  {
    mpHttpHandler->add_transmitter_location_entry(MAP_RESET, nullptr, "", 0, 0, 0, false);
  }

  if (!mIsFileMode)
  {
    usleep(250'000); // wait for the reader to start as some LOs on some devices need some more time to swing-in (TODO: better reset FIB decoder later?)
  }

  _enable_ui_elements_for_safety(!mIsScanning);

  mpDabProcessor->start(); // resets also the FIB decoder

  if (iSId > 0)
  {
    mpDabProcessor->get_fib_decoder()->set_SId_for_fast_audio_selection(iSId);
  }

  if (!mIsScanning)
  {
    _write_fid_or_Ch_to_settings(iFIdOrCh);
    mEpgTimer.start(cEpgTimeoutMs);
  }

  mIsChannelRunning.store(true);
  mpAudioManager->set_channel_running(true);
  mpEpgMotHandler->set_channel_running(true);
  mpTiiManager->set_channel_running(true);
}

// Apart from stopping the reader, a lot of administration is to be done.
void DabRadio::_stop_channel()
{
  if (mpInputDevice == nullptr) // should not happen
  {
    return;
  }

  _stop_eti_handler(mDumpStatus);
  _stop_services(true);
  _stop_source_dumping(mDumpStatus);

  if (mpFibContentTable != nullptr)
  {
    mpFibContentTable->hide();
    mpFibContentTable.reset();
  }

  if (mpFicDumpPointer != nullptr)
  {
    mpDabProcessor->stop_fic_dump();
    mpFicDumpPointer = nullptr;
  }

  mEpgTimer.stop();
  mpInputDevice->stopReader();
  mpDabProcessor->stop();
  usleep(1000); // minimum task-switch time on Windows; lets the processor threads wind down
  mpTechDataWidget->cleanUp();
  mEnsListRetriggerTimer.stop();
  mScanSecurityTimer.stop();

  if (mpHttpHandler != nullptr)
  {
    mpHttpHandler->add_transmitter_location_entry(MAP_RESET, nullptr, "", 0, 0, 0, false);
  }

  ui->lblEnsName->setText("");
  ui->lblCountryName->setText("");
  ui->progBarFicError->setValue(0);

  _enable_ui_elements_for_safety(false);  // hide some buttons

  mServiceList.clear();
  _clean_screen(mStatusInfo);
  mpTiiManager->clear_tii_list_and_label();

  mIsChannelRunning.store(false);
  mpAudioManager->set_channel_running(false);
  mpEpgMotHandler->set_channel_running(false);
  mpTiiManager->set_channel_running(false);
  qDebug() << "Channel stopped";
}

// This is called after selecting the device or new file
void DabRadio::_start_playing(const QString & iFIdOrCh, const u32 iSId, const bool iSameFIdOrCh)
{
  if (mpInputDevice == nullptr)
  {
    qCritical() << "Input device is not running";
    return;
  }

  if (mpDabProcessor == nullptr)  // should not happen
  {
    qCritical() << "mpDabProcessor is not running";
    return;
  }

  if (!mIsChannelRunning)
  {
    // qDebug() << "Sample reader is not running";
    mChannelDesc.clean_channel();
    _start_channel(iFIdOrCh, iSId);
    assert(mIsChannelRunning);
  }

  if (!mIsScanning)
  {
    if (iSameFIdOrCh)  // Is it only a service change within the same channel/file?
    {
      if (iSId > 0)
      {
        qDebug() << "Service change within the same channel/file";
        // Stop running audio services but maintain global services (like EPG)
        _stop_services(false);

        if (_start_primary_and_secondary_service(iSId, false))
        {
          mpServiceListHandler->set_selector(iFIdOrCh, iSId);
          _inform_ensemble_list(mChannelDesc.get_ident_info(), EInfoReason::NewSId); // only the SId has changed, store it in the Ensemble List
        }
      }
    }
    else  // channel (and service) has changed
    {
      if (iSId == 0)
      {
        _write_warning_message("Choose a service from the service list");
      }
    }
  }
}

