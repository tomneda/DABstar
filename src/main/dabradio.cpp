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

#include "dab-constants.h"
#include "mot-content-types.h"
#include "dabradio.h"
#include "dab-tables.h"
#include "ITU_Region_1.h"
#include "coordinates.h"
#include "mapport.h"
#include "techdata.h"
#include "service-list-handler.h"
#include "setting-helper.h"
#include "audiooutputqt.h"
#include "compass_direction.h"
#include "copyright_info.h"
#include "time-table.h"
#include "ui_dabradio.h"
#include <cmath>
#include <fstream>
#include <numeric>
#include <vector>
#include <QCoreApplication>
#include <QSettings>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QStringList>
#include <QMouseEvent>
#include <QDir>
#include <QSpacerItem>
#include <QCryptographicHash>
#include <QRegularExpression>

#if defined(__MINGW32__) || defined(_WIN32)
  #include <windows.h>
#else
  #include <unistd.h>
#endif

// Q_LOGGING_CATEGORY(sLogRadioInterface, "RadioInterface", QtDebugMsg)
Q_LOGGING_CATEGORY(sLogRadioInterface, "RadioInterface", QtWarningMsg)

#if defined(__MINGW32__) || defined(_WIN32)

__int64 FileTimeToInt64 (FILETIME & ft)
{
  ULARGE_INTEGER foo;

  foo.LowPart	= ft.dwLowDateTime;
  foo.HighPart	= ft.dwHighDateTime;
  return (foo.QuadPart);
}

bool get_cpu_times (size_t &idle_time, size_t &total_time)
{
  FILETIME IdleTime, KernelTime, UserTime;
  size_t	thisIdle, thisKernel, thisUser;

  GetSystemTimes (&IdleTime, &KernelTime, &UserTime);

  thisIdle	= FileTimeToInt64 (IdleTime);
  thisKernel	= FileTimeToInt64 (KernelTime);
  thisUser	= FileTimeToInt64 (UserTime);
  idle_time	= (size_t) thisIdle;
  total_time	= (size_t)(thisKernel + thisUser);
  return true;
}

#else

std::vector<size_t> get_cpu_times()
{
  std::ifstream proc_stat("/proc/stat");
  proc_stat.ignore(5, ' ');    // Skip the 'cpu' prefix.
  std::vector<size_t> times;
  for (size_t time; proc_stat >> time; times.push_back(time))
  {
  }
  return times;
}

bool get_cpu_times(size_t & idle_time, size_t & total_time)
{
  const std::vector<size_t> cpu_times = get_cpu_times();
  if (cpu_times.size() < 4)
  {
    return false;
  }
  idle_time = cpu_times[3];
  total_time = std::accumulate(cpu_times.begin(), cpu_times.end(), (size_t)0);
  return true;
}

#endif


DabRadio::DabRadio(QSettings * const ipSettings, const QString & iFileNameDb, const QString & iFileNameAltFreqList, const i32 iDataPort, QWidget * iParent)
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
  , mSpectrumViewer(this, ipSettings, mpSpectrumBuffer, mpIqBuffer, mpCarrBuffer, mpResponseBuffer)
  , mCirViewer(mpCirBuffer)
  , mBandHandler(iFileNameAltFreqList, ipSettings)
  , mOpenFileDialog(ipSettings)
  , mConfig(this)
  , mDeviceSelector(ipSettings)
{
  mIsRunning.store(false);
  mIsScanning.store(false);

  // "mProcessParams" is introduced to reduce the number of parameters for the dabProcessor
  mProcessParams.spectrumBuffer = mpSpectrumBuffer;
  mProcessParams.iqBuffer = mpIqBuffer;
  mProcessParams.carrBuffer = mpCarrBuffer;
  mProcessParams.responseBuffer = mpResponseBuffer;
  mProcessParams.frameBuffer = mpFrameBuffer;
  mProcessParams.cirBuffer = mpCirBuffer;
  mProcessParams.threshold = 3.0f;
  mProcessParams.tiiFramesToCount = 5;

  //	set on top or not? checked at start up
  if (Settings::Config::cbAlwaysOnTop.read().toBool())
  {
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
  }

  //	The settings are done, now creation of the GUI parts
  ui->setupUi(this);

  Settings::Main::posAndSize.read_widget_geometry(this, 720, 540, true);

  setup_ui_colors();
  _create_status_info();

  ui->lblDynLabel->setTextFormat(Qt::RichText);
  ui->lblDynLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  ui->lblDynLabel->setOpenExternalLinks(true);

  ui->thermoPeakLevelLeft->setValue(-40.0);
  ui->thermoPeakLevelRight->setValue(-40.0);
  ui->thermoPeakLevelLeft->setFillBrush(QColor(0x6F70EF));
  ui->thermoPeakLevelRight->setFillBrush(QColor(0x6F70EF));
  ui->thermoPeakLevelLeft->setBorderWidth(0);
  ui->thermoPeakLevelRight->setBorderWidth(0);

  mpServiceListHandler.reset(new ServiceListHandler(iFileNameDb, ui->tblServiceList));

  // only the queued call will consider the button size
  QMetaObject::invokeMethod(this, "_slot_update_mute_state", Qt::QueuedConnection, Q_ARG(bool, false));
  QMetaObject::invokeMethod(this, "_slot_favorite_changed", Qt::QueuedConnection, Q_ARG(bool, false));
  QMetaObject::invokeMethod(this, &DabRadio::_slot_set_static_button_style, Qt::QueuedConnection);

  setWindowTitle(PRJ_NAME);

  mpTechDataWidget.reset(new TechData(this, mpTechDataBuffer));

  _show_epg_label(false);

  mChannel.currentService.valid = false;
  mChannel.nextService.valid = false;
  mChannel.serviceCount = -1;

  // load last used service

  if (QString presetName = Settings::Main::varPresetName.read().toString();
      !presetName.isEmpty())
  {
    QStringList ss = presetName.split(":");

    if (ss.size() == 2)
    {
      mChannel.nextService.channel = ss.at(0);
      mChannel.nextService.serviceName = ss.at(1);
    }
    else
    {
      mChannel.nextService.channel = "";
      mChannel.nextService.serviceName = presetName;
    }

    mChannel.nextService.SId = 0;
    mChannel.nextService.SCIds = 0;
    mChannel.nextService.valid = true;
  }

  mChannel.targetPos = cf32(0, 0);
  const f32 local_lat = Settings::Config::varLatitude.read().toFloat();
  const f32 local_lon = Settings::Config::varLongitude.read().toFloat();
  mChannel.localPos = cf32(local_lat, local_lon);

  mConfig.cmbSoftBitGen->addItems(get_soft_bit_gen_names()); // fill soft-bit-type combobox with text elements

  mpTechDataWidget->hide();  // until shown otherwise
  mServiceList.clear();
#ifdef DATA_STREAMER
  dataStreamer = new tcpServer(iDataPort);
#else
  (void)iDataPort;
#endif

  // Where do we leave the audio out?
  mpAudioOutput = new AudioOutputQt(this);
  connect(mpAudioOutput, &IAudioOutput::signal_audio_devices_list, this, &DabRadio::_slot_load_audio_device_list);
  connect(this, &DabRadio::signal_start_audio, mpAudioOutput, &IAudioOutput::slot_start, Qt::QueuedConnection);
  connect(this, &DabRadio::signal_switch_audio, mpAudioOutput, &IAudioOutput::slot_restart, Qt::QueuedConnection);
  connect(this, &DabRadio::signal_stop_audio, mpAudioOutput, &IAudioOutput::slot_stop, Qt::QueuedConnection);
  connect(this, &DabRadio::signal_set_audio_device, mpAudioOutput, &IAudioOutput::slot_set_audio_device, Qt::QueuedConnection);
  connect(this, &DabRadio::signal_audio_mute, mpAudioOutput, &IAudioOutput::slot_mute, Qt::QueuedConnection);
  connect(this, &DabRadio::signal_audio_buffer_filled_state, ui->progBarAudioBuffer, &QProgressBar::setValue);
  connect(ui->sliderVolume, &QSlider::valueChanged, this, &DabRadio::_slot_handle_volume_slider);

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

  mPicturesPath = Settings::Config::varPicturesPath.read().toString();
  mPicturesPath = check_and_create_dir(mPicturesPath);

  mMotPath = Settings::Config::varMotPath.read().toString();
  mMotPath = check_and_create_dir(mMotPath);

  mEpgPath = Settings::Config::varEpgPath.read().toString();
  mEpgPath = check_and_create_dir(mEpgPath);

  connect(&mEpgProcessor, &EpgDecoder::signal_set_epg_data, this, &DabRadio::slot_set_epg_data);

  //	timer for autostart epg service
  mEpgTimer.setSingleShot(true);
  connect(&mEpgTimer, &QTimer::timeout, this, &DabRadio::slot_epg_timer_timeout);

  mpTimeTable = new timeTableHandler(this);
  mpTimeTable->hide();

  connect(this, &DabRadio::signal_set_new_channel, ui->cmbChannelSelector, &QComboBox::setCurrentIndex);
  connect(ui->btnHttpServer, &QPushButton::clicked, this,  &DabRadio::_slot_handle_http_button);

  //	restore some settings from previous incarnations
  mBandHandler.setupChannels(ui->cmbChannelSelector, BAND_III);
  const QString skipFileName = Settings::Config::varSkipFile.read().toString();
  mBandHandler.setup_skipList(skipFileName);

  connect(mpTechDataWidget.get(), &TechData::signal_handle_timeTable, this, &DabRadio::_slot_handle_time_table);

  ui->lblVersion->setText(QString("V" + mVersionStr));
  ui->lblCopyrightIcon->setToolTip(get_copyright_text());
  ui->lblCopyrightIcon->setTextInteractionFlags(Qt::TextBrowserInteraction);
  ui->lblCopyrightIcon->setOpenExternalLinks(true);

  QString tiiFileName = Settings::Config::varTiiFile.read().toString();
  mChannel.tiiFile = false;

  if (tiiFileName.isEmpty() || !QFile(tiiFileName).exists())
  {
    tiiFileName = ":res/txdata.tii";
  }

  mChannel.tiiFile = mTiiHandler.fill_cache_from_tii_file(tiiFileName);
  if (!mChannel.tiiFile)
  {
    ui->btnHttpServer->setToolTip("File '" + tiiFileName + "' could not be loaded. So this feature is switched off.");
    ui->btnHttpServer->setEnabled(false);
  }

  connect(this, &DabRadio::signal_dab_processor_started, &mSpectrumViewer, &SpectrumViewer::slot_update_settings);
  connect(&mSpectrumViewer, &SpectrumViewer::signal_window_closed, this, &DabRadio::_slot_handle_spectrum_button);
  connect(mpTechDataWidget.get(), &TechData::signal_window_closed, this, &DabRadio::_slot_handle_tech_detail_button);

  mChannel.etiActive = false;
  show_pause_slide();

  // start the timer(s)
  // The displaytimer is there to show the number of seconds running and handle - if available - the tii data
  mDisplayTimer.setInterval(cDisplayTimeoutMs);
  connect(&mDisplayTimer, &QTimer::timeout, this, &DabRadio::_slot_update_time_display);
  mDisplayTimer.start(cDisplayTimeoutMs);

  //	timer for scanning
  mChannelTimer.setSingleShot(true);
  mChannelTimer.setInterval(cChannelTimeoutMs);
  connect(&mChannelTimer, &QTimer::timeout, this, &DabRadio::_slot_channel_timeout);

  //	presetTimer
  mPresetTimer.setSingleShot(true);
  mPresetTimer.setInterval(cPresetTimeoutMs);
  connect(&mPresetTimer, &QTimer::timeout, this, &DabRadio::_slot_preset_timeout);

  _set_clock_text();
  mClockResetTimer.setSingleShot(true);
  connect(&mClockResetTimer, &QTimer::timeout, [this](){ _set_clock_text(); });

  mTiiIndexCntTimer.setSingleShot(true);
  mTiiIndexCntTimer.setInterval(cTiiIndexCntTimeoutMs);

  mConfig.deviceSelector->addItems(mDeviceSelector.get_device_name_list());

  {
    const QString h = Settings::Main::varSdrDevice.read().toString();
    const i32 k = mConfig.deviceSelector->findText(h);

    if (k != -1)
    {
      mConfig.deviceSelector->setCurrentIndex(k);

      mpInputDevice = mDeviceSelector.create_device(mConfig.deviceSelector->currentText(), mChannel.realChannel);

      if (mpInputDevice != nullptr)
      {
        _set_device_to_file_mode(!mChannel.realChannel);
      }
    }
  }

  connect(ui->btnEject, &QPushButton::clicked, this, [this](bool){ _slot_new_device(mDeviceSelector.get_device_name()); });
  connect(ui->configButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_config_button);

  if (Settings::SpectrumViewer::varUiVisible.read().toBool())
  {
    mSpectrumViewer.show();
  }

  if (Settings::TechDataViewer::varUiVisible.read().toBool())
  {
    mpTechDataWidget->show();
  }

  mShowTiiListWindow = Settings::TiiViewer::varUiVisible.read().toBool();

  // if a device was selected, we just start, otherwise we wait until one is selected
  if (mpInputDevice != nullptr)
  {
    if (do_start())
    {
      qApp->installEventFilter(this);
      return;
    }
    else
    {
      mpInputDevice = nullptr;
    }
  }

  mConfig.show(); // show configuration windows that the user selects a (valid) device

  connect(mConfig.deviceSelector, &QComboBox::textActivated, this, &DabRadio::_slot_do_start);

  qApp->installEventFilter(this);
}

DabRadio::~DabRadio()
{
  if (mAudioOutputThread != nullptr)
  {
    mAudioOutputThread->quit();  // this deletes mpAudioOutput
    mAudioOutputThread->wait();
    delete mAudioOutputThread;
  }

  delete ui;

  fprintf(stdout, "RadioInterface is deleted\n");
}

