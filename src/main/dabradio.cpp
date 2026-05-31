/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C)  2015, 2016, 2017, 2018, 2019, 2020, 2021
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "dabradio.h"
#include "ui_dabradio.h"
#include "dab-tables.h"
#include "techdata.h"
#include "service-list-handler.h"
#include "setting-helper.h"
#include "time-table.h"
#include "map-http-server.h"
#include "updatechecker.h"
#include "updatedialog.h"
#include "appversion.h"
#include "spectrum-viewer.h"
#include "cir-viewer.h"
#include "configuration.h"
#include "ensemble-list.h"
#include "itu-regions.h"
#include "audio_manager.h"
#include <QMessageBox>
#include <QDesktopServices>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <unistd.h>
#endif

// Q_LOGGING_CATEGORY(sLogDabRadio, "DabRadio", QtDebugMsg)
Q_LOGGING_CATEGORY(sLogDabRadio, "DabRadio", QtInfoMsg)

DabRadio::DabRadio(QSettings * const ipSettings, const QString & iServiceListDbFileName, const QString & iEnsembleListDbFileName, const i32 iDataPort, QWidget * iParent)
  : QWidget(iParent)
  , ui(new Ui_DabRadio)
  , mpSpectrumBuffer(sRingBufferFactoryCmplx.get_ringbuffer(RingBufferFactory<cf32>::EId::SpectrumBuffer).get())
  , mpIqBuffer(sRingBufferFactoryCmplx.get_ringbuffer(RingBufferFactory<cf32>::EId::IqBuffer).get())
  , mpCarrBuffer(sRingBufferFactoryFloat.get_ringbuffer(RingBufferFactory<f32>::EId::CarrBuffer).get())
  , mpResponseBuffer(sRingBufferFactoryFloat.get_ringbuffer(RingBufferFactory<f32>::EId::ResponseBuffer).get())
  , mpFrameBuffer(sRingBufferFactoryUInt8.get_ringbuffer(RingBufferFactory<u8>::EId::FrameBuffer).get())
  , mpDataBuffer(sRingBufferFactoryUInt8.get_ringbuffer(RingBufferFactory<u8>::EId::DataBuffer).get())
  , mpAudioBufferFromDecoder(sRingBufferFactoryInt16.get_ringbuffer(RingBufferFactory<i16>::EId::AudioFromDecoder).get())
  , mpAudioBufferToOutput(sRingBufferFactoryInt16.get_ringbuffer(RingBufferFactory<i16>::EId::AudioToOutput).get())
  , mpTechDataBuffer(sRingBufferFactoryInt16.get_ringbuffer(RingBufferFactory<i16>::EId::TechDataBuffer).get())
  , mpCirBuffer(sRingBufferFactoryCmplx.get_ringbuffer(RingBufferFactory<cf32>::EId::CirBuffer).get())
  , mpSpectrumViewer(new SpectrumViewer(this, ipSettings, mpSpectrumBuffer, mpIqBuffer, mpCarrBuffer, mpResponseBuffer))
  , mpCirViewer(new CirViewer(mpCirBuffer))
  , mpConfig(new Configuration(this))
  , mpItuTables(new ItuTables())
  , mOpenFileDialog(ipSettings)
  , mDeviceSelector(ipSettings)
{
  // "mProcessParams" is introduced to reduce the number of parameters for the dabProcessor
  mProcessParams.spectrumBuffer = mpSpectrumBuffer;
  mProcessParams.iqBuffer = mpIqBuffer;
  mProcessParams.carrBuffer = mpCarrBuffer;
  mProcessParams.responseBuffer = mpResponseBuffer;
  mProcessParams.frameBuffer = mpFrameBuffer;
  mProcessParams.cirBuffer = mpCirBuffer;
  mProcessParams.threshold = 3.0f;
  mProcessParams.tiiFramesToCount = 5;

  // set on top or not? checked at start up
  if (Settings::Config::cbAlwaysOnTop.read().toBool())
  {
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
  }

  ui->setupUi(this);

  setWindowTitle(PRJ_NAME);

  Settings::Main::posAndSize.read_widget_geometry(this, false, true, 0, 30 /*reserve height of dyn. info fields*/);
  Settings::Main::cbAutoNextService.register_widget_and_update_ui_from_setting(ui->cbAutoNextService, 0);

  mpServiceListHandler.reset(new ServiceListHandler(iServiceListDbFileName, ui->tblServiceList));
  mpTechDataWidget.reset(new TechData(this, mpTechDataBuffer));
  mpEnsembleList.reset(new EnsembleList(iEnsembleListDbFileName));

  qDebug("Using Qt version: " QT_VERSION_STR);

  _initialize_audio_output();
  _initialize_epg_mot_handler();
  _initialize_tii_manager();
  _initialize_signal_slot_connections();
  _initialize_ui_elements();
  _initialize_ensemble_list();
  _initialize_status_info();
  _initialize_dynamic_label();
  _initialize_thermo_peak_levels();
  _initialize_time_table();
  _initialize_version_and_copyright_info();
  _initialize_device_selector(mChannelDesc);

  _show_pause_slide();
  _set_clock_text();

  _get_local_position_from_config(mLocalPos);
  mpTiiManager->set_local_pos(mLocalPos);

  mpConfig->cmbSoftBitGen->addItems(_get_soft_bit_gen_names()); // fill soft-bit-type combobox with text elements
  mpServiceListHandler->restore_favorites(); // try to restore former favorites from a backup table

#ifdef DATA_STREAMER
  dataStreamer = new tcpServer(iDataPort);
#else
  (void)iDataPort;
#endif

  connect(ui->btnHttpServer, &QPushButton::clicked, this,  &DabRadio::_slot_handle_http_button);
  connect(mpTechDataWidget.get(), &TechData::signal_handle_timeTable, this, &DabRadio::_slot_handle_time_table);
  connect(this, &DabRadio::signal_dab_processor_started, mpSpectrumViewer.get(), &SpectrumViewer::slot_update_settings);
  connect(mpSpectrumViewer.get(), &SpectrumViewer::signal_window_closed, this, &DabRadio::_slot_handle_spectrum_button);
  connect(mpTechDataWidget.get(), &TechData::signal_window_closed, this, &DabRadio::_slot_handle_tech_detail_button);
  connect(ui->configButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_config_button);
  connect(mpCirViewer.get(), &CirViewer::signal_frame_closed, this, &DabRadio::_slot_handle_cir_button);

  _initialize_and_start_timers();

  _show_or_hide_windows_from_config();

  // Preselect if it is device or file mode, this finally starts the DAB processor
  const i32 deviceFilePlayerId = Settings::Main::varDeviceFilePlayerId.read().toInt(); // device or file player?

  _slot_service_list_src_change(deviceFilePlayerId);

  QTimer::singleShot(5000, this, &DabRadio::_slot_check_for_update);
}

DabRadio::~DabRadio()
{
  mpAudioManager.reset(); // shut down audio thread before UI is deleted

  delete ui;

  qCInfo(sLogDabRadio) << "RadioInterface is deleted";
}