void DabRadio::_set_clock_text(const QString & iText /*= QString()*/)
{
  if (!iText.isEmpty())
  {
    mClockResetTimer.start(cClockResetTimeoutMs); // if in 5000ms no new clock text is sent, this method is called with an empty iText

    if (!mClockActiveStyle)
    {
      ui->lblLocalTime->setStyleSheet(get_bg_style_sheet({ 255, 60, 60 }, "QLabel") + " QLabel { color: white; }");
      mClockActiveStyle = true;
    }
    ui->lblLocalTime->setText(iText);
  }
  else
  {
    if (mClockActiveStyle)
    {
      ui->lblLocalTime->setStyleSheet(get_bg_style_sheet({ 57, 0, 0 }, "QLabel") + " QLabel { color: #333333; }");
      mClockActiveStyle = false;
    }
    //ui->lblLocalTime->setText("YYYY-MM-DD hh:mm:ss");
    ui->lblLocalTime->setText("0000-00-00 00:00:00");
  }
}

void DabRadio::_show_epg_label(const bool iShowLabel)
{
  _set_status_info_status(mStatusInfo.EPG, iShowLabel);
}

// _slot_do_start(QString) is called when - on startup - NO device was registered to be used,
// and the user presses the selectDevice comboBox
void DabRadio::_slot_do_start(const QString & dev)
{
  mpInputDevice = mDeviceSelector.create_device(dev, mChannel.realChannel);

  if (mpInputDevice == nullptr)
  {
    disconnect_gui();
    return;
  }

  _set_device_to_file_mode(!mChannel.realChannel);

  do_start();
}

// _slot_new_device is called from the UI when selecting a device with the selector
void DabRadio::_slot_new_device(const QString & deviceName)
{
  //	Part I : stopping all activities
  mIsRunning.store(false);
  stop_scanning();
  stop_channel();
  disconnect_gui();
  if (mpInputDevice != nullptr)
  {
    fprintf(stderr, "device is deleted\n");
    mpInputDevice = nullptr;
  }

  LOG("selecting ", deviceName);
  mDeviceSelector.reset_file_input_last_file(deviceName); // this allows new file name selection for "file input"
  mpInputDevice = mDeviceSelector.create_device(deviceName, mChannel.realChannel);

  if (mpInputDevice == nullptr)
  {
    return;    // nothing will happen
  }

  _set_device_to_file_mode(!mChannel.realChannel);

  do_start();    // will set running
}

QString DabRadio::_get_scan_message(const bool iEndMsg) const
{
  QString s = "<span style='color:yellow;'>";
  s += (iEndMsg ? "Scan ended - Select a service on the left" : "Scanning channel: " + ui->cmbChannelSelector->currentText()) + "</span>";
  s += "<br><span style='color:lightblue; font-size:small;'>Found DAB channels: " + QString::number(mScanResult.NrChannels) + "</span>";
  s += "<br><span style='color:lightblue; font-size:small;'>Found audio services: " + QString::number(mScanResult.NrAudioServices) + "</span>";
  s += "<br><span style='color:lightblue; font-size:small;'>Found non-audio services: " + QString::number(mScanResult.NrNonAudioServices) + "</span>";
  return s;
}

//	when doStart is called, a device is available and selected
bool DabRadio::do_start()
{
  _update_channel_selector();

  // here the DAB processor is created (again)
  mpDabProcessor.reset(new DabProcessor(this, mpInputDevice.get(), &mProcessParams));

  mChannel.clean_channel();

  //	Some hidden buttons can be made visible now
  connect_gui();
  connect_dab_processor();

  mTiiListDisplay.hide();

  mpDabProcessor->set_sync_on_strongest_peak(Settings::Config::cbUseStrongestPeak.read().toBool());
  mpDabProcessor->set_dc_avoidance_algorithm(Settings::Config::cbUseDcAvoidance.read().toBool());
  mpDabProcessor->set_dc_removal(Settings::Config::cbUseDcRemoval.read().toBool());
  mpDabProcessor->set_tii_collisions(Settings::Config::cbTiiCollisions.read().toBool());
  mpDabProcessor->set_tii_processing(true);
  mpDabProcessor->set_tii_threshold(Settings::Config::sbTiiThreshold.read().toInt());
  mpDabProcessor->set_tii_sub_id(Settings::Config::sbTiiSubId.read().toInt());
  {
    int idx = mConfig.cmbSoftBitGen->currentIndex();
    mpDabProcessor->slot_soft_bit_gen_type((ESoftBitType)idx);
  }

  if (Settings::CirViewer::varUiVisible.read().toBool())
  {
    mpDabProcessor->activate_cir_viewer(true);
    mCirViewer.show();
  }

  // should the device widget be shown?
  if (Settings::Main::varDeviceUiVisible.read().toBool())
  {
    mpInputDevice->show();
  }

  // if (mChannel.nextService.valid)
  // {
  //   mPresetTimer.start(cPresetTimeoutMs);
  // }

  emit signal_dab_processor_started(); // triggers the DAB processor rereading (new) settings

  //	after the preset timer signals, the service will be started
  start_channel(ui->cmbChannelSelector->currentText());
  mIsRunning.store(true);

  return true;
}

void DabRadio::_update_channel_selector()
{
  if (mChannel.nextService.channel != "")
  {
    int k = ui->cmbChannelSelector->findText(mChannel.nextService.channel);
    if (k != -1)
    {
      ui->cmbChannelSelector->setCurrentIndex(k);
    }
  }
  else
  {
    ui->cmbChannelSelector->setCurrentIndex(0);
  }
}

//
///////////////////////////////////////////////////////////////////////////////
//
//	a slot called by the DAB-processor
void DabRadio::slot_set_and_show_freq_corr_rf_Hz(int iFreqCorrRF)
{
  if (mpInputDevice != nullptr && mChannel.nominalFreqHz > 0)
  {
    mpInputDevice->setVFOFrequency(mChannel.nominalFreqHz + iFreqCorrRF);
  }

  mSpectrumViewer.show_freq_corr_rf_Hz(iFreqCorrRF);
}

void DabRadio::slot_show_freq_corr_bb_Hz(int iFreqCorrBB)
{
  mSpectrumViewer.show_freq_corr_bb_Hz(iFreqCorrBB);
}

//
//	might be called when scanning only
void DabRadio::_slot_channel_timeout()
{
  qCDebug(sLogRadioInterface()) << Q_FUNC_INFO;
  slot_no_signal_found();
}

///////////////////////////////////////////////////////////////////////////
//
//	a slot, called by the fic/fib handlers
void DabRadio::slot_add_to_ensemble(const QString & iServiceName, const u32 iSId)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (!mIsScanning)
  {
    mPresetTimer.start(cPresetTimeoutMs);
  }

  qCDebug(sLogRadioInterface()) << Q_FUNC_INFO << iServiceName << QString::number(iSId, 16);

  const i32 subChId = mpDabProcessor->get_sub_channel_id(iServiceName, iSId);

  if (subChId < 0)
  {
    return;
  }

  SServiceId ed;
  ed.name = iServiceName;
  ed.SId = iSId;
  // ed.subChId = subChId;

  if (std::any_of(mServiceList.cbegin(), mServiceList.cend(), [& ed](const SServiceId & sid) { return sid.name == ed.name; } ))
  {
    return; // service already in service list
  }

  const bool isAudioService = mpDabProcessor->is_audio_service(iServiceName);

  if (mConfig.cbShowNonAudioInServiceList->isChecked() || isAudioService)
  {
    mServiceList = insert_sorted(mServiceList, ed);
    mpServiceListHandler->add_entry(mChannel.channelName, iServiceName);
  }

  if (mIsScanning)
  {
    if (isAudioService)
    {
      mScanResult.NrAudioServices++;
      if (mScanResult.LastChannel != mChannel.channelName)
      {
        mScanResult.LastChannel = mChannel.channelName;
        mScanResult.NrChannels++;
      }
    }
    else
    {
      mScanResult.NrNonAudioServices++;
    }
  }

  if (!mIsScanning) // trigger a second time as routine takes sometimes longer...
  {
    mPresetTimer.start(cPresetTimeoutMs);
  }
}

//	The ensembleId is written as hexadecimal, however, the
//	number display of Qt is only 7 segments ...
static QString hex_to_str(int v)
{
  QString res;
  for (int i = 0; i < 4; i++)
  {
    u8 t = (v & 0xF000) >> 12;
    QChar c = t <= 9 ? (char)('0' + t) : (char)('A' + t - 10);
    res.append(c);
    v <<= 4;
  }
  return res;
}

//	a slot, called by the fib processor
void DabRadio::slot_name_of_ensemble(int id, const QString & v)
{
  qCDebug(sLogRadioInterface) << Q_FUNC_INFO << id << v;

  if (!mIsScanning && mPresetTimer.isActive())  // retrigger preset timer if already running
  {
    mPresetTimer.start(cPresetTimeoutMs);
  }

  if (!mIsRunning.load())
  {
    return;
  }

  QFont font = ui->ensembleId->font();
  font.setPointSize(14);
  ui->ensembleId->setFont(font);
  ui->ensembleId->setText(v + QString("(") + hex_to_str(id) + QString(")"));

  mChannel.ensembleName = v;
  mChannel.Eid = id;
}

void DabRadio::_slot_handle_content_button()
{
  const QStringList s = mpDabProcessor->basicPrint();

  if (mpContentTable != nullptr)
  {
    mpContentTable->hide();
    delete mpContentTable;
    mpContentTable = nullptr;
    return;
  }
  QString theTime;
  QString SNR = "SNR " + QString::number(mChannel.snr);

  if (Settings::Config::cbUseUtcTime.read().toBool())
  {
    theTime = convertTime(mUTC.year, mUTC.month, mUTC.day, mUTC.hour, mUTC.minute);
  }
  else
  {
    theTime = convertTime(mLocalTime.year, mLocalTime.month, mLocalTime.day, mLocalTime.hour, mLocalTime.minute);
  }

  QString convLocation = ui->cmbTiiList->itemText(0).replace(",", ";");
  QString header = mChannel.ensembleName + ";" + mChannel.channelName + ";" + QString::number(mChannel.nominalFreqHz / 1000) + ";"
                   + hex_to_str(mChannel.Eid) + " " + ";" + ui->transmitter_coordinates->text() + " " + ";" + theTime + ";" + SNR + ";"
                   + QString::number(mServiceList.size()) + ";" + convLocation + "\n";

  mpContentTable = new ContentTable(this, &Settings::Storage::instance(), mChannel.channelName, mpDabProcessor->scan_width());
  connect(mpContentTable, &ContentTable::signal_go_service, this, &DabRadio::slot_handle_content_selector);

  mpContentTable->addLine(header);
  //	my_contentTable		-> addLine ("\n");

  for (const auto & sl : s)
  {
    mpContentTable->addLine(sl);
  }
  mpContentTable->show();
}

QString DabRadio::check_and_create_dir(const QString & s) const
{
  if (s.isEmpty())
  {
    return s;
  }

  QString dir = s;

  if (!dir.endsWith(QChar('/')))
  {
    dir += QChar('/');
  }

  create_directory(dir, false);

  return dir;
}

bool DabRadio::save_MOT_EPG_data(const QByteArray & result, const QString & objectName, int contentType)
{
  if (mEpgPath == "")
  {
    return false;
  }

  QString temp = objectName;
  if (temp.isEmpty())
  {
    temp = "epg_file";
  }
  temp = mEpgPath + temp;

  create_directory(temp, true);

  std::vector<u8> epgData(result.begin(), result.end());
  const u32 ensembleId = mpDabProcessor->get_ensemble_id();
  const u32 currentSId = extract_epg(temp, mServiceList, ensembleId);
  const u32 julianDate = mpDabProcessor->get_julian_date();
  const i32 subType = getContentSubType((MOTContentType)contentType);
  mEpgProcessor.process_epg(epgData.data(), (i32)epgData.size(), currentSId, subType, julianDate);

  if (Settings::Config::cbGenXmlFromEpg.read().toBool())
  {
    mEpgHandler.decode(epgData, QDir::toNativeSeparators(temp));
  }
  return true;
}

void DabRadio::slot_handle_mot_object(const QByteArray & result, const QString & objectName, int contentType, bool dirElement)
{
  QString realName;

  switch (getContentBaseType((MOTContentType)contentType))
  {
  case MOTBaseTypeGeneralData: break;
  case MOTBaseTypeText: save_MOT_text(result, contentType, objectName); break;
  case MOTBaseTypeImage: show_MOT_image(result, contentType, objectName, dirElement); break;
  case MOTBaseTypeAudio: break;
  case MOTBaseTypeVideo: break;
  case MOTBaseTypeTransport: save_MOT_object(result, objectName); break;
  case MOTBaseTypeSystem: break;
  case MOTBaseTypeApplication: save_MOT_EPG_data(result, objectName, contentType); break;
  case MOTBaseTypeProprietary:; break;
  }
}

void DabRadio::save_MOT_text(const QByteArray & result, int contentType, const QString & name)
{
  (void)contentType;
  if (mMotPath == "")
  {
    return;
  }

  QString textName = QDir::toNativeSeparators(mMotPath + name);

  FILE * x = fopen(textName.toUtf8().data(), "w+b");
  if (x == nullptr)
  {
    fprintf(stderr, "cannot write file %s\n", textName.toUtf8().data());
  }
  else
  {
    fprintf(stdout, "going to write text file %s\n", textName.toUtf8().data());
    (void)fwrite(result.data(), 1, result.length(), x);
    fclose(x);
  }
}

void DabRadio::save_MOT_object(const QByteArray & result, const QString & name)
{
  if (mMotPath == "")
  {
    return;
  }

  if (name == "")
  {
    const QString name2 = "motObject_" + QString::number(mMotObjectCnt);
    ++mMotObjectCnt;
    save_MOT_text(result, 5, name2);
  }
  else
  {
    save_MOT_text(result, 5, name);
  }
}

void DabRadio::create_directory(const QString & iDirOrPath, const bool iContainsFileName) const
{
  const QString temp = iContainsFileName ? iDirOrPath.left(iDirOrPath.lastIndexOf(QChar('/'))) : iDirOrPath;

  if (!QDir(temp).exists())
  {
    QDir().mkpath(temp);
  }
}

//	MOT slide, to show
void DabRadio::show_MOT_image(const QByteArray & data, const int contentType, const QString & pictureName, const int dirs)
{
  const char * type;
  if (!mIsRunning.load() || pictureName.isEmpty())
  {
    return;
  }

  switch (static_cast<MOTContentType>(contentType))
  {
  case MOTCTImageGIF:  type = "gif"; break;
  case MOTCTImageJFIF: type = "jpg"; break;
  case MOTCTImageBMP:  type = "bmp"; break;
  case MOTCTImagePNG:  type = "png"; break;
  default: return;
  }

  qCDebug(sLogRadioInterface(), "show_MOTlabel %s, contentType 0x%x, dirs %d, type %s",
          pictureName.toLocal8Bit().constData(), contentType, dirs, type);

  if (Settings::Config::cbSaveSlides.read().toBool() && (mPicturesPath != ""))
  {
    const QString hashStr = QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().left(8);
    static const QRegularExpression regex(R"([<>:\"/\\|?*\s])");
    const QString pathEnsemble = mChannel.ensembleName.trimmed().replace(regex, "-");
    const QString pathService = mChannel.currentService.serviceName.trimmed().replace(regex, "-");
    const QString filename = pathEnsemble + "_" + pathService + "_" + hashStr + "." + type;
    const QString filepath = pathEnsemble + "/" + pathService + "/";
    QString pict = mPicturesPath + filepath + filename;
    create_directory(pict, true);
    pict = QDir::toNativeSeparators(pict);
    qInfo() << "filepath:" << pict << "(pictureName:" << pictureName << ")";

    if (!QFile::exists(pict))
    {
      QFile file(pict);
      if (!file.open(QIODevice::WriteOnly))
      {
        qCCritical(sLogRadioInterface(), "cannot write file %s", pict.toUtf8().data());
      }
      else
      {
        qCDebug(sLogRadioInterface(), "going to write picture file %s", pict.toUtf8().data());
        file.write(data);
      }
    }
    else
    {
      qCDebug(sLogRadioInterface(), "file %s already exists", pict.toUtf8().data());
    }
  }

  // only show slides if it is a audio service
  if (mChannel.currentService.is_audio)
  {
    QPixmap p;
    p.loadFromData(data, type);
    write_picture(p);
  }
}

void DabRadio::write_picture(const QPixmap & iPixMap) const
{
  constexpr int w = 320;
  constexpr int h = 240;

  // typical the MOT size is 320 : 240 , so only scale for other sizes
  if (iPixMap.width() != w || iPixMap.height() != h)
  {
    //qDebug("MOT w: %d, h: %d (scaled)", iPixMap.width(), iPixMap.height());
    ui->pictureLabel->setPixmap(iPixMap.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  else
  {
    // qDebug("MOT w: %d, h: %d", iPixMap.width(), iPixMap.height());
    ui->pictureLabel->setPixmap(iPixMap);
  }

  ui->pictureLabel->setAlignment(Qt::AlignCenter);
  ui->pictureLabel->show();
}

//
//	sendDatagram is triggered by the ip handler,
void DabRadio::slot_send_datagram(int length)
{
  auto * const localBuffer = make_vla(u8, length);

  if (mpDataBuffer->get_ring_buffer_read_available() < length)
  {
    fprintf(stderr, "Something went wrong\n");
    return;
  }

  mpDataBuffer->get_data_from_ring_buffer(localBuffer, length);
}

//
//	tdcData is triggered by the backend.
void DabRadio::slot_handle_tdc_data(int frametype, int length)
{
  qCDebug(sLogRadioInterface) << Q_FUNC_INFO << frametype << length;
#ifdef DATA_STREAMER
  u8 localBuffer [length + 8];
#endif
  (void)frametype;
  if (!mIsRunning.load())
  {
    return;
  }
  if (mpDataBuffer->get_ring_buffer_read_available() < length)
  {
    fprintf(stderr, "Something went wrong\n");
    return;
  }
#ifdef DATA_STREAMER
  localBuffer[0] = 0xFF;
  localBuffer[1] = 0x00;
  localBuffer[2] = 0xFF;
  localBuffer[3] = 0x00;
  localBuffer[4] = (length & 0xFF) >> 8;
  localBuffer[5] = length & 0xFF;
  localBuffer[6] = 0x00;
  localBuffer[7] = frametype == 0 ? 0 : 0xFF;
  if (mIsRunning.load())
  {
    dataStreamer->sendData(localBuffer, length + 8);
  }
#endif
}

/**
  *	If a change is detected, we have to restart the selected
  *	service - if any. If the service is a secondary service,
  *	it might be the case that we have to start the main service
  *	how do we find that?
  *
  *	Response to a signal, so we presume that the signaling body exists
  *	signal may be pending though
  */
void DabRadio::slot_change_in_configuration()
{
  if (!mIsRunning.load() || mpDabProcessor == nullptr)
  {
    return;
  }
  SDabService s;
  if (mChannel.currentService.valid)
  {
    s = mChannel.currentService;
  }
  stop_service(s);
  stop_scanning();

  // we stop all secondary services as well, but we maintain there description, file descriptors remain of course
  for (auto & backgroundService : mChannel.backgroundServices)
  {
    mpDabProcessor->stop_service(backgroundService.subChId, BACK_GROUND);
  }

  fprintf(stdout, "configuration change will be effected\n");

  //	we rebuild the services list from the fib and	then we (try to) restart the service
  mServiceList = mpDabProcessor->get_services();

  if (mChannel.etiActive)
  {
    mpDabProcessor->reset_eti_generator();
  }

  //	Of course, it may be disappeared
  if (s.valid)
  {
    if (const QString ss = mpDabProcessor->find_service(s.SId, s.SCIds);
        ss != "")
    {
      start_service(s);
      return;
    }

    //	The service is gone, it may be the subservice of another one
    s.SCIds = 0;
    s.serviceName = mpDabProcessor->find_service(s.SId, s.SCIds);

    if (s.serviceName != "")
    {
      start_service(s);
    }
  }

  //	we also have to restart all background services,
  for (u16 i = 0; i < mChannel.backgroundServices.size(); ++i)
  {
    if (const QString ss = mpDabProcessor->find_service(s.SId, s.SCIds);
        ss == "") // it is gone, close the file if any
    {
      if (mChannel.backgroundServices.at(i).fd != nullptr)
      {
        fclose(mChannel.backgroundServices.at(i).fd);
      }
      mChannel.backgroundServices.erase(mChannel.backgroundServices.begin() + i);
    }
    else // (re)start the service
    {
      if (mpDabProcessor->is_audio_service(ss))
      {
        Audiodata ad;
        FILE * f = mChannel.backgroundServices.at(i).fd;
        mpDabProcessor->get_data_for_audio_service(ss, &ad);
        mpDabProcessor->set_audio_channel(&ad, mpAudioBufferFromDecoder, f, BACK_GROUND);
        mChannel.backgroundServices.at(i).subChId = ad.subchId;
      }
      else
      {
        Packetdata pd;
        mpDabProcessor->get_data_for_packet_service(ss, &pd, 0);
        mpDabProcessor->set_data_channel(&pd, mpDataBuffer, BACK_GROUND);
        mChannel.backgroundServices.at(i).subChId = pd.subchId;
      }
    }
  }
}

//
//	In order to not overload with an enormous amount of
//	signals, we trigger this function at most 10 times a second
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

#ifdef HAVE_PLUTO_RXTX
    if (streamerOut != nullptr)
    {
      streamerOut->audioOut(vec, amount, rate);
    }
#endif
  }

  emit signal_audio_buffer_filled_state((i32)mAudioBufferFillFiltered);
}
//

/////////////////////////////////////////////////////////////////////////////
//
/**
  *	\brief TerminateProcess
  *	Pretty critical, since there are many threads involved
  *	A clean termination is what is needed, regardless of the GUI
  */
void DabRadio::_slot_terminate_process()
{
  if (mIsScanning.load())
  {
    stop_scanning();
  }
  mIsRunning.store(false);
  _show_hide_buttons(false);
  mTiiListDisplay.hide();

  Settings::Main::posAndSize.write_widget_geometry(this);

  mConfig.save_position_and_config();

#ifdef  DATA_STREAMER
  fprintf (stdout, "going to close the dataStreamer\n");
  delete		dataStreamer;
#endif
  if (mpHttpHandler != nullptr)
  {
    mpHttpHandler->stop();
  }
  mDisplayTimer.stop();
  mChannelTimer.stop();
  mPresetTimer.stop();
  mEpgTimer.stop();
  emit signal_stop_audio();
  if (mDlTextFile != nullptr)
  {
    fclose(mDlTextFile);
  }
#ifdef HAVE_PLUTO_RXTX
  if (streamerOut != nullptr)
  {
    streamerOut->stop();
  }
#endif
  if (mpDabProcessor != nullptr)
  {
    mpDabProcessor->stop();
  }

  mpTechDataWidget->hide();

  if (mpContentTable != nullptr)
  {
    mpContentTable->clearTable();
    mpContentTable->hide();
    delete mpContentTable;
  }

  mBandHandler.saveSettings();
  stop_frame_dumping();
  stop_source_dumping();
  stop_audio_dumping();
  mBandHandler.hide();
  mConfig.hide();
  LOG("terminating ", "");
  usleep(1000);    // pending signals
  if (mpLogFile != nullptr)
  {
    fclose(mpLogFile);
  }
  mpLogFile = nullptr;

  // everything should be halted by now
  mSpectrumViewer.hide();
  mCirViewer.hide();

  mpDabProcessor.reset();
  delete mpTimeTable;
  mpInputDevice = nullptr;

  fprintf(stdout, ".. end the radio silences\n");
}

void DabRadio::_slot_update_time_display()
{
  if (!mIsRunning.load())
  {
    return;
  }

#if 0 && !defined(NDEBUG)
  if (mResetRingBufferCnt > 5) // wait 5 seconds to start
  {
    sRingBufferFactoryUInt8.print_status(false);
    sRingBufferFactoryInt16.print_status(false);
    sRingBufferFactoryCmplx16.print_status(false);
    sRingBufferFactoryFloat.print_status(false);
    sRingBufferFactoryCmplx.print_status(false);
  }
  else
  {
    // reset min/max state
    sRingBufferFactoryUInt8.print_status(true);
    sRingBufferFactoryInt16.print_status(true);
    sRingBufferFactoryCmplx16.print_status(true);
    sRingBufferFactoryFloat.print_status(true);
    sRingBufferFactoryCmplx.print_status(true);

    ++mResetRingBufferCnt;
  }
#endif

  mNumberOfSeconds++;

  if ((mNumberOfSeconds % 2) == 0)
  {
    size_t idle_time, total_time;
    if (get_cpu_times(idle_time, total_time))
    {
      const f32 idle_time_delta = static_cast<f32>(idle_time - mPreviousIdleTime);
      const f32 total_time_delta = static_cast<f32>(total_time - mPreviousTotalTime);
      const f32 utilization = 100.0f * (1.0f - idle_time_delta / total_time_delta);
      mConfig.cpuMonitor->display(QString("%1").arg(utilization, 0, 'f', 2));
      mPreviousIdleTime = idle_time;
      mPreviousTotalTime = total_time;
    }
  }

  //	The timer runs autonomously, so it might happen
  //	that it rings when there is no processor running
  if (mpDabProcessor == nullptr)
  {
    return;
  }
}

void DabRadio::_slot_handle_device_widget_button()
{
  if (mpInputDevice == nullptr)
  {
    return;
  }

  if (mpInputDevice->isHidden())
  {
    mpInputDevice->show();
  }
  else
  {
    mpInputDevice->hide();
  }

  Settings::Main::varDeviceUiVisible.write(!mpInputDevice->isHidden());
}

void DabRadio::_slot_handle_tii_button()
{
  assert(mpDabProcessor != nullptr);

  if (mShowTiiListWindow)
  {
    mTiiListDisplay.hide();
  }
  else
  {
    mTiiListDisplay.show();
  }

  mShowTiiListWindow = !mShowTiiListWindow;
  Settings::TiiViewer::varUiVisible.write(mShowTiiListWindow);
}

void DabRadio::slot_handle_tii_threshold(int trs)
{
  if (mpDabProcessor == nullptr)
  {
    return;
  }

  mpDabProcessor->set_tii_threshold(trs);
}

void DabRadio::slot_handle_tii_subid(int subid)
{
  if (mpDabProcessor == nullptr)
  {
    return;
  }

  mpDabProcessor->set_tii_sub_id(subid);
}
///////////////////////////////////////////////////////////////////////////
//	signals, received from ofdm_decoder for which that data is
//	to be displayed
///////////////////////////////////////////////////////////////////////////

//	called from the fibDecoder
void DabRadio::slot_clock_time(int year, int month, int day, int hours, int minutes, int utc_day, int utc_hour, int utc_min, int utc_sec)
{
  mLocalTime.year = year;
  mLocalTime.month = month;
  mLocalTime.day = day;
  mLocalTime.hour = hours;
  mLocalTime.minute = minutes;
  mLocalTime.second = utc_sec;

  mUTC.year = year;
  mUTC.month = month;
  mUTC.day = utc_day;
  mUTC.hour = utc_hour;
  mUTC.minute = utc_min;

  QString result;

  if (Settings::Config::cbUseUtcTime.read().toBool())
  {
    result = convertTime(year, month, day, utc_hour, utc_min, utc_sec);
  }
  else
  {
    result = convertTime(year, month, day, hours, minutes, utc_sec);
  }
  _set_clock_text(result);
}

QString DabRadio::convertTime(int year, int month, int day, int hours, int minutes, int seconds /*= -1*/)
{
  QString t = QString::number(year).rightJustified(4, '0') + "-" +
              QString::number(month).rightJustified(2, '0') + "-" +
              QString::number(day).rightJustified(2, '0') + "  " +
              QString::number(hours).rightJustified(2, '0') + ":" +
              QString::number(minutes).rightJustified(2, '0');

  if (seconds >= 0)
  {
    t += ":" + QString::number(seconds).rightJustified(2, '0');
  }
  return t;
}

//
//	called from the MP4 decoder
void DabRadio::slot_show_frame_errors(int s)
{
  if (!mIsRunning.load())
  {
    return;
  }
  if (!mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->slot_show_frame_error_bar(s);
  }
}

//
//	called from the MP4 decoder
void DabRadio::slot_show_rs_errors(int s)
{
  if (!mIsRunning.load())
  {    // should not happen
    return;
  }

  if (!mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->slot_show_rs_error_bar(s);
  }
}

//
//	called from the aac decoder
void DabRadio::slot_show_aac_errors(int s)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (!mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->slot_show_aac_error_bar(s);
  }
}