void DabRadio::_slot_device_selected(const QString & iDeviceName)
{
  assert(!mIsFileMode);
  // _stop_scanning(mChannelDesc);
  mChannelDesc.set_device_name(iDeviceName);
  Settings::Main::varSdrDevice.write(iDeviceName);
  _create_new_input_device_and_dab_processor(iDeviceName);
  const QString ch = Settings::Main::varPresetCh.read().toString();
  const u32 sIdNext = Settings::Main::varPresetCSId.read().toUInt();
  if (!ch.isEmpty() && ch != "0")
  {
    emit signal_FId_or_Ch_selected(ch, sIdNext);
  }
}

// might be called when scanning only from DAB processor and security timer
void DabRadio::_slot_no_dip_sync_found()
{
  mScanSecurityTimer.stop();
  if (mDipSyncState != EDipSyncState::DipNotFound) // avoid repeated calls to the ensemble list
  {
    mDipSyncState = EDipSyncState::DipNotFound;
    _inform_ensemble_list(mChannelDesc.get_ident_info(), EInfoReason::NoNullSymbDet);
  }
}

void DabRadio::_slot_dip_sync_found()
{
  mDipSyncState = EDipSyncState::DipFound;
}

// triggers when the security timer timed-out to ensure a stable behavior
void DabRadio::_slot_scanning_security_timeout()
{
  qCWarning(sLogDabRadio).noquote() << "Scanning security timeout triggered for" << mChannelDesc.get_type_info() << " (too weak signal received)";
  _inform_ensemble_list(mChannelDesc.get_ident_info(), (mDipSyncState == EDipSyncState::DipFound ? EInfoReason::WeakSignalDet : EInfoReason::NoNullSymbDet));
}

// sendDatagram is triggered by the ip handler,
void DabRadio::slot_send_datagram(i32 iLength)
{
  auto * const localBuffer = make_vla(u8, iLength);

  if (mpDataBuffer->get_ring_buffer_read_available() < iLength)
  {
    qWarning() << "slot_send_datagram: not enough data in ring buffer";
    return;
  }

  mpDataBuffer->get_data_from_ring_buffer(localBuffer, iLength);
}

//
// tdcData is triggered by the backend.
void DabRadio::slot_handle_tdc_data(i32 iFrameType, i32 iLength)
{
  qCDebug(sLogDabRadio) << Q_FUNC_INFO << iFrameType << iLength;
#ifdef DATA_STREAMER
  auto * const localBuffer = make_vla(u8, iLength + 8);
#endif
  (void)iFrameType;
  if (!mIsChannelRunning)
  {
    return;
  }
  if (mpDataBuffer->get_ring_buffer_read_available() < iLength)
  {
    qWarning() << "slot_handle_tdc_data: not enough data in ring buffer";
    return;
  }

  // dump data to screen
  // mpDataBuffer->get_data_from_ring_buffer(localBuffer, iLength);
  // qCDebug(sLogRadioInterface) << "tdcData: " << QString::fromWCharArray(reinterpret_cast<wchar_t *>(localBuffer), iLength);

#ifdef DATA_STREAMER
  localBuffer[0] = 0xFF;
  localBuffer[1] = 0x00;
  localBuffer[2] = 0xFF;
  localBuffer[3] = 0x00;
  localBuffer[4] = (iLength & 0xFF) >> 8;
  localBuffer[5] = iLength & 0xFF;
  localBuffer[6] = 0x00;
  localBuffer[7] = iFrameType == 0 ? 0 : 0xFF;
  if (mIsChannelRunning)
  {
    dataStreamer->sendData(localBuffer, iLength + 8);
  }
#endif
}

/**
  * If a change is detected, we have to restart the selected
  * service - if any. If the service is a secondary service,
  * it might be the case that we have to start the main service
  * how do we find that?
  *
  * Response to a signal, so we presume that the signaling body exists
  * signal may be pending though
  */
void DabRadio::slot_change_in_configuration()
{
  qCWarning(sLogDabRadio) << "Configuration change is not supported yet -> Consider providing sample data where such a configuration change happens";
}

void DabRadio::_slot_terminate_process()
{
  if (mIsScanning)
  {
    _stop_scanning(mChannelDesc);
  }

  slot_http_terminate();

  mIsChannelRunning.store(false);
  _adapt_gui_for_device_or_file_play(false);
  mpTiiManager->hide_tii_display();

  Settings::Main::posAndSize.write_widget_geometry(this);

  mpConfig->save_position_and_config();

#ifdef  DATA_STREAMER
  fprintf (stdout, "going to close the dataStreamer\n");
  delete        dataStreamer;
#endif

  if (mpHttpHandler != nullptr)
  {
    mpHttpHandler->stop();
  }

  mDisplayTimer.stop();
  mEnsListRetriggerTimer.stop();
  mScanSecurityTimer.stop();
  mEpgTimer.stop();

  mpAudioManager->stop_audio_output();

  mpEpgMotHandler->close_dl_text_file();

  if (mpDabProcessor != nullptr)
  {
    mpDabProcessor->stop();
  }

  mpTechDataWidget->hide();

  if (mpFibContentTable != nullptr)
  {
    // mpContentTable->clearTable();
    mpFibContentTable->hide();
    mpFibContentTable.reset();
  }

  // mBandHandler.saveSettings();
  // mBandHandler.hide();

  mpAudioManager->stop_all_dumping();
  _stop_source_dumping(mDumpStatus);

  mpConfig->hide();
  usleep(1000);    // pending signals

  // everything should be halted by now
  mpSpectrumViewer->hide();
  mpCirViewer->hide();

  mpDabProcessor.reset();
  mpInputDevice.reset();

  qInfo() << "Terminated DAB radio";
}

void DabRadio::_slot_handle_device_widget_button() const
{
  if (mpInputDevice == nullptr)
  {
    return;
  }

  mpInputDevice->setVisible(mpInputDevice->isHidden());
  Settings::Main::varDeviceUiVisible.write(!mpInputDevice->isHidden());
}


////////////////////////////////////////////////////////////////////////////

void DabRadio::slot_set_stream_selector(i32 iIndex)
{
  if (!mIsChannelRunning)
  {
    return;
  }

  mpAudioManager->slot_set_audio_device(mpConfig->cmbSoundOutput->itemData(iIndex).toByteArray());
}

void DabRadio::_slot_handle_tech_detail_button()
{
  if (!mIsChannelRunning)
  {
    return;
  }

  mpTechDataWidget->setVisible(mpTechDataWidget->isHidden());
  Settings::TechDataViewer::varUiVisible.write(!mpTechDataWidget->isHidden());
}

void DabRadio::_slot_handle_cir_button()
{
  if (!mIsChannelRunning)
    return;

  const bool willShow = mpCirViewer->is_hidden();
  if (mpDabProcessor) mpDabProcessor->activate_cir_viewer(willShow);
  mpCirViewer->setVisible(willShow);
  Settings::CirViewer::varUiVisible.write(willShow);
}

void DabRadio::_slot_handle_open_pic_folder_button() const
{
  const QString picFolder = mpEpgMotHandler->get_pic_folder();

  if (QDir(picFolder).exists())
  {
    QDesktopServices::openUrl(QUrl::fromLocalFile(picFolder));
  }
}

void DabRadio::_slot_handle_reset_button()
{
  if (!mIsChannelRunning)
  {
    return;
  }

  _stop_scanning(mChannelDesc);
  _stop_channel();
  mChannelDesc.clean_channel();
  _start_channel(mChannelDesc.get_fId_or_ch(), mChannelDesc.get_sId_next());
}