//
//	called from the ficHandler
void DabRadio::slot_show_fic_success(bool b)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (b)
  {
    mFicSuccess++;
  }

  if (++mFicBlocks >= 100)
  {
    QPalette p = ui->progBarFicError->palette();
    if (mFicSuccess < 85)
    {
      p.setColor(QPalette::Highlight, Qt::red);
    }
    else
    {
      p.setColor(QPalette::Highlight, 0xE6E600);
    }

    ui->progBarFicError->setPalette(p);
    ui->progBarFicError->setValue(mFicSuccess);
    mFicSuccess = 0;
    mFicBlocks = 0;
  }
}

void DabRadio::slot_show_fic_ber(f32 ber)
{
  if (!mIsRunning.load())
    return;
  if (!mSpectrumViewer.is_hidden())
    mSpectrumViewer.show_fic_ber(ber);
}

// called from the PAD handler
void DabRadio::slot_show_mot_handling()
{
  if (!mIsRunning.load())
  {
    return;
  }
  mpTechDataWidget->slot_trigger_motHandling();
}

//	called from the PAD handler

void DabRadio::slot_show_label(const QString & s)
{
#ifdef HAVE_PLUTO_RXTX
  if (streamerOut != nullptr)
  {
    streamerOut->addRds(std::string(s.toUtf8().data()));
  }
#endif
  if (mIsRunning.load())
  {
    ui->lblDynLabel->setStyleSheet("color: white");
    ui->lblDynLabel->setText(_convert_links_to_clickable(s));
  }

  //	if we dtText is ON, some work is still to be done
  if ((mDlTextFile == nullptr) || (mDynLabelCache.add_if_new(s)))
  {
    return;
  }

  QString currentChannel = mChannel.channelName;
  QDateTime theDateTime = QDateTime::currentDateTime();
  fprintf(mDlTextFile,
          "%s.%s %4d-%02d-%02d %02d:%02d:%02d  %s\n",
          currentChannel.toUtf8().data(),
          mChannel.currentService.serviceName.toUtf8().data(),
          mLocalTime.year,
          mLocalTime.month,
          mLocalTime.day,
          mLocalTime.hour,
          mLocalTime.minute,
          mLocalTime.second,
          s.toUtf8().data());
}

void DabRadio::slot_set_stereo(bool b)
{
  _set_status_info_status(mStatusInfo.Stereo, b);
}

static QString tiiNumber(int n)
{
  if (n >= 10)
  {
    return QString::number(n);
  }
  return QString("0") + QString::number(n);
}

void DabRadio::slot_show_tii(const std::vector<STiiResult> & iTiiList)
{
  if (!mIsRunning.load())
    return;

  if (mpDabProcessor->get_ecc() == 0)
    return;

  QString country;

  if (!mChannel.has_ecc)
  {
    mChannel.ecc_byte = mpDabProcessor->get_ecc();
    country = find_ITU_code(mChannel.ecc_byte, (mChannel.Eid >> 12) & 0xF);
    mChannel.has_ecc = true;
    mChannel.transmitterName = "";

    if (country != mChannel.countryName)
    {
      ui->transmitter_country->setText(country);
      mChannel.countryName = country;
    }
  }

  const bool isDropDownVisible = ui->cmbTiiList->view()->isVisible();
  const f32 ownLatitude = real(mChannel.localPos);
  const f32 ownLongitude = imag(mChannel.localPos);
  const bool ownCoordinatesSet = (ownLatitude != 0.0f && ownLongitude != 0.0f);

  if (mShowTiiListWindow)
  {
    mTiiListDisplay.start_adding();
  }

  if (!isDropDownVisible)
  {
    ui->cmbTiiList->clear();
  }

  if (mShowTiiListWindow)
  {
    mTiiListDisplay.show();
    mTiiListDisplay.set_window_title("TII list of channel " + mChannel.channelName);
  }

  mTransmitterIds = iTiiList;
  SCacheElem tr_fb; // fallback data storage, if no database entry was found

  for (u32 index = 0; index < mTransmitterIds.size(); index++)
  {
    const STiiResult & tii = mTransmitterIds[index];

    if (tii.mainId == 0xFF)	// shouldn't be
      continue;

    if (index == 0) // show only the "strongest" TII number
    {
      QString a = "TII: " + tiiNumber(tii.mainId) + "-" + tiiNumber(tii.subId);
      ui->transmitter_coordinates->setAlignment(Qt::AlignRight);
      ui->transmitter_coordinates->setText(a);
    }

    const SCacheElem * pTr = mTiiHandler.get_transmitter_name((mChannel.realChannel ? mChannel.channelName : "any"), mChannel.Eid, tii.mainId, tii.subId);
    const bool dataValid = (pTr != nullptr);

    if (!dataValid)
    {
      tr_fb = {};
      tr_fb.mainId = tii.mainId;
      tr_fb.subId = tii.subId;
      tr_fb.country = country;
      tr_fb.Eid = mChannel.Eid;
      tr_fb.transmitterName = "(no database entry)";
      pTr = &tr_fb;
    }

    TiiListDisplay::SDerivedData bd;

    if (dataValid)
    {
      if (ownCoordinatesSet)
      {
        bd.distance_km = mTiiHandler.distance(pTr->latitude, pTr->longitude, ownLatitude, ownLongitude);
        bd.corner_deg = mTiiHandler.corner(pTr->latitude, pTr->longitude, ownLatitude, ownLongitude);
      }
      else
      {
        bd.distance_km = bd.corner_deg = 0.0f;
      }
    }
    else
    {
      bd = {};
    }
    bd.strength_dB = log10_times_10(tii.strength);
    bd.phase_deg = tii.phaseDeg;
    bd.isNonEtsiPhase = tii.isNonEtsiPhase;

    if (mShowTiiListWindow)
    {
      mTiiListDisplay.add_row(*pTr, bd);
    }

    if (!isDropDownVisible && dataValid)
    {
      if (ownCoordinatesSet)
      {
        ui->cmbTiiList->addItem(QString("%1/%2: %3  %4km  %5  %6m + %7m")
                              .arg(index + 1)
                              .arg(mTransmitterIds.size())
                              .arg(pTr->transmitterName)
                              .arg(bd.distance_km, 0, 'f', 1)
                              .arg(CompassDirection::get_compass_direction(bd.corner_deg).c_str())
                              .arg(pTr->altitude)
                              .arg(pTr->height));
      }
      else
      {
        ui->cmbTiiList->addItem(QString("%1/%2: %3 (set map coord. for dist./dir.) %4m + %5m")
                              .arg(index + 1)
                              .arg(mTransmitterIds.size())
                              .arg(pTr->transmitterName)
                              .arg(pTr->altitude)
                              .arg(pTr->height));

      }
    }

    // see if we have a map
    if (mpHttpHandler && dataValid)
    {
      const QDateTime theTime = (Settings::Config::cbUseUtcTime.read().toBool() ? QDateTime::currentDateTimeUtc() : QDateTime::currentDateTime());
      mpHttpHandler->putData(MAP_NORM_TRANS, pTr, theTime.toString(Qt::TextDate),
                             bd.strength_dB, (int)bd.distance_km, (int)bd.corner_deg, bd.isNonEtsiPhase);
    }
  }

  // iterate over combobox entries, if activated
  if (mConfig.cbAutoIterTiiEntries->isChecked() && ui->cmbTiiList->count() > 0 && !isDropDownVisible)
  {
    ui->cmbTiiList->setCurrentIndex((int)mTiiIndex % ui->cmbTiiList->count());

    if (!mTiiIndexCntTimer.isActive())
    {
      mTiiIndex = (mTiiIndex + 1) % ui->cmbTiiList->count();
      mTiiIndexCntTimer.start();
    }
  }

  if (mShowTiiListWindow)
  {
    mTiiListDisplay.finish_adding();
  }
}

void DabRadio::slot_show_spectrum(i32 /*amount*/)
{
  if (!mIsRunning.load())
  {
    return;
  }

  mSpectrumViewer.show_spectrum(mpInputDevice->getVFOFrequency());
}

void DabRadio::slot_show_cir()
{
  if (!mIsRunning.load() || mCirViewer.is_hidden())
  {
    return;
  }

  mCirViewer.show_cir();
}

void DabRadio::slot_show_iq(int iAmount, f32 iAvg)
{
  if (!mIsRunning.load())
  {
    return;
  }

  mSpectrumViewer.show_iq(iAmount, iAvg);
}

void DabRadio::slot_show_lcd_data(const OfdmDecoder::SLcdData * pQD)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (!mSpectrumViewer.is_hidden())
  {
    mSpectrumViewer.show_lcd_data(pQD->CurOfdmSymbolNo, pQD->ModQuality, pQD->TestData1, pQD->TestData2, pQD->MeanSigmaSqFreqCorr, pQD->SNR);
  }
}

void DabRadio::slot_show_digital_peak_level(f32 iPeakLevel)
{
  if (!mIsRunning.load())
  {
    return;
  }

  mSpectrumViewer.show_digital_peak_level(iPeakLevel);
}

//	called from the MP4 decoder
void DabRadio::slot_show_rs_corrections(int c, int ec)
{
  if (!mIsRunning)
  {
    return;
  }

  _set_status_info_status(mStatusInfo.RsError, c > 0);
  _set_status_info_status(mStatusInfo.CrcError, ec > 0);

  if (!mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->slot_show_rs_corrections(c, ec);
  }
}

//
//	called from the DAB processor
void DabRadio::slot_show_clock_error(f32 e)
{
  if (!mIsRunning.load())
  {
    return;
  }
  if (!mSpectrumViewer.is_hidden())
  {
    mSpectrumViewer.show_clock_error(e);
  }
}

//
//	called from the phasesynchronizer
void DabRadio::slot_show_correlation(f32 threshold, const QVector<int> & v)
{
  if (!mIsRunning.load())
  {
    return;
  }

  mSpectrumViewer.show_correlation(threshold, v, mTransmitterIds);
  mChannel.nrTransmitters = v.size();
}

////////////////////////////////////////////////////////////////////////////

void DabRadio::slot_set_stream_selector(int k)
{
  if (!mIsRunning.load())
  {
    return;
  }

  emit signal_set_audio_device(mConfig.cmbSoundOutput->itemData(k).toByteArray());
}

void DabRadio::_slot_handle_tech_detail_button()
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->show();
  }
  else
  {
    mpTechDataWidget->hide();
  }
  Settings::TechDataViewer::varUiVisible.write(!mpTechDataWidget->isHidden());
}

void DabRadio::_slot_handle_cir_button()
{
  if (!mIsRunning.load())
    return;

  if (mCirViewer.is_hidden())
  {
    if (mpDabProcessor) mpDabProcessor->activate_cir_viewer(true);
    mCirViewer.show();
  }
  else
  {
    if (mpDabProcessor) mpDabProcessor->activate_cir_viewer(false);
    mCirViewer.hide();
  }
  Settings::CirViewer::varUiVisible.write(!mCirViewer.is_hidden());
}


// Whenever the input device is a file, some functions, e.g. selecting a channel,
// setting an alarm, are not meaningful
void DabRadio::_show_hide_buttons(const bool iShow)
{
#if 1
  if (iShow)
  {
    mConfig.dumpButton->show();
    ui->btnScanning->show();
    ui->cmbChannelSelector->show();
    ui->btnToggleFavorite->show();
    ui->btnEject->hide();
  }
  else
  {
    mConfig.dumpButton->hide();
    ui->btnScanning->hide();
    ui->cmbChannelSelector->hide();
    ui->btnToggleFavorite->hide();
    ui->btnEject->show();
  }
#else
  mConfig.dumpButton->setEnabled(iShow);
  mConfig.frequencyDisplay->setEnabled(iShow);
  btnScanning->setEnabled(iShow);
  channelSelector->setEnabled(iShow);
  btnToggleFavorite->setEnabled(iShow);
#endif
}

void DabRadio::_slot_handle_reset_button()
{
  if (!mIsRunning.load())
  {
    return;
  }
  QString channelName = mChannel.channelName;
  stop_scanning();
  stop_channel();
  start_channel(channelName);
}

////////////////////////////////////////////////////////////////////////
//
//	dump handling
//
/////////////////////////////////////////////////////////////////////////

void setButtonFont(QPushButton * b, const QString& text, int size)
{
  QFont font = b->font();
  font.setPointSize(size);
  b->setFont(font);
  b->setText(text);
  b->update();
}

void DabRadio::stop_source_dumping()
{
  if (mpRawDumper == nullptr)
  {
    return;
  }

  LOG("source dump stops ", "");
  mpDabProcessor->stop_dumping();
  sf_close(mpRawDumper);
  mpRawDumper = nullptr;
  setButtonFont(mConfig.dumpButton, "Raw dump", 10);
}

//
void DabRadio::start_source_dumping()
{
  QString deviceName = mpInputDevice->deviceName();
  QString channelName = mChannel.channelName;

  if (mIsScanning.load())
  {
    return;
  }

  mpRawDumper = mOpenFileDialog.open_raw_dump_sndfile_ptr(deviceName, channelName);
  if (mpRawDumper == nullptr)
  {
    return;
  }

  LOG("source dump starts ", channelName);
  setButtonFont(mConfig.dumpButton, "writing", 12);
  mpDabProcessor->startDumping(mpRawDumper);
}

void DabRadio::_slot_handle_source_dump_button()
{
  if (!mIsRunning.load() || mIsScanning.load())
  {
    return;
  }

  if (mpRawDumper != nullptr)
  {
    stop_source_dumping();
  }
  else
  {
    start_source_dumping();
  }
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

void DabRadio::stop_audio_dumping()
{
  if (mAudioDumpState == EAudioDumpState::Stopped)
  {
    return;
  }

  LOG("Audio dump stops", "");
  mAudioDumpState = EAudioDumpState::Stopped;
  mWavWriter.close();
  mpTechDataWidget->slot_audio_dump_button_text("Dump WAV", 10);
}

void DabRadio::start_audio_dumping()
{
  if (mAudioDumpState != EAudioDumpState::Stopped)
  {
    return;
  }

  mAudioWavDumpFileName = mOpenFileDialog.get_audio_dump_file_name(mChannel.currentService.serviceName);
  if (mAudioWavDumpFileName.isEmpty())
  {
    return;
  }
  LOG("Audio dump starts ", ui->serviceLabel->text());
  mpTechDataWidget->slot_audio_dump_button_text("Recording", 12);
  mAudioDumpState = EAudioDumpState::WaitForInit;
}

void DabRadio::_slot_handle_frame_dump_button()
{
  if (!mIsRunning.load() || mIsScanning.load())
  {
    return;
  }

  if (mChannel.currentService.frameDumper != nullptr)
  {
    stop_frame_dumping();
  }
  else
  {
    start_frame_dumping();
  }
}

void DabRadio::stop_frame_dumping()
{
  if (mChannel.currentService.frameDumper == nullptr)
  {
    return;
  }

  fclose(mChannel.currentService.frameDumper);
  mpTechDataWidget->slot_frame_dump_button_text("Dump AAC", 10);
  mChannel.currentService.frameDumper = nullptr;
}

void DabRadio::start_frame_dumping()
{
  // TODO: what is with MP2 data streams? here only AAC is considered as filename
  mChannel.currentService.frameDumper = mOpenFileDialog.open_frame_dump_file_ptr(mChannel.currentService.serviceName);

  if (mChannel.currentService.frameDumper == nullptr)
  {
    return;
  }

  mpTechDataWidget->slot_frame_dump_button_text("Recording", 12);
}

//	called from the mp4 handler, using a signal
void DabRadio::slot_new_frame(int amount)
{
  auto * const buffer = make_vla(u8, amount);
  if (!mIsRunning.load())
  {
    return;
  }

  if (mChannel.currentService.frameDumper == nullptr)
  {
    mpFrameBuffer->flush_ring_buffer();
  }
  else
  {
    while (mpFrameBuffer->get_ring_buffer_read_available() >= amount)
    {
      mpFrameBuffer->get_data_from_ring_buffer(buffer, amount);
      if (mChannel.currentService.frameDumper != nullptr)
      {
        fwrite(buffer, amount, 1, mChannel.currentService.frameDumper);
      }
    }
  }
}

void DabRadio::_slot_handle_spectrum_button()
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (mSpectrumViewer.is_hidden())
  {
    mSpectrumViewer.show();
  }
  else
  {
    mSpectrumViewer.hide();
  }
  Settings::SpectrumViewer::varUiVisible.write(!mSpectrumViewer.is_hidden());
}

void DabRadio::connect_dab_processor()
{
  //	we avoided till now connecting the channel selector
  //	to the slot since that function does a lot more, things we
  //	do not want here
  connect(ui->cmbChannelSelector, &QComboBox::textActivated, this, &DabRadio::_slot_handle_channel_selector);
  connect(&mSpectrumViewer, &SpectrumViewer::signal_cb_nom_carrier_changed, mpDabProcessor.get(), &DabProcessor::slot_show_nominal_carrier);
  connect(&mSpectrumViewer, &SpectrumViewer::signal_cmb_carrier_changed, mpDabProcessor.get(), &DabProcessor::slot_select_carrier_plot_type);
  connect(&mSpectrumViewer, &SpectrumViewer::signal_cmb_iqscope_changed, mpDabProcessor.get(), &DabProcessor::slot_select_iq_plot_type);
  connect(mConfig.cmbSoftBitGen, qOverload<int>(&QComboBox::currentIndexChanged), mpDabProcessor.get(), [this](int idx) { mpDabProcessor->slot_soft_bit_gen_type((ESoftBitType)idx); });

  //	Just to be sure we disconnect here.
  //	It would have been helpful to have a function
  //	testing whether or not a connection exists, we need a kind
  //	of "reset"
  disconnect(mConfig.deviceSelector, &QComboBox::textActivated, this, &DabRadio::_slot_do_start);
  disconnect(mConfig.deviceSelector, &QComboBox::textActivated, this, &DabRadio::_slot_new_device);
  connect(mConfig.deviceSelector, &QComboBox::textActivated, this, &DabRadio::_slot_new_device);
}
  //	When changing (or setting) a device, we do not want anybody
//	to have the buttons on the GUI touched, so
//	we just disconnect them and (re)connect them as soon as
//	a device is operational
void DabRadio::connect_gui()
{
  connect(mConfig.contentButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_content_button);
  connect(ui->btnTechDetails, &QPushButton::clicked, this, &DabRadio::_slot_handle_tech_detail_button);
  connect(mConfig.resetButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_reset_button);
  connect(ui->btnScanning, &QPushButton::clicked, this, &DabRadio::_slot_handle_scan_button);
  connect(ui->btnSpectrumScope, &QPushButton::clicked, this, &DabRadio::_slot_handle_spectrum_button);
  connect(ui->btnDeviceWidget, &QPushButton::clicked, this, &DabRadio::_slot_handle_device_widget_button);
  connect(mConfig.dumpButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_source_dump_button);
  connect(ui->btnPrevService, &QPushButton::clicked, this, &DabRadio::_slot_handle_prev_service_button);
  connect(ui->btnNextService, &QPushButton::clicked, this, &DabRadio::_slot_handle_next_service_button);
  connect(ui->btnTargetService, &QPushButton::clicked, this, &DabRadio::_slot_handle_target_service_button);
  connect(mpTechDataWidget.get(), &TechData::signal_handle_audioDumping, this, &DabRadio::_slot_handle_audio_dump_button);
  connect(mpTechDataWidget.get(), &TechData::signal_handle_frameDumping, this, &DabRadio::_slot_handle_frame_dump_button);
  connect(ui->btnMuteAudio, &QPushButton::clicked, this, &DabRadio::_slot_handle_mute_button);
  //connect(ensembleDisplay, &QListView::clicked, this, &RadioInterface::_slot_select_service);
  connect(mConfig.skipList_button, &QPushButton::clicked, this, &DabRadio::_slot_handle_skip_list_button);
  connect(mConfig.skipFile_button, &QPushButton::clicked, this, &DabRadio::_slot_handle_skip_file_button);
  connect(mpServiceListHandler.get(), &ServiceListHandler::signal_selection_changed, this, &DabRadio::_slot_service_changed);
  connect(mpServiceListHandler.get(), &ServiceListHandler::signal_favorite_status, this, &DabRadio::_slot_favorite_changed);
  connect(ui->btnToggleFavorite, &QPushButton::clicked, this, &DabRadio::_slot_handle_favorite_button);
  connect(ui->btnTii, &QPushButton::clicked, this, &DabRadio::_slot_handle_tii_button);
  connect(ui->btnCir, &QPushButton::clicked, this, &DabRadio::_slot_handle_cir_button);
}

void DabRadio::disconnect_gui()
{
  disconnect(mConfig.contentButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_content_button);
  disconnect(ui->btnTechDetails, &QPushButton::clicked, this, &DabRadio::_slot_handle_tech_detail_button);
  disconnect(mConfig.resetButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_reset_button);
  disconnect(ui->btnScanning, &QPushButton::clicked, this, &DabRadio::_slot_handle_scan_button);
  disconnect(ui->btnSpectrumScope, &QPushButton::clicked, this, &DabRadio::_slot_handle_spectrum_button);
  disconnect(ui->btnDeviceWidget, &QPushButton::clicked, this, &DabRadio::_slot_handle_device_widget_button);
  disconnect(mConfig.dumpButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_source_dump_button);
  disconnect(ui->btnPrevService, &QPushButton::clicked, this, &DabRadio::_slot_handle_prev_service_button);
  disconnect(ui->btnNextService, &QPushButton::clicked, this, &DabRadio::_slot_handle_next_service_button);
  disconnect(ui->btnTargetService, &QPushButton::clicked, this, &DabRadio::_slot_handle_target_service_button);
  disconnect(mpTechDataWidget.get(), &TechData::signal_handle_audioDumping, this, &DabRadio::_slot_handle_audio_dump_button);
  disconnect(mpTechDataWidget.get(), &TechData::signal_handle_frameDumping, this, &DabRadio::_slot_handle_frame_dump_button);
  disconnect(ui->btnMuteAudio, &QPushButton::clicked, this, &DabRadio::_slot_handle_mute_button);
  //disconnect(ensembleDisplay, &QListView::clicked, this, &RadioInterface::_slot_select_service);
  disconnect(mConfig.skipList_button, &QPushButton::clicked, this, &DabRadio::_slot_handle_skip_list_button);
  disconnect(mConfig.skipFile_button, &QPushButton::clicked, this, &DabRadio::_slot_handle_skip_file_button);
  disconnect(mpServiceListHandler.get(), &ServiceListHandler::signal_selection_changed, this, &DabRadio::_slot_service_changed);
  disconnect(mpServiceListHandler.get(), &ServiceListHandler::signal_favorite_status, this, &DabRadio::_slot_favorite_changed);
  disconnect(ui->btnToggleFavorite, &QPushButton::clicked, this, &DabRadio::_slot_handle_favorite_button);
  disconnect(ui->btnTii, &QPushButton::clicked, this, &DabRadio::_slot_handle_tii_button);
  disconnect(ui->btnCir, &QPushButton::clicked, this, &DabRadio::_slot_handle_cir_button);
}

void DabRadio::closeEvent(QCloseEvent * event)
{
  if (Settings::Config::cbCloseDirect.read().toBool())
  {
    _slot_terminate_process();
    event->accept();
    return;
  }

  QMessageBox::StandardButton resultButton = QMessageBox::question(this, PRJ_NAME, "Quitting " PRJ_NAME "\n\nAre you really sure?\n\n(You can switch-off this message in the configuration settings)",
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

void DabRadio::slot_start_announcement(const QString & name, int subChId)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (name == ui->serviceLabel->text())
  {
    _set_status_info_status(mStatusInfo.Announce, true);
    //serviceLabel->setStyleSheet("color : yellow");
    fprintf(stdout, "Announcement for %s (%d) starts\n", name.toUtf8().data(), subChId);
  }
}

void DabRadio::slot_stop_announcement(const QString & name, int subChId)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (name == ui->serviceLabel->text())
  {
    _set_status_info_status(mStatusInfo.Announce, false);
    fprintf(stdout, "Announcement for %s (%d) stops\n", name.toUtf8().data(), subChId);
  }
}

// called from content widget when only a service has changed
void DabRadio::_slot_select_service(QModelIndex ind)
{
  if (!mIsScanning)
  {
    const QString currentProgram = ind.data(Qt::DisplayRole).toString();
    local_select(mChannel.channelName, currentProgram);
  }
}

// called from the service list handler when channel and service has changed
void DabRadio::_slot_service_changed(const QString & iChannel, const QString & iService)
{
  if (!mIsScanning)
  {
    local_select(iChannel, iService);
  }
}

// called from the service list handler when when the favorite state has changed
void DabRadio::_slot_favorite_changed(const bool iIsFav)
{
  mCurFavoriteState = iIsFav;
  set_favorite_button_style();
}

//	selecting from a content description
void DabRadio::slot_handle_content_selector(const QString & s)
{
  local_select(mChannel.channelName, s);
}

void DabRadio::local_select(const QString & iChannel, const QString & iService)
{
  if (mpDabProcessor == nullptr)  // should not happen
  {
    return;
  }

  stop_service(mChannel.currentService);

  QString serviceName = iService;
  serviceName.resize(16, ' '); // fill up to 16 spaces

  // Is it only a service change within the same channel?
  if (iChannel == mChannel.channelName)
  {
    mChannel.currentService.valid = false;
    SDabService s;
    mpDabProcessor->get_parameters(serviceName, &s.SId, &s.SCIds);
    if (s.SId == 0)
    {
      write_warning_message("Insufficient data for this program (1)");
      return;
    }
    s.serviceName = iService; // TODO: service or serviceName?
    start_service(s);
    return;
  }

  // The hard part is stopping the current service, quitting the current channel, selecting a new channel, waiting a while
  stop_channel();
  //      and start the new channel first
  int k = ui->cmbChannelSelector->findText(iChannel);
  if (k != -1)
  {
    _update_channel_selector(k);
  }
  else
  {
    QMessageBox::warning(this, tr("Warning"), tr("Incorrect preset\n"));
    return;
  }

  // Prepare the service, start the new channel and wait
  mChannel.nextService.valid = true;
  mChannel.nextService.channel = iChannel;
  mChannel.nextService.serviceName = serviceName;
  mChannel.nextService.SId = 0;
  mChannel.nextService.SCIds = 0;

  // mPresetTimer.start(cPresetTimeoutMs);

  start_channel(ui->cmbChannelSelector->currentText());
}

////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////

void DabRadio::stop_service(SDabService & ioDabService)
{
  mPresetTimer.stop();
  mChannelTimer.stop();

  if (mpDabProcessor == nullptr)
  {
    fprintf(stderr, "Expert error 22\n");
    return;
  }

  //	stop "dumpers"
  if (mChannel.currentService.frameDumper != nullptr)
  {
    stop_frame_dumping();
    mChannel.currentService.frameDumper = nullptr;
  }

  if (mAudioDumpState != EAudioDumpState::Stopped)
  {
    stop_audio_dumping();
  }

  mpTechDataWidget->cleanUp(); //	and clean up the technical widget
  clean_up(); //	and clean up this main widget

  //	stop "secondary services" - if any - as well
  if (ioDabService.valid)
  {
    mpDabProcessor->stop_service(ioDabService.subChId, FORE_GROUND);

    if (ioDabService.is_audio)
    {
      emit signal_stop_audio();

      for (i32 i = 0; i < 5; ++i) // TODO: from where the 5?
      {
        Packetdata pd;
        mpDabProcessor->get_data_for_packet_service(ioDabService.serviceName, &pd, i);

        if (pd.defined)
        {
          mpDabProcessor->stop_service(pd.subchId, FORE_GROUND);
          break;
        }
      }
    }

    ioDabService.valid = false;
  }

  show_pause_slide();
  clean_screen();
}