void DabRadio::_slot_handle_spectrum_button()
{
  mpSpectrumViewer->setVisible(mpSpectrumViewer->is_hidden());
  Settings::SpectrumViewer::varUiVisible.write(!mpSpectrumViewer->is_hidden());
}

// When changing (or setting) a device, we do not want anybody to have the buttons on the GUI touched, so we just disconnect them and (re)connect them as soon as a device is operational
void DabRadio::_connect_dab_processor_signals()
{
  if (!mSignalSlotConn.empty()) // avoid registering twice
  {
    return;
  }

  mSignalSlotConn =
  {
    // DAB processor related connections
    connect(mpDabProcessor->get_fib_decoder(), &IFibDecoder::signal_fib_loaded_state, this, &DabRadio::_slot_fib_loaded_state, Qt::QueuedConnection),
    connect(mpDabProcessor.get(), &DabProcessor::signal_no_dip_sync_found, this, &DabRadio::_slot_no_dip_sync_found),
    connect(mpDabProcessor.get(), &DabProcessor::signal_dip_sync_found, this, &DabRadio::_slot_dip_sync_found),
    connect(mpSpectrumViewer.get(), &SpectrumViewer::signal_cb_nom_carrier_changed, mpDabProcessor.get(), &DabProcessor::slot_show_nominal_carrier),
    connect(mpSpectrumViewer.get(), &SpectrumViewer::signal_cmb_carrier_changed, mpDabProcessor.get(), &DabProcessor::slot_select_carrier_plot_type),
    connect(mpSpectrumViewer.get(), &SpectrumViewer::signal_cmb_iq_scope_changed, mpDabProcessor.get(), &DabProcessor::slot_select_iq_plot_type),
    connect(mpConfig->cmbSoftBitGen, qOverload<i32>(&QComboBox::currentIndexChanged), mpDabProcessor.get(), [this](i32 idx) { mpDabProcessor->slot_soft_bit_gen_type((ESoftBitType)idx); })
  };
}

void DabRadio::_disconnect_dab_processor_signals()
{
  for (const auto & gc : mSignalSlotConn)
  {
    disconnect(gc);
  }
  mSignalSlotConn.clear();
}

void DabRadio::closeEvent(QCloseEvent * event)
{
  if (Settings::Config::cbCloseDirect.read().toBool())
  {
    _slot_terminate_process();
    event->accept();
    return;
  }

  const QMessageBox::StandardButton resultButton = QMessageBox::question(this, PRJ_NAME, "Quitting " PRJ_NAME "\n\nAre you really sure?\n\n(You can switch-off this message in the configuration settings)",
                                                                         QMessageBox::No | QMessageBox::Yes, QMessageBox::Yes);
  if (resultButton != QMessageBox::Yes)
  {
    event->ignore();
  }
  else
  {
    _slot_terminate_process();
    event->accept();
  }
}

void DabRadio::slot_start_announcement(const QString & iName, i32 iSubChId)
{
  if (!mIsChannelRunning)
  {
    return;
  }

  if (!iName.isEmpty() && iName == ui->lblServiceName->text())
  {
    _set_status_info_status(mStatusInfo.Announce, true);
    qCInfo(sLogDabRadio) << "Announcement starts for service " << ui->lblServiceName->text() << "SubChannel " << iSubChId;
  }
}

void DabRadio::slot_stop_announcement(const QString & iName, i32 iSubChId)
{
  if (!mIsChannelRunning)
  {
    return;
  }

  if (!iName.isEmpty() && iName == ui->lblServiceName->text())
  {
    _set_status_info_status(mStatusInfo.Announce, false);
    qCInfo(sLogDabRadio) << "Announcement stops for service " << ui->lblServiceName->text() << "SubChannel " << iSubChId;
  }
}

// Called from the service list handler when when the favorite state has changed
void DabRadio::_slot_favorite_changed(const bool iIsFav)
{
  mCurFavoriteState = iIsFav;
  _set_favorite_button_style();
}

// Called from the service list handler when channel and service has changed
void DabRadio::_slot_service_changed(const QString & iFIdOrCh, const QString & /*iService*/, const u32 iSId)
{
  if (!mIsScanning)
  {
    emit signal_FId_or_Ch_selected(iFIdOrCh, iSId);
  }
}