void DabRadio::start_service(SDabService & s)
{
  QString serviceName = s.serviceName;

  mChannel.currentService = s;
  mChannel.currentService.frameDumper = nullptr;
  mChannel.currentService.valid = false;
  LOG("start service ", serviceName.toUtf8().data());
  LOG("service has SNR ", QString::number(mChannel.snr));

  mpServiceListHandler->set_selector(mChannel.channelName, serviceName);

  //	and display the servicename on the serviceLabel
  QFont font = ui->serviceLabel->font();
  font.setPointSize(16);
  font.setBold(true);
  ui->serviceLabel->setStyleSheet("QLabel { color: lightblue; }");
  ui->serviceLabel->setFont(font);
  ui->serviceLabel->setText(serviceName);
  ui->lblDynLabel->setText("");
  //lblDynLabel->setOpenExternalLinks(true); // TODO: make this work (Bayern1 sent link)

  Audiodata ad;
  mpDabProcessor->get_data_for_audio_service(serviceName, &ad);

  if (ad.defined)
  {
    mChannel.currentService.valid = true;
    mChannel.currentService.is_audio = true;
    mChannel.currentService.subChId = ad.subchId;
    if (mpDabProcessor->has_time_table(ad.SId))
    {
      mpTechDataWidget->slot_show_timetableButton(true);
    }

    start_audio_service(&ad);
    const QString csn = mChannel.channelName + ":" + serviceName;
    Settings::Main::varPresetName.write(csn);

#ifdef HAVE_PLUTO_RXTX
    if (streamerOut != nullptr)
    {
      streamerOut->addRds(std::string(serviceName.toUtf8().data()));
    }
#endif
  }
  else if (mpDabProcessor->is_packet_service(serviceName))
  {
    Packetdata pd;
    mpDabProcessor->get_data_for_packet_service(serviceName, &pd, 0);
    mChannel.currentService.valid = true;
    mChannel.currentService.is_audio = false;
    mChannel.currentService.subChId = pd.subchId;
    start_packet_service(serviceName);
  }
  else
  {
    write_warning_message("Insufficient data for this program (2)");
    Settings::Main::varPresetName.write("");
  }
}

void DabRadio::start_audio_service(const Audiodata * const ipAD)
{
  mChannel.currentService.valid = true;

  (void)mpDabProcessor->set_audio_channel(ipAD, mpAudioBufferFromDecoder, nullptr, FORE_GROUND);

  for (int i = 1; i < 10; i++)
  {
    Packetdata pd;
    mpDabProcessor->get_data_for_packet_service(ipAD->serviceName, &pd, i);
    if (pd.defined)
    {
      mpDabProcessor->set_data_channel(&pd, mpDataBuffer, FORE_GROUND);
      fprintf(stdout, "adding %s (%d) as subservice\n", pd.serviceName.toUtf8().data(), pd.subchId);
      break;
    }
  }
  //	activate sound
  mpCurAudioFifo = nullptr; // trigger possible new sample rate setting
  //mpSoundOut->restart();
  ui->programTypeLabel->setText(getProgramType(ipAD->programType));
  //	show service related data
  mpTechDataWidget->show_serviceData(ipAD);
  _set_status_info_status(mStatusInfo.InpBitRate, (i32)(ipAD->bitRate));
}

void DabRadio::start_packet_service(const QString & iS)
{
  Packetdata pd;
  mpDabProcessor->get_data_for_packet_service(iS, &pd, 0);

  if ((!pd.defined) || (pd.DSCTy == 0) || (pd.bitRate == 0))
  {
    write_warning_message("Insufficient data for this program (3)");
    return;
  }

  if (!mpDabProcessor->set_data_channel(&pd, mpDataBuffer, FORE_GROUND))
  {
    QMessageBox::warning(this, tr("sdr"), tr("could not start this service\n"));
    return;
  }

  switch (pd.DSCTy)
  {
  case 5:
    fprintf(stdout, "selected apptype %d\n", pd.appType);
    write_warning_message("Transport channel not implemented");
    break;
  case 60:
    write_warning_message("MOT not supported");
    break;
  case 59:
    write_warning_message("Embedded IP not supported");
    break;
  case 44:
    write_warning_message("Journaline");
    break;
  default:
    write_warning_message("Unimplemented protocol");
  }
}

//	This function is only used in the Gui to clear
//	the details of a selected service
void DabRadio::clean_screen()
{
  ui->serviceLabel->setText("");
  ui->lblDynLabel->setText("");
  mpTechDataWidget->cleanUp();
  ui->programTypeLabel->setText("");
  mpTechDataWidget->cleanUp();
  _reset_status_info();
}

////////////////////////////////////////////////////////////////////////////
//
//	next and previous service selection
////////////////////////////////////////////////////////////////////////////

void DabRadio::_slot_handle_prev_service_button()
{
  mpServiceListHandler->jump_entries(-1);
//  disconnect(btnPrevService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_prev_service_button);
//  handle_serviceButton(BACKWARDS);
//  connect(btnPrevService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_prev_service_button);
}

void DabRadio::_slot_handle_next_service_button()
{
  mpServiceListHandler->jump_entries(+1);
}

void DabRadio::_slot_handle_target_service_button()
{
  mpServiceListHandler->jump_entries(0);
}


////////////////////////////////////////////////////////////////////////////
//
//	The user(s)
///////////////////////////////////////////////////////////////////////////
//
//	setPresetService () is called after a time out to
//	actually start the service that we were waiting for
//	Assumption is that the channel is set, and the servicename
//	is to be found in "nextService"
void DabRadio::_slot_preset_timeout()
{
  qCDebug(sLogRadioInterface) << Q_FUNC_INFO;

  if (!mIsRunning.load())
  {
    return;
  }

  mPresetTimer.stop();
  stop_scanning();

  if (!mChannel.nextService.valid)
  {
    return;
  }

  if (mChannel.nextService.channel != mChannel.channelName)
  {
    return;
  }

  if (mChannel.Eid == 0)
  {
    write_warning_message("Ensemble not yet recognized");
    return;
  }

  // crosscheck service list if a non more existing service is stored there
  QStringList serviceList;
  for (const auto & sl : mServiceList)
  {
    serviceList << sl.name;
  }
  mpServiceListHandler->delete_not_existing_services_at_channel(mChannel.channelName, serviceList);

  QString presetName = mChannel.nextService.serviceName;
  presetName.resize(16, ' '); // fill up to 16 spaces

  SDabService s;
  s.serviceName = presetName;
  mpDabProcessor->get_parameters(presetName, &s.SId, &s.SCIds);

  if (s.SId == 0)
  {
    write_warning_message("Insufficient data for this program (5)");
    return;
  }

  mChannel.nextService.valid = false;
  start_service(s);
}

void DabRadio::write_warning_message(const QString & iMsg)
{
  ui->lblDynLabel->setStyleSheet("color: #ff9100");
  ui->lblDynLabel->setText(iMsg);
}