// Called from a FIB content list
void DabRadio::slot_handle_fib_content_selector(const u32 iSId)
{
  if (!mIsScanning)
  {
    emit signal_FId_or_Ch_selected(mChannelDesc.get_fId_or_ch(), iSId);
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

  _show_pause_slide();
  _clean_screen(mStatusInfo);
}

void DabRadio::_display_service_label(const QString & iServiceLabel) const
{
  QFont font = ui->lblServiceName->font();
  font.setPointSize(16);
  font.setBold(true);
  ui->lblServiceName->setStyleSheet("QLabel { color: lightblue; }");
  ui->lblServiceName->setFont(font);
  ui->lblServiceName->setText(iServiceLabel);
}

void DabRadio::_write_SId_to_settings(const u32 iSId) const
{
  if (mIsFileMode)
  {
    Settings::Main::varPresetFSId.write(iSId);
  }
  else
  {
    Settings::Main::varPresetCSId.write(iSId);
  }
}

void DabRadio::_write_FId_or_Ch_to_settings(const QString & iFIdOrCh) const
{
  if (mIsFileMode)
  {
    Settings::Main::varPresetFId.write(iFIdOrCh);
  }
  else
  {
    Settings::Main::varPresetCh.write(iFIdOrCh);
  }
}

bool DabRadio::_start_primary_and_secondary_service(const u32 iSId, const bool iStartPrimaryAudioOnly)
{
  /*
   * This method can be called twice after all services are terminated and a new service is selected:
   * For a faster audio startup where only FIG 0/1 and FIG 0/2 are needed, iStartPrimaryAudioOnly == true.
   * In this case the packet data are not retrieved. This happens afterward with a second call to this method
   * with iStartPrimaryAudioOnly == false where all necessary FIB data are given.
   * Calling this with iStartPrimaryAudioOnly == true is not mandatory.
   */

  if (iSId == 0)
  {
    _write_warning_message("Choose a service from the service list");
    return false;
  }

  assert(mIsScanning == false); // while scan, no service handler should be started

  ui->lblDynLabel->setText("");

  // First try to setup a audio channel with this SId
  // TODO: Maybe we can skip this because SIds for primary packet data are always bigger than 0xFFFF?
  SAudioData ad;
  mpDabProcessor->get_fib_decoder()->get_data_for_audio_service(iSId, ad);

  if (ad.isDefined) // belongs SId to a audio service?
  {
    if (mCurPrimaryAudioService.SId == iSId)
    {
      if (iStartPrimaryAudioOnly)
      {
        qWarning(sLogDabRadio) << "Primary audio service" << Qt::hex << Qt::showbase << iSId << "already started";
      }
    }
    else if (!_create_primary_backend_audio_service(ad) && iStartPrimaryAudioOnly)
    {
      qCCritical(sLogDabRadio) << "Could not create primary audio service";
      return false;
    }

    mCurPrimaryAudioService.SId = iSId;
    mCurPrimaryAudioService.SubChId = ad.SubChId;
    mCurPrimaryAudioService.serviceLabel = ad.serviceLabel; // may not be initialized (empty) with iStartPrimaryAudioOnly == true
    mpAudioManager->set_service_label(ad.serviceLabel);
    mpEpgMotHandler->set_service_label(ad.serviceLabel);

    mpTechDataWidget->show_service_data(&ad); // may not show all data with iStartPrimaryAudioOnly == true but that is ok

    _set_status_info_status(mStatusInfo.InpBitRate, (i32)ad.bitRate); // the (i32) is important for the template deduction
    _set_status_info_status(mStatusInfo.ProtLevel, getProtectionLevel(ad.shortForm, ad.protLevel));

    // TODO: how handle EPG?
    // if (mpDabProcessor->get_fib_decoder()->has_time_table(ad.SId))
    // {
    //   mpTechDataWidget->slot_show_timetableButton(true);
    // }

    // Now try to find and create the secondary service(s)
    if (!iStartPrimaryAudioOnly) // only if FIB data complete loaded
    {
      // Store primary channel to settings file
      _write_SId_to_settings(mCurPrimaryAudioService.SId);

      std::vector<SPacketData> pdVec;
      mpDabProcessor->get_fib_decoder()->get_data_for_packet_service(iSId, pdVec);

      if (!pdVec.empty())
      {
        // TODO: considering further data packages?
        if (pdVec.size() > 1)
        {
          qCWarning(sLogDabRadio) << "More than one data packet for service, but ignored (1)";
        }

        const SPacketData & pd = pdVec[0];

        SDabService ds;
        ds.SId = pd.SId;
        ds.SubChId = pd.SubChId;
        ds.SCIdS = pd.SCIdS;

        if (!_create_secondary_backend_packet_service(pd))
        {
          qCCritical(sLogDabRadio) << "Could not create a secondary packet service";
          return false;
        }

        mCurSecondaryServiceVec.emplace_back(ds);
      }
    }
  }
  // Alternative to audio it could be a data service, but only with full-loaded FIB data
  else if (!iStartPrimaryAudioOnly)
  {
    std::vector<SPacketData> pdVec;
    mpDabProcessor->get_fib_decoder()->get_data_for_packet_service(iSId, pdVec);

    if (!pdVec.empty())
    {
      if (mCurPrimaryPacketService.SId == iSId)
      {
        qWarning(sLogDabRadio) << "Primary packet service" << iSId << "already started";
        return false;
      }

      const SPacketData & pd = pdVec[0];
      mCurPrimaryPacketService.SId = pd.SId;
      mCurPrimaryPacketService.SubChId = pd.SubChId;
      mCurPrimaryPacketService.serviceLabel = pd.serviceLabel;

      // TODO: considering further data packages?
      if (pdVec.size() > 1)
      {
        qCWarning(sLogDabRadio) << "More than one data packet for service, but ignored (2)";
      }

      if (!_create_primary_backend_packet_service(pd))
      {
        qCCritical(sLogDabRadio) << "Could not create primary packet service";
        return false;
      }
    }
    else
    {
      qWarning(sLogDabRadio) << "No packet data for service" << iSId;
    }
  }

  _display_service_label(mCurPrimaryAudioService.serviceLabel);
  _update_audio_data_addon(mCurPrimaryAudioService.SId);
  return true;
}

bool DabRadio::_create_primary_backend_audio_service(const SAudioData & iAD)
{
  if (!iAD.isDefined || iAD.bitRate == 0)
  {
    _write_warning_message("Insufficient data for this program (2)");
    return false;
  }

  if (!mpDabProcessor->is_service_running(iAD, EProcessFlag::Primary))
  {
    mpDabProcessor->set_audio_channel(iAD, mpAudioManager->get_audio_buffer_from_decoder(), EProcessFlag::Primary);
    mpAudioManager->reset_audio_fifo();
    mpAudioManager->set_audio_frame_type(iAD.ASCTy == 0x3F ? AudioManager::EAudioFrameType::AAC : AudioManager::EAudioFrameType::MP2);
  }
  else
  {
    qCritical() << "Primary audio service already started";
  }

  return true;
}

bool DabRadio::_create_secondary_backend_packet_service(const SPacketData & iPD) const
{
  if (!mpDabProcessor->is_service_running(iPD, EProcessFlag::Secondary))
  {
    mpDabProcessor->set_data_channel(iPD, mpDataBuffer, EProcessFlag::Secondary);
    qInfo().nospace() << "Adding " << iPD.serviceLabel.trimmed() << " (SubChId " << iPD.SubChId << ") as sub-service";
  }
  else
  {
    qDebug() << "Secondary packet service already started";
  }

  return true;
}

bool DabRadio::_create_primary_backend_packet_service(const SPacketData & iPD) const
{
  if (!iPD.isDefined || iPD.DSCTy == 0 || iPD.bitRate == 0)
  {
    _write_warning_message("Insufficient data for this program (3)");
    return false;
  }

  if (!mpDabProcessor->is_service_running(iPD, EProcessFlag::Primary))
  {
    mpDabProcessor->set_data_channel(iPD, mpDataBuffer, EProcessFlag::Primary);

    // Unfortunately, we can not support primary data services yet, but the work will go on...
    switch (iPD.DSCTy)
    {
    case 5:
      qCDebug(sLogDabRadio) << "Selected AppType" << iPD.appTypeVec[0];
      _write_warning_message("Primary 'Transparent Data Channel' (TCD) not supported");
      break;
    case 24:
      _write_warning_message("Primary 'MPEG-2 Transport Stream' not supported");
      break;
    case 60:
      _write_warning_message("Primary 'Multimedia Object Transfer' (MOT) not supported");
      break;
    case 44:
      _write_warning_message("Primary Journaline");
      break;
    default:
      _write_warning_message("Primary unimplemented protocol");
      break;
    }
  }
  else
  {
    qDebug() << "Primary packet service already started";
  }

  return true;
}

void DabRadio::_update_audio_data_addon(const u32 iSId) const
{
  SAudioDataAddOns adao;
  mpDabProcessor->get_fib_decoder()->get_data_for_audio_service_addon(iSId, adao);
  mpTechDataWidget->show_service_data_addon(&adao);
  ui->lblProgType->setText(adao.programType.has_value() ? getProgramType(adao.programType.value()) : "(no info received)");
}

void DabRadio::_update_scan_statistics(const QString & iFIdOrCh, const SServiceId & sl)
{
  if (sl.isAudioChannel)
  {
    mScanResult.nrAudioServices++;
    if (mScanResult.lastFIdOrCh != iFIdOrCh)
    {
      mScanResult.lastFIdOrCh = iFIdOrCh;
      mScanResult.nrChannels++;
    }
  }
  else
  {
    mScanResult.nrNonAudioServices++;
  }
}

void DabRadio::_slot_fib_loaded_state(IFibDecoder::EFibLoadingState iFibLoadingState)
{
  qDebug() << "FIB data loading state" << (i32)iFibLoadingState;

  assert(iFibLoadingState >= IFibDecoder::EFibLoadingState::S1_FastAudioDataLoaded &&
         iFibLoadingState <= IFibDecoder::EFibLoadingState::S5_DeferredDataLoaded);

  if (!mIsChannelRunning)
  {
    return;
  }

  if (!mChannelDesc.ecc_checked)
  {
    mChannelDesc.ecc_byte = mpDabProcessor->get_fib_decoder()->get_ecc();
    if (mChannelDesc.ecc_byte != 0)
    {
      mChannelDesc.ecc_checked = true;
      const u8 countryId = (mChannelDesc.Eid >> 12) & 0xF;
      const auto & itu = mpItuTables->find_ITU_entry(mChannelDesc.ecc_byte, countryId);
      // mChannelDesc.deferredData.countryName = QString("%1(%2)").arg(itu.Country).arg(itu.ITU_Code);
      // mChannelDesc.deferredData.countryName = QString("%1(%2)").arg(itu.ITU_Code).arg(itu.Country);
      mChannelDesc.deferredData.countryName = QString("%1(%2/%3)").arg(itu.ITU_Code).arg(mChannelDesc.ecc_byte, 2, 16, QChar('0')).arg(countryId, 1, 16, QChar('0'));
      ui->lblCountryName->setText(itu.Country);
      // qDebug() << "Ch/FId" << mChannelDesc.get_fId_or_ch() << Qt::hex << Qt::showbase << "with EId" << mChannelDesc.Eid <<  "has ECC byte"  << mChannelDesc.ecc_byte << "with country name" << mChannelDesc.deferredData.countryName.value();
    }
  }

  if (iFibLoadingState == IFibDecoder::EFibLoadingState::S5_DeferredDataLoaded)
  {
    qDebug() << "FIB data in state 5 are ignored";
    return;
  }

  const bool fibDataNeeded = mChannelDesc.check_ensemble_list_scan_level_should_be_achieved(EScanLevel::SL2_FibData); // true: FIB data until S4_FullyPacketDataLoaded needed

  // while scanning or ensemble list data gathering, we wait for the full packets
  if ((mIsScanning || fibDataNeeded) && iFibLoadingState < IFibDecoder::EFibLoadingState::S4_FullyPacketDataLoaded)
  {
    return;
  }

  mScanSecurityTimer.stop();

  if (iFibLoadingState == IFibDecoder::EFibLoadingState::S1_FastAudioDataLoaded)
  {
    if (mChannelDesc.get_sId_next() == 0)
    {
      qCCritical(sLogDabRadio) << "Fast audio select triggered, but no SId_next set";
      return;
    }

    qDebug() << "Fast audio select triggered -> necessary data for audio start available (except service label)";

    if (_start_primary_and_secondary_service(mChannelDesc.get_sId_next(), true))
    {
      mpServiceListHandler->set_selector(mChannelDesc.get_fId_or_ch(), mChannelDesc.get_sId_next());
    }
    return;
  }

  // mServiceList is mainly filled for audio in state S3_FullyAudioDataLoaded but for primary data services, also S4_FullyPacketDataLoaded should be evaluated
  mServiceList = mpDabProcessor->get_fib_decoder()->get_service_list();
  mpEpgMotHandler->set_service_list(&mServiceList);

  // In the service list we only collected audio services. But while scanning we run here also for packet data to update the scan statistics
  if (mIsScanning || fibDataNeeded || iFibLoadingState == IFibDecoder::EFibLoadingState::S3_FullyAudioDataLoaded)
  {
    ServiceListHandler::TSIdList sIdList;

    // Fill service list with wanted (via filter) content
    for (const auto & sl : mServiceList)
    {
      if (sl.isAudioChannel) // some kind obsolete but to get sure
      {
        sIdList << sl.SId;
        mpServiceListHandler->add_entry(mChannelDesc.get_fId_or_ch(), sl.serviceLabel, sl.SId);
      }

      if (mIsScanning)
      {
        _update_scan_statistics(mChannelDesc.get_fId_or_ch(), sl); // collect statistics while scanning for audio and packet data
      }
    }

    // Crosscheck service list if a not more existing service is stored there (while scan the list was already cleaned up before)
    if (!mIsScanning)
    {
      mpServiceListHandler->delete_not_existing_SId_at_channel(mChannelDesc.get_fId_or_ch(), sIdList);
    }
  }

  if (iFibLoadingState >= IFibDecoder::EFibLoadingState::S4_FullyPacketDataLoaded)
  {
    if (!mIsScanning) // ioChannelDesc.SId_next is invalid while scanning
    {
      // u32 sId = mChannelDesc.get_sId_next();
      //
      // // If no SID is given, look in the service list for the first audio service
      // if (sId == 0)
      // {
      //   for (const auto & sl : mServiceList)
      //   {
      //     if (sl.isAudioChannel && sl.SId > 0)
      //     {
      //       // mChannelDesc.set_sId_next(sl.SId);
      //       sId = sl.SId;
      //       qDebug() << "No SId given -> choose first audio service for start:" << sId;
      //       break;
      //     }
      //   }
      // }

      if (mChannelDesc.get_sId_next() > 0)
      {
        if (_start_primary_and_secondary_service(mChannelDesc.get_sId_next(), false)) // TODO: audio service could already run here
        {
          mpServiceListHandler->set_selector(mChannelDesc.get_fId_or_ch(), mChannelDesc.get_sId_next());
        }

        // mChannelDesc.set_sId_next(0); // used, so make it invalid
      }

      // _inform_ensemble_list(mChannelDesc.identInfoEL, mChannelDesc.get_fId_or_ch(), EInfoReason::NewFib); // triggers ensemble list update
    }
    else // while scanning
    {
      ui->lblDynLabel->setText(_get_scan_message(false));
    }

    if (mIsScanning || fibDataNeeded)
    {
      mEnsListRetriggerCnt = 0;
      mEnsListRetriggerTimer.start(1); // triggers _slot_ensemble_list_retrigger_timeout()  in the timeout slot
    }
  }
}

void DabRadio::_write_warning_message(const QString & iMsg) const
{
  ui->lblDynLabel->setStyleSheet("color: #ff9100");
  ui->lblDynLabel->setText(iMsg);
}

void DabRadio::_start_scanning(SChannelDescriptor & ioChannelDesc)
{
  mEnsListRetriggerTimer.stop();
  mScanSecurityTimer.stop();
  mEpgTimer.stop();
  _stop_channel();
  mChannelDesc.clean_channel();

  mIsScanning.store(true);
  mpAudioManager->set_scanning(true);

  mScanResult = {};
  ui->lblDynLabel->setText(_get_scan_message(false));

  if (mpDabProcessor != nullptr) mpDabProcessor->set_scan_mode(true); // avoid MSC activities

  ui->rbDevicePlayer->setEnabled(false);
  ui->rbFilePlayer->setEnabled(false);
  ui->cmbDeviceSelect->setEnabled(false);

  mpServiceListHandler->delete_table(false);
  mpServiceListHandler->create_new_table();
  mScanSecurityTimer.start();
}

// _stop_scanning is called
// 1. when the scanbutton is touched during scanning
// 2. on user selection of a service or a channel select
// 3. on device selection
// 4. on handling a reset
void DabRadio::_stop_scanning(SChannelDescriptor & ioChannelDesc)
{
  if (mIsScanning)
  {
    if (mpDabProcessor != nullptr) mpDabProcessor->set_scan_mode(false);
    ui->lblDynLabel->setText(_get_scan_message(true));
    mEnsListRetriggerTimer.stop();
    mScanSecurityTimer.stop();
    ui->rbDevicePlayer->setEnabled(true);
    ui->rbFilePlayer->setEnabled(true);
    ui->cmbDeviceSelect->setEnabled(!mIsFileMode);
    _enable_ui_elements_for_safety(true);
    // ioChannelDesc.set_device_channel(""); // trigger restart of channel after next service list click (TODO: ??)
    mIsScanning.store(false);
    mpAudioManager->set_scanning(false);
    mpServiceListHandler->restore_favorites(); // try to restore former favorites from a backup table
  }
}

void DabRadio::slot_epg_timer_timeout()
{
  mEpgTimer.stop();

  // TODO: there is a public switch for this missing?!
  // if (!mpSH->read(SettingHelper::epgFlag).toBool())
  {
    return;
  }

  if (mIsScanning)
  {
    return;
  }

  // TODO: make this EPG true!
  for (const auto & serv: mServiceList)
  {
    if (serv.hasSpiEpgData)
    {
      std::vector<SPacketData> pdVec;
      mpDabProcessor->get_fib_decoder()->get_data_for_packet_service(serv.SId, pdVec);

      if (pdVec.empty())
      {
        continue;
      }

      const SPacketData & pd = pdVec[0];

      if ((!pd.isDefined) || (pd.DSCTy == 0) || (pd.bitRate == 0))
      {
        continue;
      }

      if (pd.DSCTy == 60)
      {
        _show_epg_label(true);
        qCDebug(sLogDabRadio) << "Starting hidden service " << serv.serviceLabel;
        mpDabProcessor->set_data_channel(pd, mpDataBuffer, EProcessFlag::Primary);  // which ProcessFlag?
        SDabService s;
        // s.channel = pd.channel;
        // s.serviceLabel = pd.serviceName;
        s.SId = pd.SId;
        s.SCIdS = pd.SCIdS;
        s.SubChId = pd.SubChId;
        // s.fd = nullptr;
        mCurSecondaryServiceVec.emplace_back(s);
      }
    }
#ifdef  __DABDATA__
    else
    {
      packetdata pd;
      my_dabProcessor->dataforPacketService(serv.name, &pd, 0);
      if ((pd.defined) && (pd.DSCTy == 59))
      {
        _show_epg_label(true);
        fprintf(stdout, "Starting hidden service %s\n", serv.name.toUtf8().data());
        my_dabProcessor->set_dataChannel(&pd, &dataBuffer, BACK_GROUND);
        dabService s;
        s.channel = channel.channelName;
        s.serviceName = pd.serviceName;
        s.SId = pd.SId;
        s.SCIdS = pd.SCIdS;
        s.subChId = pd.subchId;
        s.fd = nullptr;
        channel.backgroundServices.push_back(s);
        break;
      }
    }
#endif
  }
}

//------------------------------------------------------------------------
//------------------------------------------------------------------------

void DabRadio::_slot_handle_time_table()
{
  i32 epgWidth = 70; // comes only from ini file formerly

  if (mCurPrimaryPacketService.SId == 0)
  {
    return;
  }

  mpTimeTable->clear();

  if (epgWidth < 50)
  {
    epgWidth = 50;
  }
  // TODO: check EPG
  // std::vector<SEpgElement> res = mpDabProcessor->get_fib_decoder()->find_epg_data(mChannelDesc.currentService.SId);
  // for (const auto & element: res)
  // {
  //   mpTimeTable->addElement(element.theTime, epgWidth, element.theText, element.theDescr);
  // }
}


void DabRadio::slot_handle_dc_avoidance_algorithm(bool iIsChecked)
{
  assert(mpDabProcessor != nullptr);
  mpDabProcessor->set_dc_avoidance_algorithm(iIsChecked);
}

void DabRadio::slot_handle_dc_and_iq_corr(const bool iDcCorr, const bool iIqCorr)
{
  assert(mpDabProcessor != nullptr);
  mpDabProcessor->set_dc_and_iq_correction(iDcCorr, iIqCorr);
}


void DabRadio::slot_load_table()
{
  mpTiiManager->load_table();  // emits signal_tii_file_status which updates mChannelDesc.tiiFile
}

// ensure that we only get a handler if we have a start location
void DabRadio::_slot_handle_http_button()
{
  if (std::real(mLocalPos) == 0 || std::imag(mLocalPos) == 0)
  {
    QMessageBox::information(this, "Data missing", "Provide your coordinates first on the configuration window!");
    return;
  }

  if (mpHttpHandler == nullptr)
  {
    const QString browserAddress = Settings::Config::varBrowserAddress.read().toString();
    const QString mapPort = Settings::Config::varMapPort.read().toString();

    QString mapFile;
    if (Settings::Config::cbSaveTransToCsv.read().toBool())
    {
      mapFile = mOpenFileDialog.get_maps_file_name();
    }
    else
    {
      mapFile = "";
    }
    mpHttpHandler.reset(new MapHttpServer(this, mapPort, browserAddress, mLocalPos, mapFile, Settings::Config::cbManualBrowserStart.read().toBool()));
    mMaxDistance = -1;

    if (mpHttpHandler != nullptr)
    {
      mpTiiManager->set_http_handler(mpHttpHandler.get());
      _set_http_server_button(EHttpButtonState::On);
    }
  }
  else
  {
    _set_http_server_button(EHttpButtonState::Waiting); // closing browser could be taken some seconds
    mpHttpHandler->add_transmitter_location_entry(MAP_CLOSE, nullptr, "", 0, 0, 0, false);
  }
}

void DabRadio::slot_http_terminate()
{
  mpTiiManager->set_http_handler(nullptr);
  mpHttpHandler.reset();
  _set_http_server_button(EHttpButtonState::Off);
}

void DabRadio::_show_pause_slide() const
{
  mpEpgMotHandler->show_pause_slide();
}

QString DabRadio::_convert_links_to_clickable(const QString & iText) const
{
  // qDebug() << "iText: " << iText << iText.length();

  if (!mpConfig->cbUrlClickable->isChecked())
  {
    return iText;
  }

  // Allow uppercase also in top-level-domain as some texts are overall in uppercase letters.
  // Allow also umlauts as part of the URL, maybe there are more special letters valid for an URL outside...
  static const QRegularExpression regex2(R"([\w\-äöüÄÖÜ]{2,}\.[a-zA-Z]{2}[a-zA-Z0-9/\.]{0,}$)");
  static const QRegularExpression regex1("\\s+"); // match any whitespace

  QStringList wordList = iText.split(regex1, Qt::SkipEmptyParts);
  QString result = iText;

  for (const QString & word : wordList)
  {
    // qDebug() << "word: " << word << word.length();

    if (const QRegularExpressionMatch match = regex2.match(word);
        match.hasMatch())
    {
      QString linkStr;

      if (word.indexOf("@", 1) > 0)
      {
        linkStr = "mailto:" + word;
      }
      else if (!word.startsWith("http"))
      {
        linkStr = "https://" + word; // could be only http: but almost should by quite seldom nowadays
      }
      else
      {
        linkStr = word;
      }
      const QString repl = QString("<a href=\"%1\" style=\"color: #7983FF;\">%2</a>").arg(linkStr, word);

      // qDebug() << "repl: " << repl << repl.length();

      // Replacing has a drawback: if repl is found more than once then the link could get corrupted.
      // Possible solution: remove more than one same "word" finding and sort the rest with the length (top to down).
      // This would be a very seldom case, so we ignore them.
      // Assemble back each word to the entire string has also the disadvantage that several separators can got lost.
      result.replace(word, repl);
    }
  }

  // qDebug() << "result:" << result << result.length();
  return result;
}

void DabRadio::_get_local_position_from_config(cf32 & oLocalPos) const
{
  const f32 local_lat = Settings::Config::varLatitude.read().toFloat();
  const f32 local_lon = Settings::Config::varLongitude.read().toFloat();
  oLocalPos = cf32(local_lat, local_lon);
}

void DabRadio::_initialize_audio_output()
{
  AudioManager::SResourceConfig cfg;
  cfg.pAudioBufferFromDecoder = mpAudioBufferFromDecoder;
  cfg.pAudioBufferToOutput    = mpAudioBufferToOutput;
  cfg.pFrameBuffer            = mpFrameBuffer;
  cfg.pConfig                 = mpConfig.get();
  cfg.pTechDataWidget         = mpTechDataWidget.get();
  cfg.pProgBarAudioBuffer     = ui->progBarAudioBuffer;
  cfg.pThermoPeakLevelLeft    = ui->thermoPeakLevelLeft;
  cfg.pThermoPeakLevelRight   = ui->thermoPeakLevelRight;
  cfg.pSliderVolume           = ui->sliderVolume;
  cfg.pOpenFileDialog         = &mOpenFileDialog;

  mpAudioManager.reset(new AudioManager(cfg, this));

  connect(mpAudioManager.get(), &AudioManager::signal_sbr_used,           this, [this](bool v){ _set_status_info_status(mStatusInfo.SBR, v); });
  connect(mpAudioManager.get(), &AudioManager::signal_ps_used,            this, [this](bool v){ _set_status_info_status(mStatusInfo.PS, v); });
  connect(mpAudioManager.get(), &AudioManager::signal_output_sample_rate, this, [this](u32 v){ _set_status_info_status(mStatusInfo.OutSampRate, v); });
  connect(mpAudioManager.get(), &AudioManager::signal_unmute_requested,   this, [this](){ _slot_update_mute_state(false); });
}

void DabRadio::slot_new_audio(const i32 iAmount, const u32 iAudioSampleRate, const u32 iAudioFlags)
{
  mpAudioManager->new_audio(iAmount, iAudioSampleRate, iAudioFlags);
}

void DabRadio::slot_new_aac_mp2_frame()
{
  mpAudioManager->new_aac_mp2_frame();
}

// MOT wrappers — backend classes (MotObject, PadHandler) reference DabRadio* directly
void DabRadio::slot_handle_mot_object(const QByteArray & iResult, const QString & iObjectName, const i32 iContentType, const bool iDirElement) const
{
  // qDebug() << "MOT progress: " << iResult.length() << iObjectName << iContentType << iDirElement;
  mpEpgMotHandler->handle_mot_object(iResult, iObjectName, iContentType, iDirElement);
}

void DabRadio::slot_trigger_mot_indicator() const
{
  mpEpgMotHandler->trigger_mot_indicator();
  static i32 motCntStart = 0;
  static i32 motCntEnd = 20;
  // qDebug() << "MOT progress: " << motCntStart << motCntEnd;
  // _set_MOT_progress(motCntStart, motCntEnd);
  motCntStart = (motCntStart + 3) % 100;
  motCntEnd = (motCntEnd + 3) % 100;
}

void DabRadio::_initialize_epg_mot_handler()
{
  EpgMotHandler::SResourceConfig cfg;
  cfg.pConfig           = mpConfig.get();
  cfg.pPictureLabel     = ui->pictureLabel;
  cfg.pBtnOpenPicFolder = ui->btnOpenPicFolder;
  cfg.pOpenFileDialog   = &mOpenFileDialog;
  mpEpgMotHandler.reset(new EpgMotHandler(cfg, this));

  mpEpgMotHandler->slot_init_paths();

  connect(mpEpgMotHandler.get(), &EpgMotHandler::signal_mot_indicator, this, [this](bool b) { _set_status_info_status(mStatusInfo.MOT, b); });
  connect(mpEpgMotHandler.get(), &EpgMotHandler::signal_epg_indicator, this, [this](bool b) { _show_epg_label(b); });

  // EPG timer for autostart epg service
  mEpgTimer.setSingleShot(true);
  connect(&mEpgTimer, &QTimer::timeout, this, &DabRadio::slot_epg_timer_timeout);
}

void DabRadio::_initialize_time_table()
{
  mpTimeTable.reset(new TimeTableHandler(this));
  mpTimeTable->hide();
}

void DabRadio::_initialize_tii_manager()
{
  TiiManager::SResourceConfig cfg;
  cfg.pConfig       = mpConfig.get();
  cfg.pCmbTiiList   = ui->cmbTiiList;
  cfg.pLblTii       = ui->lblTii;
  cfg.pParentWidget = this;
  mpTiiManager.reset(new TiiManager(cfg, this));

  const bool ok = mpTiiManager->init_tii_file();
  mChannelDesc.tiiFile = ok;

  connect(mpTiiManager.get(), &TiiManager::signal_tii_file_status, this, [this](bool valid, const QString & toolTip) {
    mChannelDesc.tiiFile = valid;
    ui->btnHttpServer->setEnabled(valid);
    if (!valid)
    {
      ui->btnHttpServer->setToolTip(toolTip);
    }
  });
}

void DabRadio::_initialize_and_start_timers()
{
  // The displaytimer is there to show the number of seconds running and handle - if available - the tii data
  mDisplayTimer.setInterval(cDisplayTimeoutMs);
  connect(&mDisplayTimer, &QTimer::timeout, this, &DabRadio::_slot_update_time_display);
  mDisplayTimer.start(cDisplayTimeoutMs);

  // timer for scanning
  mScanSecurityTimer.setSingleShot(true);
  mScanSecurityTimer.setInterval(cScanningTimeoutMs);
  connect(&mScanSecurityTimer, &QTimer::timeout, this, &DabRadio::_slot_scanning_security_timeout);

  // mEnsembleList.FibTimeoutTimer.setSingleShot(true);
  mEnsListRetriggerTimer.setSingleShot(true);
  connect(&mEnsListRetriggerTimer, &QTimer::timeout, this, &DabRadio::_slot_ensemble_list_retrigger_timeout);

  mClockResetTimer.setSingleShot(true);
  connect(&mClockResetTimer, &QTimer::timeout, [this](){ _set_clock_text(); });
}

void DabRadio::slot_check_for_update()
{
  _check_on_github_for_update(true);
}

void DabRadio::_slot_check_for_update()
{
  if (!Settings::Config::cbCheckForUpdates.read().toBool())
  {
    qCDebug(sLogDabRadio) << "Update check is switched off";
    return;
  }

  const QDateTime lastUpdateCheckDate = Settings::Main::varUpdateCheckTime.read().toDateTime();

  if (lastUpdateCheckDate.isValid()) // if no entry is set in the settings this would be invalid
  {
    const i32 diffDaysToLastCheck = (i32)lastUpdateCheckDate.daysTo(QDateTime::currentDateTime());
    const i32 diffDaysWanted = Settings::Config::sbUpdateCheckDays.read().toInt();

    if (diffDaysToLastCheck < diffDaysWanted)
    {
      qCDebug(sLogDabRadio) << "Checking for update remaining days:" << diffDaysWanted - diffDaysToLastCheck;
      return;
    }
  }

  _check_on_github_for_update(false);
}

void DabRadio::_check_on_github_for_update(const bool iShowMessageBox)
{
  UpdateChecker * updateChecker = new UpdateChecker(this);

  auto update_checker = [this, updateChecker, iShowMessageBox](const bool result)
  {
    if (result)
    {
      const AppVersion verNew(updateChecker->version());
      const AppVersion verCur(PRJ_VERS);

      if (!verCur.isValid())
      {
        qCCritical(sLogDabRadio) << "The current application version assignment is invalid";
        return;
      }

      if (verNew.isValid())
      {
        Settings::Main::varUpdateCheckTime.write(QDateTime::currentDateTime());

        if (verNew > verCur)
        {
          qCInfo(sLogDabRadio, "New application version found: %s", updateChecker->version().toUtf8().data());

          const i32 diffDaysWanted = Settings::Config::sbUpdateCheckDays.read().toInt();
          const auto dialog = new UpdateDialog(updateChecker->version(), updateChecker->releaseNotes(),
                                               Qt::WindowTitleHint | Qt::WindowCloseButtonHint, diffDaysWanted, this);
          connect(dialog, &UpdateDialog::finished, dialog, &QObject::deleteLater);
          dialog->open();
        }
        else if (verNew == verCur)
        {
          const QString text = "This DABstar version " + verCur.toString() + " is up to date";
          qCInfo(sLogDabRadio).noquote() << text;
          if (iShowMessageBox) QMessageBox::information(this, "Update", text);
        }
        else
        {
          const QString text = "The latest version on GitHub is " + verNew.toString() + ", but this version is assigned as " + verCur.toString() + "! -> skip update";
          qCWarning(sLogDabRadio).noquote() << text;
          if (iShowMessageBox) QMessageBox::warning(this, "Update", text);
        }
      }
    }
    else
    {
      const QString text = "Update check failed! Maybe a network issue to Github?";
      qCWarning(sLogDabRadio).noquote() << text;
      if (iShowMessageBox) QMessageBox::warning(this, "Update", text);
    }
    updateChecker->deleteLater();
  };

  connect(updateChecker, &UpdateChecker::finished, this, update_checker);

  updateChecker->check();
}

void DabRadio::_initialize_signal_slot_connections()
{
  // DabRadio UI connections
  assert(ui != nullptr);
  connect(ui->btnFib, &QPushButton::clicked, this, &DabRadio::_slot_handle_content_button);
  connect(ui->btnTechDetails, &QPushButton::clicked, this, &DabRadio::_slot_handle_tech_detail_button);
  connect(ui->btnSpectrumScope, &QPushButton::clicked, this, &DabRadio::_slot_handle_spectrum_button);
  connect(ui->btnDeviceWidget, &QPushButton::clicked, this, &DabRadio::_slot_handle_device_widget_button);
  connect(ui->btnPrevService, &QPushButton::clicked, this, &DabRadio::_slot_handle_prev_service_button);
  connect(ui->btnNextService, &QPushButton::clicked, this, &DabRadio::_slot_handle_next_service_button);
  connect(ui->btnTargetService, &QPushButton::clicked, this, &DabRadio::_slot_handle_target_service_button);
  connect(ui->btnMuteAudio, &QPushButton::clicked, this, &DabRadio::_slot_handle_mute_button);
  connect(ui->btnToggleFavorite, &QPushButton::clicked, this, &DabRadio::_slot_handle_favorite_button);
  connect(ui->btnTii, &QPushButton::clicked, mpTiiManager.get(), &TiiManager::slot_handle_tii_button);
  connect(ui->btnCir, &QPushButton::clicked, this, &DabRadio::_slot_handle_cir_button);
  connect(ui->btnOpenPicFolder, &QPushButton::clicked, this, &DabRadio::_slot_handle_open_pic_folder_button);

  // configuration UI connections
  assert(mpConfig != nullptr);
  connect(mpConfig.get(), &Configuration::signal_handle_dc_and_iq_corr, this, &DabRadio::slot_handle_dc_and_iq_corr);
  connect(mpConfig->cbTestTone, &QPushButton::clicked, mpAudioManager.get(), &AudioManager::slot_set_test_tone);
  connect(mpConfig->sbPeakLevelDelay, &QSpinBox::valueChanged, mpAudioManager.get(), &AudioManager::slot_update_peak_level_delay);
  connect(mpConfig->resetButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_reset_button);
  connect(mpConfig->dumpButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_source_dump_button);
  connect(mpConfig->etiButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_eti_button);
  connect(mpConfig->loadTableButton, &QPushButton::clicked, this, &DabRadio::slot_load_table);
  connect(mpConfig->cmbSoundOutput, qOverload<i32>(&QComboBox::activated), this, &DabRadio::slot_set_stream_selector);
  connect(mpConfig.get(), &Configuration::signal_coordinates_changed, this, [this]() { _get_local_position_from_config(mLocalPos); mpTiiManager->set_local_pos(mLocalPos); });
  connect(mpConfig->cbUseDcAvoidance, &QCheckBox::clicked, this, &DabRadio::slot_handle_dc_avoidance_algorithm);
  connect(mpConfig->sbTiiThreshold, &QSpinBox::valueChanged, mpTiiManager.get(), &TiiManager::slot_handle_tii_threshold);
  connect(mpConfig->cbTiiCollisions, &QCheckBox::clicked, mpTiiManager.get(), &TiiManager::slot_handle_tii_collisions);
  connect(mpConfig->sbTiiSubId, &QSpinBox::valueChanged, mpTiiManager.get(), &TiiManager::slot_handle_tii_subid);
  connect(mpConfig->cmbMotObjectSaving5, qOverload<i32>(&QComboBox::activated), mpEpgMotHandler.get(), &EpgMotHandler::slot_handle_mot_saving_selector);
  connect(mpConfig->dlTextButton, &QPushButton::clicked, mpEpgMotHandler.get(), &EpgMotHandler::slot_handle_dl_text_button);
  connect(mpConfig.get(), &Configuration::signal_data_base_path_changed, mpEpgMotHandler.get(), &EpgMotHandler::slot_init_paths);
  connect(mpConfig->btnCheckForUpdate, &QPushButton::clicked, this, &DabRadio::slot_check_for_update);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(mpConfig->cbUseStrongestPeak, &QCheckBox::checkStateChanged, mpTiiManager.get(), &TiiManager::slot_use_strongest_peak);
#else
  connect(mpConfig->cbUseStrongestPeak, &QCheckBox::stateChanged, mpTiiManager.get(), &TiiManager::slot_use_strongest_peak);
#endif

  // Tech data connections
  assert(mpTechDataWidget != nullptr);
  connect(mpTechDataWidget.get(), &TechData::signal_handle_audioDumping, mpAudioManager.get(), &AudioManager::slot_handle_audio_dump_button);
  connect(mpTechDataWidget.get(), &TechData::signal_handle_frameDumping, mpAudioManager.get(), &AudioManager::slot_handle_frame_dump_button);

  // Service list connections
  assert(mpServiceListHandler != nullptr);
  connect(mpServiceListHandler.get(), &ServiceListHandler::signal_selection_changed, this, &DabRadio::_slot_service_changed);
  connect(mpServiceListHandler.get(), &ServiceListHandler::signal_favorite_status, this, &DabRadio::_slot_favorite_changed);
}