///////////////////////////////////////////////////////////////////////////
//
//	Channel basics
///////////////////////////////////////////////////////////////////////////
//	Precondition: no channel should be active
//
void DabRadio::start_channel(const QString & iChannel)
{
  const i32 tunedFrequencyHz = mChannel.realChannel ? mBandHandler.get_frequency_Hz(iChannel) : mpInputDevice->getVFOFrequency();
  LOG("channel starts ", iChannel);
  mSpectrumViewer.show_nominal_frequency_MHz((f32)tunedFrequencyHz / 1'000'000.0f);
  mpInputDevice->resetBuffer();
  mServiceList.clear();
  mpInputDevice->restartReader(tunedFrequencyHz);
  usleep(250'000); // wait for the reader to start as some LOs on some devices need some more time to swing-in
  mChannel.clean_channel();
  mChannel.channelName = mChannel.realChannel ? iChannel : "File";
  mChannel.nominalFreqHz = tunedFrequencyHz;

  mpServiceListHandler->set_selector_channel_only(mChannel.channelName);

  SCacheElem theTransmitter;
  if (mpHttpHandler != nullptr)
  {
    theTransmitter.latitude = 0;
    theTransmitter.longitude = 0;
    mpHttpHandler->putData(MAP_RESET, &theTransmitter, "", 0, 0, 0, false);
  }
  mTransmitterIds.clear();

  enable_ui_elements_for_safety(!mIsScanning);

  mpDabProcessor->start();

  if (!mIsScanning)
  {
    Settings::Main::varChannel.write(iChannel);
    mEpgTimer.start(cEpgTimeoutMs);
  }
}

//
//	apart from stopping the reader, a lot of administration
//	is to be done.
void DabRadio::stop_channel()
{
  if (mpInputDevice == nullptr) // should not happen
  {
    return;
  }
  stop_etiHandler();
  LOG("channel stops ", mChannel.channelName);

  // first, stop services in fore and background
  if (mChannel.currentService.valid)
  {
    stop_service(mChannel.currentService);
  }

  for (const auto & bgs : mChannel.backgroundServices)
  {
    mpDabProcessor->stop_service(bgs.subChId, BACK_GROUND);
    if (bgs.fd != nullptr)
    {
      fclose(bgs.fd);
    }
  }
  mChannel.backgroundServices.clear();

  stop_source_dumping();
  emit signal_stop_audio();

  _show_epg_label(false);

  if (mpContentTable != nullptr)
  {
    mpContentTable->hide();
    delete mpContentTable;
    mpContentTable = nullptr;
  }

  //	note framedumping - if any - was already stopped
  //	ficDumping - if on - is stopped here
  if (mpFicDumpPointer != nullptr)
  {
    mpDabProcessor->stop_fic_dump();
    mpFicDumpPointer = nullptr;
  }

  mEpgTimer.stop();
  mpInputDevice->stopReader();
  mpDabProcessor->stop();
  usleep(1000);
  mpTechDataWidget->cleanUp();

  show_pause_slide();

  mPresetTimer.stop();
  mChannelTimer.stop();
  mChannel.clean_channel();
  mChannel.targetPos = cf32(0, 0);
  if (mpHttpHandler != nullptr)
  {
    SCacheElem theTransmitter;
    theTransmitter.latitude = 0;
    theTransmitter.longitude = 0;
    mpHttpHandler->putData(MAP_RESET, &theTransmitter, "", 0, 0, 0, false);
  }
  ui->transmitter_country->setText("");
  ui->transmitter_coordinates->setText("");

  enable_ui_elements_for_safety(false);  // hide some buttons
  QCoreApplication::processEvents();

  //	no processing left at this time
  usleep(1000);    // may be handling pending signals?
  mChannel.currentService.valid = false;
  mChannel.nextService.valid = false;

  //	all stopped, now look at the GUI elements
  ui->progBarFicError->setValue(0);
  //	the visual elements related to service and channel
  ui->ensembleId->setText("");
  ui->transmitter_coordinates->setText(" ");

  mServiceList.clear();
  clean_screen();
  _show_epg_label(false);
  ui->cmbTiiList->clear();
  mTiiIndex = 0;
}

//
/////////////////////////////////////////////////////////////////////////
//
//	next- and previous channel buttons
/////////////////////////////////////////////////////////////////////////

void DabRadio::_slot_handle_channel_selector(const QString & channel)
{
  if (!mIsRunning.load())
  {
    return;
  }

  LOG("select channel ", channel.toUtf8().data());
  mPresetTimer.stop();
  stop_scanning();
  stop_channel();
  start_channel(channel);
}

////////////////////////////////////////////////////////////////////////
//
//	scanning
/////////////////////////////////////////////////////////////////////////

void DabRadio::_slot_handle_scan_button()
{
  if (!mIsRunning.load())
  {
    return;
  }
  if (mIsScanning.load())
  {
    stop_scanning();
  }
  else
  {
    start_scanning();
  }
}

void DabRadio::start_scanning()
{
  mPresetTimer.stop();
  mChannelTimer.stop();
  mEpgTimer.stop();
  connect(mpDabProcessor.get(), &DabProcessor::signal_no_signal_found, this, &DabRadio::slot_no_signal_found);
  stop_channel();

  mScanResult = {};

  const int cc = mBandHandler.firstChannel();
  LOG("scanning starts with ", QString::number(cc));
  mIsScanning.store(true);

  // if (my_scanTable == nullptr)
  // {
  //   my_scanTable = new ContentTable(this, dabSettings, "scan", my_dabProcessor->scanWidth());
  // }
  // else
  // {
  //   my_scanTable->clearTable();
  // }

  mpServiceListHandler->set_data_mode(ServiceListHandler::EDataMode::Permanent);
  mpServiceListHandler->delete_table(false);
  mpServiceListHandler->create_new_table();

  // QString topLine = QString("ensemble") + ";" + "channelName" + ";" + "frequency (KHz)" + ";" + "Eid" + ";" + "time" + ";" + "tii" + ";" + "SNR" + ";" + "nr services" + ";";
  // my_scanTable->addLine(topLine);
  // my_scanTable->addLine("\n");

  mpDabProcessor->set_scan_mode(true); // avoid MSC activities
  //  To avoid reaction of the system on setting a different value:
  _update_channel_selector(cc);
  ui->lblDynLabel->setText(_get_scan_message(false));
  ui->btnScanning->start_animation();
  mChannelTimer.start(cChannelTimeoutMs);

  start_channel(ui->cmbChannelSelector->currentText());
}

//	stop_scanning is called
//	1. when the scanbutton is touched during scanning
//	2. on user selection of a service or a channel select
//	3. on device selection
//	4. on handling a reset
void DabRadio::stop_scanning()
{
  disconnect(mpDabProcessor.get(), &DabProcessor::signal_no_signal_found, this, &DabRadio::slot_no_signal_found);

  if (mIsScanning.load())
  {
    ui->btnScanning->stop_animation();
    LOG("scanning stops ", "");
    mpDabProcessor->set_scan_mode(false);
    ui->lblDynLabel->setText(_get_scan_message(true));
    mChannelTimer.stop();
    enable_ui_elements_for_safety(true);
    mChannel.channelName = ""; // trigger restart of channel after next service list click
    mIsScanning.store(false);
    mpServiceListHandler->restore_favorites(); // try to restore former favorites from a backup table
  }
}

//	If the ofdm processor has waited - without success -
//	for a period of N frames to get a start of a synchronization,
//	it sends a signal to the GUI handler
//	If "scanning" is "on" we hop to the next frequency on
//	the list.
//	Also called as a result of time out on channelTimer

void DabRadio::slot_no_signal_found()
{
  mChannelTimer.stop();

  if (mIsRunning.load() && mIsScanning.load())
  {
    int cc = ui->cmbChannelSelector->currentIndex();
    // if (!mServiceList.empty())
    // {
    //   showServices();
    // }
    stop_channel();
    cc = mBandHandler.nextChannel(cc);

    // qInfo() << "going to channel" << cc;

    if (cc >= ui->cmbChannelSelector->count())
    {
      stop_scanning();
    }
    else
    {
      //	To avoid reaction of the system on setting a different value:
      _update_channel_selector(cc);

      ui->lblDynLabel->setText(_get_scan_message(false));
      mChannelTimer.start(cChannelTimeoutMs);
      start_channel(ui->cmbChannelSelector->currentText());
    }
  }
  else if (mIsScanning.load())
  {
    stop_scanning();
  }
}

////////////////////////////////////////////////////////////////////////////
//
// showServices
////////////////////////////////////////////////////////////////////////////

// void RadioInterface::showServices() const
// {
//   // int scanMode = mConfig.scanmodeSelector->currentIndex();
//   QString SNR = "SNR " + QString::number(mChannel.snr);
//
//   if (mpDabProcessor == nullptr)
//   {  // cannot happen
//     fprintf(stderr, "Expert error 26\n");
//     return;
//   }
//
//   QString utcTime = convertTime(mUTC.year, mUTC.month, mUTC.day, mUTC.hour, mUTC.minute);
//   QString headLine = mChannel.ensembleName + ";" + mChannel.channelName + ";" + QString::number(mChannel.nominalFreqHz / 1000) + ";"
//                    + hex_to_str(mChannel.Eid) + " " + ";" + transmitter_coordinates->text() + " " + ";" + utcTime + ";" + SNR + ";"
//                    + QString::number(mServiceList.size()) + ";" + lblStationLocation->text();
//   QStringList s = mpDabProcessor->basicPrint();
//
//   qInfo() << headLine;
//   for (const auto & i : s)
//   {
//     qInfo() << i;
//   }
//
//   // my_scanTable->addLine(headLine);
//   // my_scanTable->addLine("\n;\n");
//   // for (const auto & i : s)
//   // {
//   //   my_scanTable->addLine(i);
//   // }
//   // my_scanTable->addLine("\n;\n;\n");
//   // my_scanTable->show();
// }

/////////////////////////////////////////////////////////////////////
//
std::vector<SServiceId> DabRadio::insert_sorted(const std::vector<SServiceId> & iList, const SServiceId & iEntry)
{
  std::vector<SServiceId> oList;

  if (iList.empty())
  {
    oList.push_back(iEntry);
    return oList;
  }
  QString baseS = "";

  bool inserted = false;
  for (const auto & serv: iList)
  {
    if (!inserted)
    {
      if ((baseS < iEntry.name.toUpper()) && (iEntry.name.toUpper() < serv.name.toUpper()))
      {
        oList.push_back(iEntry);
        inserted = true;
      }
    }
    baseS = serv.name.toUpper();
    oList.push_back(serv);
  }

  if (!inserted)
  {
    oList.push_back(iEntry);
  }
  return oList;
}


//	In those case we are sure not to have an operating dabProcessor, we hide some buttons
void DabRadio::enable_ui_elements_for_safety(const bool iEnable)
{
  mConfig.dumpButton->setEnabled(iEnable);
  ui->btnToggleFavorite->setEnabled(iEnable);
  ui->btnPrevService->setEnabled(iEnable);
  ui->btnNextService->setEnabled(iEnable);
  ui->cmbChannelSelector->setEnabled(iEnable);
  mConfig.contentButton->setEnabled(iEnable);
}

void DabRadio::_slot_handle_mute_button()
{
  _slot_update_mute_state(!mMutingActive);
}

void DabRadio::_slot_update_mute_state(const bool iMute)
{
  mMutingActive = iMute;
  ui->btnMuteAudio->setIcon(QIcon(mMutingActive ? ":res/icons/muted24.png" : ":res/icons/unmuted24.png"));
  ui->btnMuteAudio->setIconSize(QSize(24, 24));
  ui->btnMuteAudio->setFixedSize(QSize(32, 32));
  signal_audio_mute(mMutingActive);
}

void DabRadio::_update_channel_selector(int index)
{
  if (ui->cmbChannelSelector->currentIndex() == index)
  {
    return;
  }

  // TODO: why is that so complicated?
  disconnect(ui->cmbChannelSelector, &QComboBox::textActivated, this, &DabRadio::_slot_handle_channel_selector);
  ui->cmbChannelSelector->blockSignals(true);
  emit signal_set_new_channel(index);

  while (ui->cmbChannelSelector->currentIndex() != index)
  {
    usleep(2000);
  }
  ui->cmbChannelSelector->blockSignals(false);
  connect(ui->cmbChannelSelector, &QComboBox::textActivated, this, &DabRadio::_slot_handle_channel_selector);
}


/////////////////////////////////////////////////////////////////////////
//	External configuration items				//////

//-------------------------------------------------------------------------
//------------------------------------------------------------------------
//
//	if configured, the interpretation of the EPG data starts automatically,
//	the servicenames of an SPI/EPG service may differ from one country
//	to another
void DabRadio::slot_epg_timer_timeout()
{
  mEpgTimer.stop();

  // TODO: there is a public switch for this missing?!
  // if (!mpSH->read(SettingHelper::epgFlag).toBool())
  {
    return;
  }

  if (mIsScanning.load())
  {
    return;
  }

  for (const auto & serv: mServiceList)
  {
    if (serv.name.contains("-EPG ", Qt::CaseInsensitive) ||
        serv.name.contains(" EPG   ", Qt::CaseInsensitive) ||
        serv.name.contains("Spored", Qt::CaseInsensitive) ||
        serv.name.contains("NivaaEPG", Qt::CaseInsensitive) ||
        serv.name.contains("SPI", Qt::CaseSensitive) ||
        serv.name.contains("BBC Guide", Qt::CaseInsensitive) ||
        serv.name.contains("EPG_", Qt::CaseInsensitive) ||
        serv.name.startsWith("EPG ", Qt::CaseInsensitive))
    {
      Packetdata pd;
      mpDabProcessor->get_data_for_packet_service(serv.name, &pd, 0);

      if ((!pd.defined) || (pd.DSCTy == 0) || (pd.bitRate == 0))
      {
        continue;
      }

      if (pd.DSCTy == 60)
      {
        LOG("hidden service started ", serv.name);
        _show_epg_label(true);
        fprintf(stdout, "Starting hidden service %s\n", serv.name.toUtf8().data());
        mpDabProcessor->set_data_channel(&pd, mpDataBuffer, BACK_GROUND);
        SDabService s;
        s.channel = pd.channel;
        s.serviceName = pd.serviceName;
        s.SId = pd.SId;
        s.SCIds = pd.SCIds;
        s.subChId = pd.subchId;
        s.fd = nullptr;
        mChannel.backgroundServices.push_back(s);
      }
    }
#ifdef  __DABDATA__
    else
    {
      packetdata pd;
      my_dabProcessor->dataforPacketService(serv.name, &pd, 0);
      if ((pd.defined) && (pd.DSCTy == 59))
      {
        LOG("hidden service started ", serv.name);
        _show_epg_label(true);
        fprintf(stdout, "Starting hidden service %s\n", serv.name.toUtf8().data());
        my_dabProcessor->set_dataChannel(&pd, &dataBuffer, BACK_GROUND);
        dabService s;
        s.channel = channel.channelName;
        s.serviceName = pd.serviceName;
        s.SId = pd.SId;
        s.SCIds = pd.SCIds;
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

u32 DabRadio::extract_epg(const QString& iName, const std::vector<SServiceId> & iServiceList, u32 ensembleId)
{
  (void)ensembleId;
  for (const auto & serv: iServiceList)
  {
    if (iName.contains(QString::number(serv.SId, 16), Qt::CaseInsensitive))
    {
      return serv.SId;
    }
  }
  return 0;
}

void DabRadio::slot_set_epg_data(int SId, int theTime, const QString & theText, const QString & theDescr)
{
  if (mpDabProcessor != nullptr)
  {
    mpDabProcessor->set_epg_data(SId, theTime, theText, theDescr);
  }
}

void DabRadio::_slot_handle_time_table()
{
  int epgWidth = 70; // comes only from ini file formerly
  if (!mChannel.currentService.valid || !mChannel.currentService.is_audio)
  {
    return;
  }

  mpTimeTable->clear();

  if (epgWidth < 50)
  {
    epgWidth = 50;
  }
  std::vector<SEpgElement> res = mpDabProcessor->find_epg_data(mChannel.currentService.SId);
  for (const auto & element: res)
  {
    mpTimeTable->addElement(element.theTime, epgWidth, element.theText, element.theDescr);
  }
}

void DabRadio::_slot_handle_skip_list_button()
{
  if (!mBandHandler.isHidden())
  {
    mBandHandler.hide();
  }
  else
  {
    mBandHandler.show();
  }
}

void DabRadio::_slot_handle_skip_file_button()
{
  const QString fileName = mOpenFileDialog.get_skip_file_file_name();
  mBandHandler.saveSettings();
  mBandHandler.setup_skipList(fileName);
}

void DabRadio::slot_use_strongest_peak(bool iIsChecked)
{
  assert(mpDabProcessor != nullptr);
  mpDabProcessor->set_sync_on_strongest_peak(iIsChecked);
  //mChannel.transmitters.clear();
}

void DabRadio::slot_handle_dc_avoidance_algorithm(bool iIsChecked)
{
  assert(mpDabProcessor != nullptr);
  mpDabProcessor->set_dc_avoidance_algorithm(iIsChecked);
}

void DabRadio::slot_handle_dc_removal(bool iIsChecked)
{
  assert(mpDabProcessor != nullptr);
  mpDabProcessor->set_dc_removal(iIsChecked);
}

void DabRadio::slot_handle_tii_collisions(bool iIsChecked)
{
  assert(mpDabProcessor != nullptr);
  mpDabProcessor->set_tii_collisions(iIsChecked);
}

void DabRadio::slot_handle_dl_text_button()
{
  if (mDlTextFile != nullptr)
  {
    fclose(mDlTextFile);
    mDlTextFile = nullptr;
    mConfig.dlTextButton->setText("dlText");
    return;
  }

  QString fileName = mOpenFileDialog.get_dl_text_file_name();
  mDlTextFile = fopen(fileName.toUtf8().data(), "w+");
  if (mDlTextFile == nullptr)
  {
    return;
  }
  mConfig.dlTextButton->setText("writing");
}

void DabRadio::_slot_handle_config_button()
{
  if (!mConfig.isHidden())
  {
    mConfig.hide();
  }
  else
  {
    mConfig.show();
  }
}

void DabRadio::slot_nr_services(int n)
{
  mChannel.serviceCount = n; // only a minor channel transmit this information, so do not rely on this
}

void DabRadio::LOG(const QString & a1, const QString & a2)
{
  if (mpLogFile == nullptr)
  {
    return;
  }

  QString theTime;
  if (Settings::Config::cbUseUtcTime.read().toBool())
  {
    theTime = convertTime(mUTC.year, mUTC.month, mUTC.day, mUTC.hour, mUTC.minute);
  }
  else
  {
    theTime = QDateTime::currentDateTime().toString();
  }

  fprintf(mpLogFile, "at %s: %s %s\n", theTime.toUtf8().data(), a1.toUtf8().data(), a2.toUtf8().data());
}

void DabRadio::slot_handle_logger_button(int)
{
  if (mConfig.cbActivateLogger->isChecked())
  {
    if (mpLogFile != nullptr)
    {
      fprintf(stderr, "should not happen (logfile)\n");
      return;
    }
    mpLogFile = mOpenFileDialog.open_log_file_ptr();
    if (mpLogFile != nullptr)
    {
      LOG("Log started with ", mpInputDevice->deviceName());
    }
    else
    {
      mConfig.cbActivateLogger->setCheckState(Qt::Unchecked); // "cancel" was chosen in file dialog
    }
  }
  else if (mpLogFile != nullptr)
  {
    fclose(mpLogFile);
    mpLogFile = nullptr;
  }
}

void DabRadio::slot_handle_set_coordinates_button()
{
  Coordinates theCoordinator;
  (void)theCoordinator.QDialog::exec();
  const f32 local_lat = Settings::Config::varLatitude.read().toFloat();
  const f32 local_lon = Settings::Config::varLongitude.read().toFloat();
  mChannel.localPos = cf32(local_lat, local_lon);
}

void DabRadio::slot_load_table()
{
  QString tableFile = Settings::Config::varTiiFile.read().toString();

  if (tableFile.isEmpty())
  {
    tableFile = QDir::homePath() + "/.txdata.tii";
    Settings::Config::varTiiFile.write(tableFile);
  }

  mTiiHandler.loadTable(tableFile);

  if (mTiiHandler.is_valid())
  {
    QMessageBox::information(this, tr("success"), tr("Loading and installing TII database completed\n"));
    mChannel.tiiFile = mTiiHandler.fill_cache_from_tii_file(tableFile);
  }
  else
  {
    QMessageBox::information(this, tr("fail"), tr("Loading database failed\n"));
    mChannel.tiiFile = false;
  }
}

//
//	ensure that we only get a handler if we have a start location
void DabRadio::_slot_handle_http_button()
{
  if (real(mChannel.localPos) == 0)
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
    mpHttpHandler = new HttpHandler(this,
                                    mapPort,
                                    browserAddress,
                                    mChannel.localPos,
                                    mapFile,
                                    Settings::Config::cbManualBrowserStart.read().toBool());
    mMaxDistance = -1;
    if (mpHttpHandler != nullptr)
    {
      _set_http_server_button(true);
    }
  }
  else
  {
    mMutex.lock();
    delete mpHttpHandler;
    mpHttpHandler = nullptr;
    mMutex.unlock();
    _set_http_server_button(false);
  }
}

//void RadioInterface::slot_http_terminate()
//{
//  locker.lock();
//  if (mapHandler != nullptr)
//  {
//    delete mapHandler;
//    mapHandler = nullptr;
//  }
//  locker.unlock();
//  btnHttpServer->setText("http");
//}

void DabRadio::show_pause_slide()
{
  // showSecondSlide is true if the 10th of seconds is even
  const bool showSecondSlide = (QDateTime::currentDateTime().time().second() / 10 & 0x1) == 0;
  const char * const picFile = (showSecondSlide ? ":res/logo/dabinvlogo.png" : ":res/logo/dabstar320x240.png");

  QPixmap p;
  if (p.load(picFile, "png"))
  {
    write_picture(p);
  }
}

void DabRadio::slot_handle_port_selector()
{
  MapPortHandler theHandler;
  (void)theHandler.QDialog::exec();
}

//////////////////////////////////////////////////////////////////////////
//	Experimental: handling eti
//	writing an eti file and scanning seems incompatible to me, so
//	that is why I use the button, originally named "btnScanning"
//	for eti when eti is prepared.
//	Preparing eti is with a checkbox on the configuration widget
//
/////////////////////////////////////////////////////////////////////////

void DabRadio::_slot_handle_eti_handler()
{
  if (mpDabProcessor == nullptr)
  {  // should not happen
    return;
  }

  if (mChannel.etiActive)
  {
    stop_etiHandler();
  }
  else
  {
    start_etiHandler();
  }
}

void DabRadio::stop_etiHandler()
{
  if (!mChannel.etiActive)
  {
    return;
  }

  mpDabProcessor->stop_eti_generator();
  mChannel.etiActive = false;
  ui->btnScanning->setText("ETI");
}

void DabRadio::start_etiHandler()
{
  if (mChannel.etiActive)
  {
    return;
  }

  QString etiFile = mOpenFileDialog.get_eti_file_name(mChannel.ensembleName, mChannel.channelName);
  if (etiFile == QString(""))
  {
    return;
  }
  LOG("etiHandler started", etiFile);
  mChannel.etiActive = mpDabProcessor->start_eti_generator(etiFile);
  if (mChannel.etiActive)
  {
    ui->btnScanning->setText("eti runs");
  }
}

void DabRadio::slot_handle_eti_active_selector(int /*k*/)
{
  if (mpInputDevice == nullptr)
  {
    return;
  }

  if (mConfig.cbActivateEti->isChecked())
  {
    stop_scanning();
    disconnect(ui->btnScanning, &QPushButton::clicked, this, &DabRadio::_slot_handle_scan_button);
    connect(ui->btnScanning, &QPushButton::clicked, this, &DabRadio::_slot_handle_eti_handler);
    ui->btnScanning->setText("ETI");

    if (mpInputDevice->isFileInput()) // restore the button's visibility
    {
      ui->btnScanning->show();
    }

    return;
  }
  //	otherwise, disconnect the eti handling and reconnect scan
  //	be careful, an ETI session may be going on
  stop_etiHandler();    // just in case
  disconnect(ui->btnScanning, &QPushButton::clicked, this, &DabRadio::_slot_handle_eti_handler);
  connect(ui->btnScanning, &QPushButton::clicked, this, &DabRadio::_slot_handle_scan_button);
  ui->btnScanning->setText("Scan");
  if (mpInputDevice->isFileInput())
  {  // hide the button now
    ui->btnScanning->hide();
  }
}

void DabRadio::slot_test_slider(int iVal) // iVal 0..1000
{
  emit signal_test_slider_changed(iVal);
}

QStringList DabRadio::get_soft_bit_gen_names()
{
  QStringList sl;

  // ATTENTION: use same sequence as in ESoftBitType
  sl << "Soft decision 1 "; // ESoftBitType::SOFTDEC1
  sl << "Soft decision 2"; // ESoftBitType::SOFTDEC2
  sl << "Soft decision 3"; // ESoftBitType::SOFTDEC3
  return sl;
}

QString DabRadio::get_bg_style_sheet(const QColor & iBgBaseColor, const char * const iWidgetType /*= nullptr*/)
{
  const f32 fac = 0.6f;
  const i32 r1 = iBgBaseColor.red();
  const i32 g1 = iBgBaseColor.green();
  const i32 b1 = iBgBaseColor.blue();
  const i32 r2 = (i32)((f32)r1 * fac);
  const i32 g2 = (i32)((f32)g1 * fac);
  const i32 b2 = (i32)((f32)b1 * fac);
  assert(r2 >= 0);
  assert(g2 >= 0);
  assert(b2 >= 0);
  QColor BgBaseColor2(r2, g2, b2);

  std::stringstream ts; // QTextStream did not work well here?!
  const char * const widgetType = (iWidgetType == nullptr ? "QPushButton" : iWidgetType);
  ts << widgetType << " { background-color: qlineargradient(x1:1, y1:0, x2:1, y2:1, stop:0 " << iBgBaseColor.name().toStdString()
     << ", stop:1 " << BgBaseColor2.name().toStdString() << "); }";
  //fprintf(stdout, "*** Style sheet: %s\n", ts.str().c_str());

  return { ts.str().c_str() };
}

void DabRadio::setup_ui_colors()
{
  ui->btnMuteAudio->setStyleSheet(get_bg_style_sheet({ 255, 60, 60 }));
  ui->btnScanning->setStyleSheet(get_bg_style_sheet({ 100, 100, 255 }));
  ui->btnDeviceWidget->setStyleSheet(get_bg_style_sheet({ 87, 230, 236 }));
  ui->configButton->setStyleSheet(get_bg_style_sheet({ 80, 155, 80 }));
  ui->btnTechDetails->setStyleSheet(get_bg_style_sheet({ 255, 255, 100 }));
  ui->btnSpectrumScope->setStyleSheet(get_bg_style_sheet({ 197, 69, 240 }));
  ui->btnPrevService->setStyleSheet(get_bg_style_sheet({ 200, 97, 40 }));
  ui->btnNextService->setStyleSheet(get_bg_style_sheet({ 200, 97, 40 }));
  ui->btnEject->setStyleSheet(get_bg_style_sheet({ 118, 60, 162 }));
  ui->btnTargetService->setStyleSheet(get_bg_style_sheet({ 33, 106, 105 }));
  ui->btnToggleFavorite->setStyleSheet(get_bg_style_sheet({ 100, 100, 255 }));
  ui->btnTii->setStyleSheet(get_bg_style_sheet({ 255, 100, 0 }));
  ui->btnCir->setStyleSheet(get_bg_style_sheet({ 220, 37, 192 }));

  _set_http_server_button(false);
}

void DabRadio::_set_http_server_button(const bool iActive)
{
  ui->btnHttpServer->setStyleSheet(get_bg_style_sheet((iActive ? 0xf97903 : 0x45bb24)));
  ui->btnHttpServer->setFixedSize(QSize(32, 32));
}

void DabRadio::_slot_handle_favorite_button(bool /*iClicked*/)
{
  mCurFavoriteState = !mCurFavoriteState;
  set_favorite_button_style();
  mpServiceListHandler->set_favorite_state(mCurFavoriteState);
}

void DabRadio::set_favorite_button_style()
{
  ui->btnToggleFavorite->setIcon(QIcon(mCurFavoriteState ? ":res/icons/starfilled24.png" : ":res/icons/starempty24.png"));
  ui->btnToggleFavorite->setIconSize(QSize(24, 24));
  ui->btnToggleFavorite->setFixedSize(QSize(32, 32));
}

void DabRadio::_slot_set_static_button_style()
{
  ui->btnPrevService->setIconSize(QSize(24, 24));
  ui->btnPrevService->setFixedSize(QSize(32, 32));
  ui->btnNextService->setIconSize(QSize(24, 24));
  ui->btnNextService->setFixedSize(QSize(32, 32));
  ui->btnEject->setIconSize(QSize(24, 24));
  ui->btnEject->setFixedSize(QSize(32, 32));
  ui->btnTargetService->setIconSize(QSize(24, 24));
  ui->btnTargetService->setFixedSize(QSize(32, 32));
  ui->btnTechDetails->setIconSize(QSize(24, 24));
  ui->btnTechDetails->setFixedSize(QSize(32, 32));
  ui->btnHttpServer->setIconSize(QSize(24, 24));
  ui->btnHttpServer->setFixedSize(QSize(32, 32));
  ui->btnDeviceWidget->setIconSize(QSize(24, 24));
  ui->btnDeviceWidget->setFixedSize(QSize(32, 32));
  ui->btnSpectrumScope->setIconSize(QSize(24, 24));
  ui->btnSpectrumScope->setFixedSize(QSize(32, 32));
  ui->configButton->setIconSize(QSize(24, 24));
  ui->configButton->setFixedSize(QSize(32, 32));
  ui->btnTii->setIconSize(QSize(24, 24));
  ui->btnTii->setFixedSize(QSize(32, 32));
  ui->btnCir->setIconSize(QSize(24, 24));
  ui->btnCir->setFixedSize(QSize(32, 32));
  ui->btnScanning->setIconSize(QSize(24, 24));
  ui->btnScanning->setFixedSize(QSize(32, 32));
  ui->btnScanning->init(":res/icons/scan24.png", 3, 1);
}

void DabRadio::_create_status_info()
{
  ui->layoutStatus->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

  _add_status_label_elem(mStatusInfo.InpBitRate,  0x40c6db, "-- kbps",  "Input bit-rate of the audio decoder");
  _add_status_label_elem(mStatusInfo.OutSampRate, 0xDE9769, "-- kSps",  "Output sample-rate of the audio decoder");
  _add_status_label_elem(mStatusInfo.Stereo,      0xf2c629, "Stereo",   "Stereo");
  _add_status_label_elem(mStatusInfo.PS,          0xf2c629, "PS",       "Parametric Stereo");
  _add_status_label_elem(mStatusInfo.SBR,         0xf2c629, "SBR",      "Spectral Band Replication");
  _add_status_label_elem(mStatusInfo.EPG,         0xf2c629, "EPG",      "Electronic Program Guide");
  _add_status_label_elem(mStatusInfo.Announce,    0xf2c629, "Ann",      "Announcement");
  _add_status_label_elem(mStatusInfo.RsError,     0xFF2E18, "RS",       "Reed Solomon Error occurred");
  _add_status_label_elem(mStatusInfo.CrcError,    0xFF2E18, "CRC",      "CRC Error occurred");

  ui->layoutStatus->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

  _reset_status_info();
}

template<typename T>
void DabRadio::_add_status_label_elem(StatusInfoElem<T> & ioElem, const u32 iColor, const QString & iName, const QString & iToolTip)
{
  ioElem.colorOn = iColor;
  ioElem.colorOff = 0x444444;
  ioElem.value = !T(); // invalidate cache

  ioElem.pLbl = new QLabel(this);

  ioElem.pLbl->setObjectName("lbl" + iName);
  ioElem.pLbl->setText(iName);
  ioElem.pLbl->setToolTip(iToolTip);

  QFont font = ioElem.pLbl->font();
  font.setBold(true);
  // font.setWeight(QFont::Thin);
  ioElem.pLbl->setFont(font);

  ui->layoutStatus->addWidget(ioElem.pLbl);
}

template<typename T>
void DabRadio::_set_status_info_status(StatusInfoElem<T> & iElem, const T iValue)
{
  if (iElem.value != iValue)
  {
    iElem.value = iValue;
    iElem.pLbl->setStyleSheet(iElem.value ? "QLabel { color: " + iElem.colorOn.name() + " }"
                                          : "QLabel { color: " + iElem.colorOff.name() + " }");

    // trick a bit: i32 are bit rates, u32 are sample rates
    if (std::is_same<T, i32>::value)
    {
      if (iElem.value == 0)
      {
        iElem.pLbl->setText("-- kbps"); // not clean to place the unit here but it is the only one yet
      }
      else
      {
        iElem.pLbl->setText(QString::number(iValue) + " kbps"); // not clean to place the unit here but it is the only one yet
      }
    }
    else if (std::is_same<T, u32>::value)
    {
      if (iElem.value == 0)
      {
        iElem.pLbl->setText("-- kSps"); // not clean to place the unit here but it is the only one yet
      }
      else
      {
        iElem.pLbl->setText(QString::number(iValue) + " kSps"); // not clean to place the unit here but it is the only one yet
      }
    }
  }
}

void DabRadio::_reset_status_info()
{
  _set_status_info_status(mStatusInfo.InpBitRate, (i32)0);
  _set_status_info_status(mStatusInfo.OutSampRate, (u32)0);
  _set_status_info_status(mStatusInfo.Stereo, false);
  _set_status_info_status(mStatusInfo.EPG, false);
  _set_status_info_status(mStatusInfo.SBR, false);
  _set_status_info_status(mStatusInfo.PS, false);
  _set_status_info_status(mStatusInfo.Announce, false);
  _set_status_info_status(mStatusInfo.RsError, false);
  _set_status_info_status(mStatusInfo.CrcError, false);
}

void DabRadio::_set_device_to_file_mode(const bool iDataFromFile)
{
  assert(mpServiceListHandler != nullptr);

  if (iDataFromFile)
  {
    _show_hide_buttons(false);
    mpServiceListHandler->set_data_mode(ServiceListHandler::EDataMode::Temporary);
    mpServiceListHandler->delete_table(false);
    mpServiceListHandler->create_new_table();
  }
  else
  {
    _show_hide_buttons(true);
    mpServiceListHandler->set_data_mode(ServiceListHandler::EDataMode::Permanent);
  }
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

void DabRadio::_slot_handle_volume_slider(const int iSliderValue)
{
  // if muting is currently active, unmute audio with touching the volume slider
  if (mMutingActive)
  {
    _slot_update_mute_state(false);
  }

  assert(mpAudioOutput != nullptr);
  mpAudioOutput->slot_setVolume(iSliderValue);
}

void DabRadio::slot_show_audio_peak_level(const f32 iPeakLeft, const f32 iPeakRight)
{
  auto decay = [](f32 iPeak, f32 & ioPeakAvr) -> void
  {
    ioPeakAvr = (iPeak >= ioPeakAvr ? iPeak : ioPeakAvr - 0.5f /*decay*/);
  };

  decay(iPeakLeft, mPeakLeftDamped);
  decay(iPeakRight, mPeakRightDamped);

  ui->thermoPeakLevelLeft->setValue(mPeakLeftDamped);
  ui->thermoPeakLevelRight->setValue(mPeakRightDamped);
}

void DabRadio::clean_up()
{
  // TODO: resetting peak meters is not working well after service change
  mPeakLeftDamped = mPeakRightDamped = -40.0f;
  slot_show_audio_peak_level(-40.0f, -40.0);
  ui->progBarFicError->setValue(0);
  ui->progBarAudioBuffer->setValue(0);
}

QString DabRadio::_convert_links_to_clickable(const QString & iText) const
{
  // qDebug() << "iText: " << iText << iText.length();

  if (!mConfig.cbUrlClickable->isChecked())
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

