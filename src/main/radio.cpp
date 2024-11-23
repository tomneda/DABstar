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
#include "radio.h"
#include "dab-tables.h"
#include "ITU_Region_1.h"
#include "coordinates.h"
#include "mapport.h"
#include "techdata.h"
#include "service-list-handler.h"
#include "setting-helper.h"
#include "Qt-audio.h"
#include "audiooutputqt.h"
#include "angle_direction.h"
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
#include "Qt-audio.h"
#include "audiooutputqt.h"
#include "time-table.h"
#include "device-exceptions.h"

#if defined(__MINGW32__) || defined(_WIN32)
  #include <windows.h>
#else
  #include <unistd.h>
#endif

Q_LOGGING_CATEGORY(sLogRadioInterface, "RadioInterface", QtInfoMsg)

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


RadioInterface::RadioInterface(QSettings * const ipSettings, const QString & iFileNameDb, const QString & iFileNameAltFreqList, int32_t iDataPort, QWidget * iParent)
  : QWidget(iParent)
  , Ui_DabRadio()
  , mSpectrumViewer(this, ipSettings, &mSpectrumBuffer, &mIqBuffer, &mCarrBuffer, &mResponseBuffer)
  , mBandHandler(iFileNameAltFreqList, ipSettings)
  , mOpenFileDialog(ipSettings)
  , mConfig(this)
  , mpSH(&SettingHelper::get_instance())
  , mDeviceSelector(ipSettings)
{
  int32_t k;
  QString h;

  mIsRunning.store(false);
  mIsScanning.store(false);

  // "mProcessParams" is introduced to reduce the number of parameters for the dabProcessor
  mProcessParams.spectrumBuffer = &mSpectrumBuffer;
  mProcessParams.iqBuffer = &mIqBuffer;
  mProcessParams.carrBuffer = &mCarrBuffer;
  mProcessParams.responseBuffer = &mResponseBuffer;
  mProcessParams.frameBuffer = &mFrameBuffer;

  mProcessParams.dabMode = mpSH->read(SettingHelper::dabMode).toInt();
  mProcessParams.threshold = mpSH->read(SettingHelper::threshold).toInt();
  mProcessParams.diff_length = mpSH->read(SettingHelper::diffLength).toInt();
  mProcessParams.tii_delay = mpSH->read(SettingHelper::tiiDelay).toInt();
  if (mProcessParams.tii_delay < 2) { mProcessParams.tii_delay = 2; }
  mProcessParams.tii_depth = mpSH->read(SettingHelper::tiiDepth).toInt();
  mProcessParams.echo_depth = mpSH->read(SettingHelper::echoDepth).toInt();

  //	set on top or not? checked at start up
  if (mpSH->read(SettingHelper::cbAlwaysOnTop).toBool())
  {
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
  }

  //	The settings are done, now creation of the GUI parts
  setupUi(this);
  setFixedSize(730, 490+50);
  setup_ui_colors();
  _create_status_info();

  thermoPeakLevelLeft->setValue(-40.0);
  thermoPeakLevelRight->setValue(-40.0);
  thermoPeakLevelLeft->setFillBrush(QColor(0x6F70EF));
  thermoPeakLevelRight->setFillBrush(QColor(0x6F70EF));
  thermoPeakLevelLeft->setBorderWidth(0);
  thermoPeakLevelRight->setBorderWidth(0);

  mpServiceListHandler.reset(new ServiceListHandler(mpSH->get_settings(), iFileNameDb, tblServiceList));

  // only the queued call will consider the button size?!
  QMetaObject::invokeMethod(this, &RadioInterface::_slot_handle_mute_button, Qt::QueuedConnection);
  QMetaObject::invokeMethod(this, &RadioInterface::_slot_set_static_button_style, Qt::QueuedConnection);
  QMetaObject::invokeMethod(this, "_slot_favorite_changed", Qt::QueuedConnection, Q_ARG(bool, false)); // only this works with arguments

  //setWindowTitle(QString(PRJ_NAME) + QString(" (V" PRJ_VERS ")"));
  setWindowTitle(PRJ_NAME);

  mpSH->read_widget_geometry(SettingHelper::mainWidget, this);

  mpTechDataWidget = new TechData(this, mpSH->get_settings(), &mTechDataBuffer);

  _show_epg_label(false);

  mChannel.currentService.valid = false;
  mChannel.nextService.valid = false;
  mChannel.serviceCount = -1;

  // load last used service

  if (QString presetName = mpSH->read(SettingHelper::presetName).toString();
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

  mChannel.targetPos = cmplx(0, 0);
  const float local_lat = mpSH->read(SettingHelper::latitude).toFloat();
  const float local_lon = mpSH->read(SettingHelper::longitude).toFloat();
  mChannel.localPos = cmplx(local_lat, local_lon);

  mConfig.cmbSoftBitGen->addItems(get_soft_bit_gen_names()); // fill soft-bit-type combobox with text elements

  mShowOnlyCurrTrans = mpSH->read(SettingHelper::cbShowOnlyCurrTrans).toBool();
  mpTechDataWidget->hide();  // until shown otherwise
  mServiceList.clear();
#ifdef DATA_STREAMER
  dataStreamer = new tcpServer(dataPort);
#endif

  // Where do we leave the audio out?
  mpAudioOutput = new AudioOutputQt(this);
  connect(mpAudioOutput, &AudioOutput::signal_audio_devices_list, this, &RadioInterface::_slot_load_audio_device_list);
  connect(this, &RadioInterface::signal_start_audio, mpAudioOutput, &AudioOutput::slot_start, Qt::QueuedConnection);
  connect(this, &RadioInterface::signal_switch_audio, mpAudioOutput, &AudioOutput::slot_restart, Qt::QueuedConnection);
  connect(this, &RadioInterface::signal_stop_audio, mpAudioOutput, &AudioOutput::slot_stop, Qt::QueuedConnection);
  connect(this, &RadioInterface::signal_set_audio_device, mpAudioOutput, &AudioOutput::slot_set_audio_device, Qt::QueuedConnection);
  connect(this, &RadioInterface::signal_audio_mute, mpAudioOutput, &AudioOutput::slot_mute, Qt::QueuedConnection);
  connect(this, &RadioInterface::signal_audio_buffer_filled_state, progBarAudioBuffer, &QProgressBar::setValue);

  mAudioOutputThread = new QThread(this);
  mAudioOutputThread->setObjectName("audioOutThr");
  mpAudioOutput->moveToThread(mAudioOutputThread);
  connect(mAudioOutputThread, &QThread::finished, mpAudioOutput, &QObject::deleteLater);
  mAudioOutputThread->start();

  _slot_load_audio_device_list(mpAudioOutput->get_audio_device_list());
  mConfig.streamoutSelector->show();

  h = mpSH->read(SettingHelper::soundChannel).toString();
  k = mConfig.streamoutSelector->findText(h);
  bool run = false;
  if (k != -1)
  {
    mConfig.streamoutSelector->setCurrentIndex(k);
    emit signal_set_audio_device(mConfig.streamoutSelector->itemData(k).toByteArray());
  }

  if (k == -1 || !run)
  {
    // TODO: sound startup the first time failed
    //mpSoundOut->selectDevice(0); // selects default device
  }

  mPicturesPath = mpSH->read(SettingHelper::picturesPath).toString();
  mPicturesPath = check_and_create_dir(mPicturesPath);

  mFilePath = mpSH->read(SettingHelper::filePath).toString();
  mFilePath = check_and_create_dir(mFilePath);

  mEpgPath = mpSH->read(SettingHelper::epgPath).toString();
  mEpgPath = check_and_create_dir(mEpgPath);

  connect(&mEpgProcessor, &EpgDecoder::signal_set_epg_data, this, &RadioInterface::slot_set_epg_data);

  //	timer for autostart epg service
  mEpgTimer.setSingleShot(true);
  connect(&mEpgTimer, &QTimer::timeout, this, &RadioInterface::slot_epg_timer_timeout);

  mpTimeTable = new timeTableHandler(this);
  mpTimeTable->hide();

  connect(this, &RadioInterface::signal_set_new_channel, cmbChannelSelector, &QComboBox::setCurrentIndex);
  connect(btnHttpServer, &QPushButton::clicked, this,  &RadioInterface::_slot_handle_http_button);

  //	restore some settings from previous incarnations
  const QString t = mpSH->read(SettingHelper::dabBand).toString();
  const uint8_t dabBand = (t == "VHF Band III" ? BAND_III : L_BAND);

  mBandHandler.setupChannels(cmbChannelSelector, dabBand);
  const QString skipFileName = mpSH->read(SettingHelper::skipFile).toString();
  mBandHandler.setup_skipList(skipFileName);

  connect(mpTechDataWidget, &TechData::signal_handle_timeTable, this, &RadioInterface::_slot_handle_time_table);

  lblVersion->setText(QString("V" + mVersionStr));
  lblCopyrightIcon->setToolTip(get_copyright_text());
  lblCopyrightIcon->setOpenExternalLinks(true);

  const QString tiiFileName = mpSH->read(SettingHelper::tiiFile).toString();
  mChannel.tiiFile = false;
  if (!tiiFileName.isEmpty())
  {
    mChannel.tiiFile = mTiiHandler.fill_cache_from_tii_file(tiiFileName);
    if (!mChannel.tiiFile)
    {
      btnHttpServer->setToolTip("File '" + tiiFileName + "' could not be loaded. So this feature is switched off.");
      btnHttpServer->setEnabled(false);
    }
  }

  connect(this, &RadioInterface::signal_dab_processor_started, &mSpectrumViewer, &SpectrumViewer::slot_update_settings);
  connect(&mSpectrumViewer, &SpectrumViewer::signal_window_closed, this, &RadioInterface::_slot_handle_spectrum_button);
  connect(mpTechDataWidget, &TechData::signal_window_closed, this, &RadioInterface::_slot_handle_tech_detail_button);

  mChannel.etiActive = false;
  show_pause_slide();

  // start the timer(s)
  // The displaytimer is there to show the number of seconds running and handle - if available - the tii data
  mDisplayTimer.setInterval(cDisplayTimeoutMs);
  connect(&mDisplayTimer, &QTimer::timeout, this, &RadioInterface::_slot_update_time_display);
  mDisplayTimer.start(cDisplayTimeoutMs);

  //	timer for scanning
  mChannelTimer.setSingleShot(true);
  mChannelTimer.setInterval(cChannelTimeoutMs);
  connect(&mChannelTimer, &QTimer::timeout, this, &RadioInterface::_slot_channel_timeout);

  //	presetTimer
  mPresetTimer.setSingleShot(true);
  connect(&mPresetTimer, &QTimer::timeout, this, &RadioInterface::_slot_preset_timeout);

  _set_clock_text();
  mClockResetTimer.setSingleShot(true);
  connect(&mClockResetTimer, &QTimer::timeout, [this](){ _set_clock_text(); });

  mConfig.deviceSelector->addItems(mDeviceSelector.get_device_name_list());

  h = mpSH->read(SettingHelper::device).toString();
  k = mConfig.deviceSelector->findText(h);
  if (k != -1)
  {
    mConfig.deviceSelector->setCurrentIndex(k);

    mpInputDevice = mDeviceSelector.create_device(mConfig.deviceSelector->currentText(), mChannel.realChannel);

    if (mpInputDevice != nullptr)
    {
      _set_device_to_file_mode(!mChannel.realChannel);
    }
  }

  connect(configButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_config_button);

  if (mpSH->read(SettingHelper::spectrumVisible).toBool())
  {
    mSpectrumViewer.show();
  }

  if (mpSH->read(SettingHelper::techDataVisible).toBool())
  {
    mpTechDataWidget->show();
  }

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

  connect(mConfig.deviceSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_do_start);

  qApp->installEventFilter(this);
}

RadioInterface::~RadioInterface()
{
  if (mAudioOutputThread != nullptr)
  {
    mAudioOutputThread->quit();  // this deletes mpAudioOutput
    mAudioOutputThread->wait();
    delete mAudioOutputThread;
  }

  fprintf(stdout, "RadioInterface is deleted\n");
}

void RadioInterface::_set_clock_text(const QString & iText /*= QString()*/)
{
  if (!iText.isEmpty())
  {
    mClockResetTimer.start(1300); // if in 1300ms no new clock text is sent, this method is called with an empty iText

    if (!mClockActiveStyle)
    {
      lblLocalTime->setStyleSheet(get_bg_style_sheet({ 255, 60, 60 }, "QLabel") + " QLabel { color: white; }");
      mClockActiveStyle = true;
    }
    lblLocalTime->setText(iText);
  }
  else
  {
    if (mClockActiveStyle)
    {
      lblLocalTime->setStyleSheet(get_bg_style_sheet({ 57, 0, 0 }, "QLabel") + " QLabel { color: #333333; }");
      mClockActiveStyle = false;
    }
    //lblLocalTime->setText("YYYY-MM-DD hh:mm:ss");
    lblLocalTime->setText("0000-00-00 00:00:00");
  }
}

QString RadioInterface::get_copyright_text() const
{
#ifdef __WITH_FDK_AAC__
  QString usedDecoder = "FDK-AAC";
#else
  QString usedDecoder = "libfaad";
#endif
  QString versionText = "<html><head/><body><p>";
  versionText = "<h3>" + QString(PRJ_NAME) + " V" + mVersionStr + " (Qt " QT_VERSION_STR " / Qwt " QWT_VERSION_STR ")</h3>";
  versionText += "<p><b>Built on " + QString(__TIMESTAMP__) + QString("<br/>Commit ") + QString(GITHASH) + ".</b></p>";
  versionText += "<p>Forked from Qt-DAB, partly extensive changed, extended, some things also removed, by Thomas Neder "
                 "(<a href=\"https://github.com/tomneda/DABstar\">https://github.com/tomneda/DABstar</a>).<br/>"
                 "For Qt-DAB see <a href=\"https://github.com/JvanKatwijk/qt-dab\">https://github.com/JvanKatwijk/qt-dab</a> by Jan van Katwijk<br/>"
                 "(<a href=\"mailto:J.vanKatwijk@gmail.com\">J.vanKatwijk@gmail.com</a>).</p>";
  versionText += "<p>Rights of Qt, Qwt, FFTW, Kiss, liquid-DSP, portaudio, " + usedDecoder + ", libsamplerate and libsndfile gratefully acknowledged.<br/>"
                 "Rights of developers of RTLSDR library, SDRplay libraries, AIRspy library and others gratefully acknowledged.<br/>"
                 "Rights of other contributors gratefully acknowledged.</p>";
  versionText += "</p></body></html>";
  return versionText;
}

void RadioInterface::_show_epg_label(const bool iShowLabel)
{
  _set_status_info_status(mStatusInfo.EPG, iShowLabel);
}

// _slot_do_start(QString) is called when - on startup - NO device was registered to be used,
// and the user presses the selectDevice comboBox
void RadioInterface::_slot_do_start(const QString & dev)
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

void RadioInterface::_trigger_preset_timer()
{
  // const int32_t switchDelay = mpSH->read(SettingHelper::switchDelay).toInt();
  mPresetTimer.setSingleShot(true);
  mPresetTimer.setInterval(cPresetTimeoutMs);
  mPresetTimer.start(cPresetTimeoutMs);
}

//	when doStart is called, a device is available and selected
bool RadioInterface::do_start()
{
  _update_channel_selector();

  // here the DAB processor is created (again)
  mpDabProcessor.reset(new DabProcessor(this, mpInputDevice.get(), &mProcessParams));

  mChannel.clean_channel();

  //	Some hidden buttons can be made visible now
  connect_gui();
  connect_dab_processor();

  mpDabProcessor->set_tiiDetectorMode(mpSH->read(SettingHelper::cbUseNewTiiDetector).toBool());
  mpDabProcessor->set_dc_avoidance_algorithm(mpSH->read(SettingHelper::cbUseDcAvoidance).toBool());
  mpDabProcessor->set_dc_removal(mpSH->read(SettingHelper::cbUseDcRemoval).toBool());

  // should the device widget be shown?
  if (mpSH->read(SettingHelper::showDeviceWidget).toBool())
  {
    mpInputDevice->show();
  }

  // if (mChannel.nextService.valid)
  // {
  //   _trigger_preset_timer();
  // }

  emit signal_dab_processor_started(); // triggers the DAB processor rereading (new) settings

  //	after the preset timer signals, the service will be started
  start_channel(cmbChannelSelector->currentText());
  mIsRunning.store(true);

  return true;
}

void RadioInterface::_update_channel_selector()
{
  if (mChannel.nextService.channel != "")
  {
    int k = cmbChannelSelector->findText(mChannel.nextService.channel);
    if (k != -1)
    {
      cmbChannelSelector->setCurrentIndex(k);
    }
  }
  else
  {
    cmbChannelSelector->setCurrentIndex(0);
  }
}

//
///////////////////////////////////////////////////////////////////////////////
//
//	a slot called by the DAB-processor
void RadioInterface::slot_set_and_show_freq_corr_rf_Hz(int iFreqCorrRF)
{
  if (mpInputDevice != nullptr && mChannel.nominalFreqHz > 0)
  {
    mpInputDevice->setVFOFrequency(mChannel.nominalFreqHz + iFreqCorrRF);
  }

  mSpectrumViewer.show_freq_corr_rf_Hz(iFreqCorrRF);
}

void RadioInterface::slot_show_freq_corr_bb_Hz(int iFreqCorrBB)
{
  mSpectrumViewer.show_freq_corr_bb_Hz(iFreqCorrBB);
}

//
//	might be called when scanning only
void RadioInterface::_slot_channel_timeout()
{
  qCDebug(sLogRadioInterface()) << Q_FUNC_INFO;
  slot_no_signal_found();
}                                                                   

///////////////////////////////////////////////////////////////////////////
//
//	a slot, called by the fic/fib handlers
void RadioInterface::slot_add_to_ensemble(const QString & iServiceName, const uint32_t iSId)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (!mIsScanning)
  {
    _trigger_preset_timer();
  }

  qCDebug(sLogRadioInterface()) << Q_FUNC_INFO << iServiceName << QString::number(iSId, 16);

  const int32_t subChId = mpDabProcessor->getSubChId(iServiceName, iSId);

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

  mServiceList = insert_sorted(mServiceList, ed);

  if (mIsScanning)
  {
    if (mpDabProcessor->is_audioService(iServiceName))
    {
      mpServiceListHandler->add_entry(mChannel.channelName, iServiceName);
    }
  }
}

//	The ensembleId is written as hexadecimal, however, the
//	number display of Qt is only 7 segments ...
static QString hex_to_str(int v)
{
  QString res;
  for (int i = 0; i < 4; i++)
  {
    uint8_t t = (v & 0xF000) >> 12;
    QChar c = t <= 9 ? (char)('0' + t) : (char)('A' + t - 10);
    res.append(c);
    v <<= 4;
  }
  return res;
}

//	a slot, called by the fib processor
void RadioInterface::slot_name_of_ensemble(int id, const QString & v)
{
  QString s;
  if (!mIsRunning.load())
  {
    return;
  }

  QFont font = ensembleId->font();
  font.setPointSize(14);
  ensembleId->setFont(font);
  ensembleId->setText(v + QString("(") + hex_to_str(id) + QString(")"));

  mChannel.ensembleName = v;
  mChannel.Eid = id;
}

void RadioInterface::_slot_handle_content_button()
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

  if (mpSH->read(SettingHelper::cbUseUtcTime).toBool())
  {
    theTime = convertTime(mUTC.year, mUTC.month, mUTC.day, mUTC.hour, mUTC.minute);
  }
  else
  {
    theTime = convertTime(mLocalTime.year, mLocalTime.month, mLocalTime.day, mLocalTime.hour, mLocalTime.minute);
  }

  QString header = mChannel.ensembleName + ";" + mChannel.channelName + ";" + QString::number(mChannel.nominalFreqHz / 1000) + ";"
                   + hex_to_str(mChannel.Eid) + " " + ";" + transmitter_coordinates->text() + " " + ";" + theTime + ";" + SNR + ";"
                   + QString::number(mServiceList.size()) + ";" + lblStationLocation->text() + "\n";

  mpContentTable = new ContentTable(this, mpSH->get_settings(), mChannel.channelName, mpDabProcessor->scanWidth());
  connect(mpContentTable, &ContentTable::signal_go_service, this, &RadioInterface::slot_handle_content_selector);

  mpContentTable->addLine(header);
  //	my_contentTable		-> addLine ("\n");

  for (const auto & sl : s)
  {
    mpContentTable->addLine(sl);
  }
  mpContentTable->show();
}

QString RadioInterface::check_and_create_dir(const QString & s)
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

  if (QDir(dir).exists())
  {
    return dir;
  }
  QDir().mkpath(dir);
  return dir;
}

void RadioInterface::slot_handle_mot_object(QByteArray result, QString objectName, int contentType, bool dirElement)
{
  QString realName;

  switch (getContentBaseType((MOTContentType)contentType))
  {
  case MOTBaseTypeGeneralData: break;
  case MOTBaseTypeText: save_MOTtext(result, contentType, objectName); break;
  case MOTBaseTypeImage: show_MOTlabel(result, contentType, objectName, dirElement); break;
  case MOTBaseTypeAudio: break;
  case MOTBaseTypeVideo: break;
  case MOTBaseTypeTransport: save_MOTObject(result, objectName); break;
  case MOTBaseTypeSystem: break;

  case MOTBaseTypeApplication:  // epg data
    if (mEpgPath == "")
    {
      return;
    }

    if (objectName == QString(""))
    {
      objectName = "epg file";
    }
    objectName = mEpgPath + objectName;

    {
      QString temp = objectName;
      temp = temp.left(temp.lastIndexOf(QChar('/')));
      if (!QDir(temp).exists())
      {
        QDir().mkpath(temp);
      }

      std::vector<uint8_t> epgData(result.begin(), result.end());
      uint32_t ensembleId = mpDabProcessor->get_ensembleId();
      uint32_t currentSId = extract_epg(objectName, mServiceList, ensembleId);
      uint32_t julianDate = mpDabProcessor->julianDate();
      int subType = getContentSubType((MOTContentType)contentType);
      mEpgProcessor.process_epg(epgData.data(), (int32_t)epgData.size(), currentSId, subType, julianDate);

      if (mpSH->read(SettingHelper::cbGenXmlFromEpg).toBool())
      {
        mEpgHandler.decode(epgData, QDir::toNativeSeparators(objectName));
      }
    }
    return;

  case MOTBaseTypeProprietary: break;
  }
}

void RadioInterface::save_MOTtext(QByteArray & result, int contentType, QString name)
{
  (void)contentType;
  if (mFilePath == "")
  {
    return;
  }

  QString textName = QDir::toNativeSeparators(mFilePath + name);

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

void RadioInterface::save_MOTObject(QByteArray & result, QString name)
{
  if (mFilePath == "")
  {
    return;
  }

  if (name == "")
  {
    name = "motObject_" + QString::number(mMotObjectCnt);
    ++mMotObjectCnt;
  }
  save_MOTtext(result, 5, name);
}

//	MOT slide, to show
void RadioInterface::show_MOTlabel(QByteArray & data, int contentType, const QString & pictureName, int dirs)
{
  const char * type;
  if (!mIsRunning.load() || (pictureName == QString("")))
  {
    return;
  }

  (void)dirs;
  switch (static_cast<MOTContentType>(contentType))
  {
  case MOTCTImageGIF: type = "GIF";
    break;

  case MOTCTImageJFIF: type = "JPG";
    break;

  case MOTCTImageBMP: type = "BMP";
    break;

  case MOTCTImagePNG: type = "PNG";
    break;

  default: return;
  }

  if (mpSH->read(SettingHelper::cbSaveSlides).toBool() && (mPicturesPath != ""))
  {
    QString pict = mPicturesPath + pictureName;
    QString temp = pict;
    temp = temp.left(temp.lastIndexOf(QChar('/')));
    if (!QDir(temp).exists())
    {
      QDir().mkpath(temp);
    }
    pict = QDir::toNativeSeparators(pict);
    FILE * x = fopen(pict.toUtf8().data(), "w+b");

    if (x == nullptr)
    {
      fprintf(stderr, "cannot write file %s\n", pict.toUtf8().data());
    }
    else
    {
      fprintf(stdout, "going to write picture file %s\n", pict.toUtf8().data());
      (void)fwrite(data.data(), 1, data.length(), x);
      fclose(x);
    }
  }

  if (!mChannel.currentService.is_audio)
  {
    return;
  }

  //	if (dirs != 0)
  //	   return;

  QPixmap p;
  p.loadFromData(data, type);
  write_picture(p);
}

void RadioInterface::write_picture(const QPixmap & iPixMap) const
{
  constexpr int w = 320;
  constexpr int h = 240;

  // typical the MOT size is 320 : 240 , so only scale for other sizes
  if (iPixMap.width() != w || iPixMap.height() != h)
  {
    //qDebug("MOT w: %d, h: %d (scaled)", iPixMap.width(), iPixMap.height());
    pictureLabel->setPixmap(iPixMap.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  else
  {
    // qDebug("MOT w: %d, h: %d", iPixMap.width(), iPixMap.height());
    pictureLabel->setPixmap(iPixMap);
  }

  pictureLabel->setAlignment(Qt::AlignCenter);
  pictureLabel->show();
}

//
//	sendDatagram is triggered by the ip handler,
void RadioInterface::slot_send_datagram(int length)
{
  auto * const localBuffer = make_vla(uint8_t, length);

  if (mDataBuffer.get_ring_buffer_read_available() < length)
  {
    fprintf(stderr, "Something went wrong\n");
    return;
  }

  mDataBuffer.get_data_from_ring_buffer(localBuffer, length);
}

//
//	tdcData is triggered by the backend.
void RadioInterface::slot_handle_tdc_data(int frametype, int length)
{
#ifdef DATA_STREAMER
  uint8_t localBuffer [length + 8];
#endif
  (void)frametype;
  if (!mIsRunning.load())
  {
    return;
  }
  if (mDataBuffer.get_ring_buffer_read_available() < length)
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
void RadioInterface::slot_change_in_configuration()
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

  //	we rebuild the services list from the fib and
  //	then we (try to) restart the service
  mServiceList = mpDabProcessor->getServices();

  // TODO: use mpDabProcessor->getServices() to setup new service channel list
//  model.clear();
//  for (const auto & serv: serviceList)
//  {
//    model.appendRow(new QStandardItem(serv.name));
//  }
//  int row = model.rowCount();
//  for (int i = 0; i < row; i++)
//  {
//    model.setData(model.index(i, 0), QFont(theFont, fontSize), Qt::FontRole);
//  }
  //ensembleDisplay->setModel(&model);
  //
  if (mChannel.etiActive)
  {
    mpDabProcessor->reset_etiGenerator();
  }

  //	Of course, it may be disappeared
  if (s.valid)
  {
    if (const QString ss = mpDabProcessor->findService(s.SId, s.SCIds);
        ss != "")
    {
      start_service(s);
      return;
    }

    //	The service is gone, it may be the subservice of another one
    s.SCIds = 0;
    s.serviceName = mpDabProcessor->findService(s.SId, s.SCIds);

    if (s.serviceName != "")
    {
      start_service(s);
    }
  }

  //	we also have to restart all background services,
  for (uint16_t i = 0; i < mChannel.backgroundServices.size(); ++i)
  {
    if (const QString ss = mpDabProcessor->findService(s.SId, s.SCIds);
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
      if (mpDabProcessor->is_audioService(ss))
      {
        Audiodata ad;
        FILE * f = mChannel.backgroundServices.at(i).fd;
        mpDabProcessor->dataforAudioService(ss, &ad);
        mpDabProcessor->set_audio_channel(&ad, &mAudioBufferFromDecoder, f, BACK_GROUND);
        mChannel.backgroundServices.at(i).subChId = ad.subchId;
      }
      else
      {
        Packetdata pd;
        mpDabProcessor->dataforPacketService(ss, &pd, 0);
        mpDabProcessor->set_data_channel(&pd, &mDataBuffer, BACK_GROUND);
        mChannel.backgroundServices.at(i).subChId = pd.subchId;
      }
      // TODO: select the background service in service list?
//      for (int j = 0; j < model.rowCount(); j++)
//      {
//        QString itemText = model.index(j, 0).data(Qt::DisplayRole).toString();
//        if (itemText == ss)
//        {
//          colorService(model.index(j, 0), Qt::blue, fontSize + 2, true);
//        }
//      }
    }
  }
}

//
//	In order to not overload with an enormous amount of
//	signals, we trigger this function at most 10 times a second
//
void RadioInterface::slot_new_audio(const int32_t iAmount, const uint32_t iAudioSampleRate, const uint32_t iAudioFlags)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (mAudioFrameCnt++ > 10)
  {
    mAudioFrameCnt = 0;

    const bool sbrUsed = ((iAudioFlags & RadioInterface::AFL_SBR_USED) != 0);
    const bool psUsed  = ((iAudioFlags & RadioInterface::AFL_PS_USED) != 0);
    _set_status_info_status(mStatusInfo.SBR, sbrUsed);
    _set_status_info_status(mStatusInfo.PS, psUsed);

    if (!mpTechDataWidget->isHidden())
    {
      mpTechDataWidget->show_sample_rate_and_audio_flags((int32_t)iAudioSampleRate, sbrUsed, psUsed);
    }
  }

  if (mpCurAudioFifo == nullptr)
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
  auto * const vec = make_vla(int16_t, iAmount);

  while (mAudioBufferFromDecoder.get_ring_buffer_read_available() > iAmount)
  {
    mean_filter(mAudioBufferFillFiltered, mAudioBufferToOutput.get_fill_state_in_percent(), 0.05f);

    // vec contains a stereo pair [0][1]...[n-2][n-1]
    mAudioBufferFromDecoder.get_data_from_ring_buffer(vec, iAmount);

    Q_ASSERT(mpCurAudioFifo != nullptr);

    // the audio output buffer got flooded, try to begin from new (set to 50% would also be ok, but needs interface in RingBuffer)
    if (iAmount > mAudioBufferToOutput.get_ring_buffer_write_available())
    {
      mAudioBufferToOutput.flush_ring_buffer();
    }

    mAudioBufferToOutput.put_data_into_ring_buffer(vec, iAmount);

    if (mAudioDumpState == EAudioDumpState::Running)
    {
      mWavWriter.write(vec, iAmount);
    }

    // ugly, but palette of progressbar can only be set in the same thread of the value setting, save time calling this only once
    if (!mProgBarAudioBufferFullColorSet)
    {
      mProgBarAudioBufferFullColorSet = true;
      QPalette p = progBarAudioBuffer->palette();
      p.setColor(QPalette::Highlight, 0xEF8B2A);
      progBarAudioBuffer->setPalette(p);
    }

#ifdef HAVE_PLUTO_RXTX
    if (streamerOut != nullptr)
    {
      streamerOut->audioOut(vec, amount, rate);
    }
#endif

    if (!mpTechDataWidget->isHidden() && !mMutingActive)
    {
      mTechDataBuffer.put_data_into_ring_buffer(vec, iAmount);
      mpTechDataWidget->audioDataAvailable(iAmount, iAudioSampleRate);
    }
  }

  emit signal_audio_buffer_filled_state((int32_t)mAudioBufferFillFiltered);
}
//

/////////////////////////////////////////////////////////////////////////////
//
/**
  *	\brief TerminateProcess
  *	Pretty critical, since there are many threads involved
  *	A clean termination is what is needed, regardless of the GUI
  */
void RadioInterface::_slot_terminate_process()
{
  if (mIsScanning.load())
  {
    stop_scanning();
  }
  mIsRunning.store(false);
  _show_hide_buttons(false);

  mpSH->write_widget_geometry(SettingHelper::mainWidget, this);

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
  delete mpTechDataWidget;
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
  mpSH->sync_and_unregister_ui_elements();
  mSpectrumViewer.hide();

  mpDabProcessor.reset();
  delete mpTimeTable;
  mpInputDevice = nullptr;

  fprintf(stdout, ".. end the radio silences\n");
}

void RadioInterface::_slot_update_time_display()
{
  if (!mIsRunning.load())
  {
    return;
  }

  mNumberOfSeconds++;

  if ((mNumberOfSeconds % 2) == 0)
  {
    size_t idle_time, total_time;
    if (get_cpu_times(idle_time, total_time))
    {
      const float idle_time_delta = static_cast<float>(idle_time - mPreviousIdleTime);
      const float total_time_delta = static_cast<float>(total_time - mPreviousTotalTime);
      const float utilization = 100.0f * (1.0f - idle_time_delta / total_time_delta);
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

// _slot_new_device is called from the UI when selecting a device with the selector
void RadioInterface::_slot_new_device(const QString & deviceName)
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
  mpInputDevice = mDeviceSelector.create_device(deviceName, mChannel.realChannel);

  if (mpInputDevice == nullptr)
  {
    return;    // nothing will happen
  }

  _set_device_to_file_mode(!mChannel.realChannel);

  do_start();    // will set running
}

void RadioInterface::_slot_handle_device_widget_button()
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

  mpSH->write(SettingHelper::deviceVisible, !mpInputDevice->isHidden());
}

///////////////////////////////////////////////////////////////////////////
//	signals, received from ofdm_decoder for which that data is
//	to be displayed
///////////////////////////////////////////////////////////////////////////

//	called from the fibDecoder
void RadioInterface::slot_clock_time(int year, int month, int day, int hours, int minutes, int utc_day, int utc_hour, int utc_min, int utc_sec)
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

  if (mpSH->read(SettingHelper::cbUseUtcTime).toBool())
  {
    result = convertTime(year, month, day, utc_hour, utc_min, utc_sec);
  }
  else
  {
    result = convertTime(year, month, day, hours, minutes, utc_sec);
  }
  _set_clock_text(result);
}

QString RadioInterface::convertTime(int year, int month, int day, int hours, int minutes, int seconds /*= -1*/)
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
void RadioInterface::slot_show_frame_errors(int s)
{
  if (!mIsRunning.load())
  {
    return;
  }
  if (!mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->show_frameErrors(s);
  }
}

//
//	called from the MP4 decoder
void RadioInterface::slot_show_rs_errors(int s)
{
  if (!mIsRunning.load())
  {    // should not happen
    return;
  }
  if (!mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->show_rsErrors(s);
  }
}

//
//	called from the aac decoder
void RadioInterface::slot_show_aac_errors(int s)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (!mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->show_aacErrors(s);
  }
}

//
//	called from the ficHandler
void RadioInterface::slot_show_fic_success(bool b)
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
    QPalette p = progBarFicError->palette();
    if (mFicSuccess < 85)
    {
      p.setColor(QPalette::Highlight, Qt::red);
    }
    else
    {
      p.setColor(QPalette::Highlight, 0xE6E600);
    }

    progBarFicError->setPalette(p);
    progBarFicError->setValue(mFicSuccess);
    mFicSuccess = 0;
    mFicBlocks = 0;
  }
}

//
//	called from the PAD handler
void RadioInterface::slot_show_mot_handling(bool b)
{
  if (!mIsRunning.load() || !b)
  {
    return;
  }
  mpTechDataWidget->show_motHandling(b);
}

//	just switch a color, called from the dabprocessor
void RadioInterface::slot_set_synced(bool b)
{
  (void)b;
}

//	called from the PAD handler

void RadioInterface::slot_show_label(const QString & s)
{
#ifdef HAVE_PLUTO_RXTX
  if (streamerOut != nullptr)
  {
    streamerOut->addRds(std::string(s.toUtf8().data()));
  }
#endif
  if (mIsRunning.load())
  {
    lblDynLabel->setText(s);
  }
  //	if we dtText is ON, some work is still to be done
  if ((mDlTextFile == nullptr) || (mDlCache.addifNew(s)))
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

void RadioInterface::slot_set_stereo(bool b)
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


void RadioInterface::slot_show_tii(int mainId, int subId)
{
  QString country = "";
  bool tiiChange = false;

  if (mainId == 0xFF)
  {
    return;
  }

  SChannelDescriptor::UTiiId id(mainId, subId);
  mChannel.transmitters.insert(id.FullId);

  if (!mIsRunning.load())
  {
    return;
  }

  if ((mainId != mChannel.mainId) || (subId != mChannel.subId))
  {
    LOG("tii numbers", tiiNumber(mainId) + " " + tiiNumber(subId));
    tiiChange = true;
  }

  mChannel.mainId = mainId;
  mChannel.subId = subId;

  QString a; // = "TII:";
  uint16_t cnt = 0;
  for (const auto & tr : mChannel.transmitters)
  {
    SChannelDescriptor::UTiiId id(tr);
    a = a + " " + tiiNumber(id.MainId) + "-" + tiiNumber(id.SubId);
    if (++cnt >= 4) break; // forbid doing a too long GUI output, due to noise reception
  }
  transmitter_coordinates->setAlignment(Qt::AlignRight);
  transmitter_coordinates->setText(a);

  //	if - for the first time now - we see an ecc value,
  //	we check whether or not a tii files is available
  if (!mChannel.has_ecc && (mpDabProcessor->get_ecc() != 0))
  {
    mChannel.ecc_byte = mpDabProcessor->get_ecc();
    country = find_ITU_code(mChannel.ecc_byte, (mChannel.Eid >> 12) & 0xF);
    mChannel.has_ecc = true;
    mChannel.transmitterName = "";
  }

  if ((country != "") && (country != mChannel.countryName))
  {
    transmitter_country->setText(country);
    mChannel.countryName = country;
    LOG("country", mChannel.countryName);
  }

  if (!mChannel.tiiFile)
  {
    return;
  }

  if (!(tiiChange || (mChannel.transmitterName == "")))
  {
    return;
  }

  if (mTiiHandler.is_black(mChannel.Eid, mainId, subId))
  {
    return;
  }

  const QString theName = mTiiHandler.get_transmitter_name((mChannel.realChannel ? mChannel.channelName : "any"), mChannel.Eid, mainId, subId);

  if (theName == "")
  {
    mTiiHandler.set_black(mChannel.Eid, mainId, subId);
    LOG("Not found ", QString::number(mChannel.Eid, 16) + " " + QString::number(mainId) + " " + QString::number(subId));
    return;
  }

  mChannel.transmitterName = theName;
  float latitude, longitude, power;
  mTiiHandler.get_coordinates(&latitude, &longitude, &power, mChannel.realChannel ? mChannel.channelName : "any", theName);
  mChannel.targetPos = cmplx(latitude, longitude);
  LOG("transmitter ", mChannel.transmitterName);
  LOG("coordinates ", QString::number(latitude) + " " + QString::number(longitude));
  LOG("current SNR ", QString::number(mChannel.snr));
  QString labelText = mChannel.transmitterName;
  //
  //      if our own position is known, we show the distance
  //
  float ownLatitude = real(mChannel.localPos);
  float ownLongitude = imag(mChannel.localPos);

  if ((ownLatitude == 0) || (ownLongitude == 0))
  {
    return;
  }

  const float distance = mTiiHandler.distance(latitude, longitude, ownLatitude, ownLongitude);
  const float corner = mTiiHandler.corner(latitude, longitude, ownLatitude, ownLongitude);
  const QString distanceStr = QString::number(distance, 'f', 1);
  const QString cornerStr = QString::number(corner, 'f', 1) + QString::fromLatin1("\xb0 (") + QString::fromStdString(AngleDirection::get_compass_direction(corner)) + ")";
  LOG("distance ", distanceStr);
  LOG("corner ", cornerStr);
  labelText += +" " + distanceStr + " km" + " " + cornerStr;
  fprintf(stdout, "%s\n", labelText.toUtf8().data());
  lblStationLocation->setText(labelText);

  //	see if we have a map
  if (mpHttpHandler == nullptr)
  {
    return;
  }

  uint8_t key = MAP_NORM_TRANS;
  if (!mShowOnlyCurrTrans && distance > (float)mMaxDistance)
  {
    mMaxDistance = (int)distance;
    key = MAP_MAX_TRANS;
  }

  //	to be certain, we check
  if (mChannel.targetPos == cmplx(0, 0) || (distance == 0) || (corner == 0))
  {
    return;
  }

  const QDateTime theTime = (mpSH->read(SettingHelper::cbUseUtcTime).toBool() ? QDateTime::currentDateTimeUtc() : QDateTime::currentDateTime());

  mpHttpHandler->putData(key,
                         mChannel.targetPos,
                         mChannel.transmitterName,
                         mChannel.channelName,
                         theTime.toString(Qt::TextDate),
                         mChannel.mainId * 100 + mChannel.subId,
                         (int)distance,
                         (int)corner,
                         power);
}

void RadioInterface::slot_show_spectrum(int32_t amount)
{
  if (!mIsRunning.load())
  {
    return;
  }

  mSpectrumViewer.show_spectrum(mpInputDevice->getVFOFrequency());
}

void RadioInterface::slot_show_iq(int iAmount, float iAvg)
{
  if (!mIsRunning.load())
  {
    return;
  }

  mSpectrumViewer.show_iq(iAmount, iAvg);
}

void RadioInterface::slot_show_mod_quality_data(const OfdmDecoder::SQualityData * pQD)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (!mSpectrumViewer.is_hidden())
  {
    mSpectrumViewer.show_quality(pQD->CurOfdmSymbolNo, pQD->ModQuality, pQD->TimeOffset, pQD->FreqOffset, pQD->PhaseCorr, pQD->SNR);
  }
}

void RadioInterface::slot_show_digital_peak_level(float iPeakLevel)
{
  if (!mIsRunning.load())
  {
    return;
  }

  mSpectrumViewer.show_digital_peak_level(iPeakLevel);
}

//	called from the MP4 decoder
void RadioInterface::slot_show_rs_corrections(int c, int ec)
{
  if (!mIsRunning)
  {
    return;
  }
  if (!mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->show_rsCorrections(c, ec);
  }
}

//
//	called from the DAB processor
void RadioInterface::slot_show_clock_error(float e)
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
void RadioInterface::slot_show_correlation(int amount, int marker, float threshold, const QVector<int> & v)
{
  if (!mIsRunning.load())
  {
    return;
  }

  mSpectrumViewer.show_correlation(amount, marker, threshold, v);
  mChannel.nrTransmitters = v.size();
}

////////////////////////////////////////////////////////////////////////////

void RadioInterface::slot_set_stream_selector(int k)
{
  if (!mIsRunning.load())
  {
    return;
  }

  mpSH->write(SettingHelper::soundChannel, mConfig.streamoutSelector->currentText());
  emit signal_set_audio_device(mConfig.streamoutSelector->itemData(k).toByteArray());
}

void RadioInterface::_slot_handle_tech_detail_button()
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
  mpSH->write(SettingHelper::techDataVisible, !mpTechDataWidget->isHidden());
}

// Whenever the input device is a file, some functions, e.g. selecting a channel,
// setting an alarm, are not meaningful
void RadioInterface::_show_hide_buttons(const bool iShow)
{
#if 1
  if (iShow)
  {
    mConfig.dumpButton->show();
    btnScanning->show();
    cmbChannelSelector->show();
    btnToggleFavorite->show();
  }
  else
  {
    mConfig.dumpButton->hide();
    btnScanning->hide();
    cmbChannelSelector->hide();
    btnToggleFavorite->hide();
  }
#else
  mConfig.dumpButton->setEnabled(iShow);
  mConfig.frequencyDisplay->setEnabled(iShow);
  btnScanning->setEnabled(iShow);
  channelSelector->setEnabled(iShow);
  btnToggleFavorite->setEnabled(iShow);
#endif
}

void RadioInterface::slot_set_sync_lost()
{
  // TODO: show something in the UI?
}

void RadioInterface::_slot_handle_reset_button()
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

void RadioInterface::stop_source_dumping()
{
  if (mpRawDumper == nullptr)
  {
    return;
  }

  LOG("source dump stops ", "");
  mpDabProcessor->stopDumping();
  sf_close(mpRawDumper);
  mpRawDumper = nullptr;
  setButtonFont(mConfig.dumpButton, "Raw dump", 10);
}

//
void RadioInterface::start_source_dumping()
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

void RadioInterface::_slot_handle_source_dump_button()
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

void RadioInterface::_slot_handle_audio_dump_button()
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

void RadioInterface::stop_audio_dumping()
{
  if (mAudioDumpState == EAudioDumpState::Stopped)
  {
    return;
  }

  LOG("Audio dump stops", "");
  mAudioDumpState = EAudioDumpState::Stopped;
  mWavWriter.close();
  mpTechDataWidget->audiodumpButton_text("Dump WAV", 10);
}

void RadioInterface::start_audio_dumping()
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
  LOG("Audio dump starts ", serviceLabel->text());
  mpTechDataWidget->audiodumpButton_text("Recording", 12);
  mAudioDumpState = EAudioDumpState::WaitForInit;
}

void RadioInterface::_slot_handle_frame_dump_button()
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

void RadioInterface::stop_frame_dumping()
{
  if (mChannel.currentService.frameDumper == nullptr)
  {
    return;
  }

  fclose(mChannel.currentService.frameDumper);
  mpTechDataWidget->framedumpButton_text("Dump AAC", 10);
  mChannel.currentService.frameDumper = nullptr;
}

void RadioInterface::start_frame_dumping()
{
  // TODO: what is with MP2 data streams? here only AAC is considered as filename
  mChannel.currentService.frameDumper = mOpenFileDialog.open_frame_dump_file_ptr(mChannel.currentService.serviceName);

  if (mChannel.currentService.frameDumper == nullptr)
  {
    return;
  }

  mpTechDataWidget->framedumpButton_text("Recording", 12);
}

//	called from the mp4 handler, using a signal
void RadioInterface::slot_new_frame(int amount)
{
  auto * const buffer = make_vla(uint8_t, amount);
  if (!mIsRunning.load())
  {
    return;
  }

  if (mChannel.currentService.frameDumper == nullptr)
  {
    mFrameBuffer.flush_ring_buffer();
  }
  else
  {
    while (mFrameBuffer.get_ring_buffer_read_available() >= amount)
    {
      mFrameBuffer.get_data_from_ring_buffer(buffer, amount);
      if (mChannel.currentService.frameDumper != nullptr)
      {
        fwrite(buffer, amount, 1, mChannel.currentService.frameDumper);
      }
    }
  }
}

void RadioInterface::_slot_handle_spectrum_button()
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
  mpSH->write(SettingHelper::spectrumVisible, !mSpectrumViewer.is_hidden());
}

void RadioInterface::connect_dab_processor()
{
  //	we avoided till now connecting the channel selector
  //	to the slot since that function does a lot more, things we
  //	do not want here
  connect(cmbChannelSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_handle_channel_selector);
  connect(&mSpectrumViewer, &SpectrumViewer::signal_cb_nom_carrier_changed, mpDabProcessor.get(), &DabProcessor::slot_show_nominal_carrier);
  connect(&mSpectrumViewer, &SpectrumViewer::signal_cmb_carrier_changed, mpDabProcessor.get(), &DabProcessor::slot_select_carrier_plot_type);
  connect(&mSpectrumViewer, &SpectrumViewer::signal_cmb_iqscope_changed, mpDabProcessor.get(), &DabProcessor::slot_select_iq_plot_type);
  connect(mConfig.cmbSoftBitGen, qOverload<int>(&QComboBox::currentIndexChanged), mpDabProcessor.get(), [this](int idx) { mpDabProcessor->slot_soft_bit_gen_type((ESoftBitType)idx); });

  //	Just to be sure we disconnect here.
  //	It would have been helpful to have a function
  //	testing whether or not a connection exists, we need a kind
  //	of "reset"
  disconnect(mConfig.deviceSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_do_start);
  disconnect(mConfig.deviceSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_new_device);
  connect(mConfig.deviceSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_new_device);
}
  //	When changing (or setting) a device, we do not want anybody
//	to have the buttons on the GUI touched, so
//	we just disconnect them and (re)connect them as soon as
//	a device is operational
void RadioInterface::connect_gui()
{
  connect(mConfig.contentButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_content_button);
  connect(btnTechDetails, &QPushButton::clicked, this, &RadioInterface::_slot_handle_tech_detail_button);
  connect(mConfig.resetButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_reset_button);
  connect(btnScanning, &QPushButton::clicked, this, &RadioInterface::_slot_handle_scan_button);
  connect(btnSpectrumScope, &QPushButton::clicked, this, &RadioInterface::_slot_handle_spectrum_button);
  connect(btnDeviceWidget, &QPushButton::clicked, this, &RadioInterface::_slot_handle_device_widget_button);
  connect(mConfig.dumpButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_source_dump_button);
  connect(btnPrevService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_prev_service_button);
  connect(btnNextService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_next_service_button);
  connect(btnTargetService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_target_service_button);
  connect(mpTechDataWidget, &TechData::signal_handle_audioDumping, this, &RadioInterface::_slot_handle_audio_dump_button);
  connect(mpTechDataWidget, &TechData::signal_handle_frameDumping, this, &RadioInterface::_slot_handle_frame_dump_button);
  connect(btnMuteAudio, &QPushButton::clicked, this, &RadioInterface::_slot_handle_mute_button);
  //connect(ensembleDisplay, &QListView::clicked, this, &RadioInterface::_slot_select_service);
  connect(mConfig.skipList_button, &QPushButton::clicked, this, &RadioInterface::_slot_handle_skip_list_button);
  connect(mConfig.skipFile_button, &QPushButton::clicked, this, &RadioInterface::_slot_handle_skip_file_button);
  connect(mpServiceListHandler.get(), &ServiceListHandler::signal_selection_changed, this, &RadioInterface::_slot_service_changed);
  connect(mpServiceListHandler.get(), &ServiceListHandler::signal_favorite_status, this, &RadioInterface::_slot_favorite_changed);
  connect(btnToggleFavorite, &QPushButton::clicked, this, &RadioInterface::_slot_handle_favorite_button);
}

void RadioInterface::disconnect_gui()
{
  disconnect(mConfig.contentButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_content_button);
  disconnect(btnTechDetails, &QPushButton::clicked, this, &RadioInterface::_slot_handle_tech_detail_button);
  disconnect(mConfig.resetButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_reset_button);
  disconnect(btnScanning, &QPushButton::clicked, this, &RadioInterface::_slot_handle_scan_button);
  disconnect(btnSpectrumScope, &QPushButton::clicked, this, &RadioInterface::_slot_handle_spectrum_button);
  disconnect(btnDeviceWidget, &QPushButton::clicked, this, &RadioInterface::_slot_handle_device_widget_button);
  disconnect(mConfig.dumpButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_source_dump_button);
  disconnect(btnPrevService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_prev_service_button);
  disconnect(btnNextService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_next_service_button);
  disconnect(btnTargetService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_target_service_button);
  disconnect(mpTechDataWidget, &TechData::signal_handle_audioDumping, this, &RadioInterface::_slot_handle_audio_dump_button);
  disconnect(mpTechDataWidget, &TechData::signal_handle_frameDumping, this, &RadioInterface::_slot_handle_frame_dump_button);
  disconnect(btnMuteAudio, &QPushButton::clicked, this, &RadioInterface::_slot_handle_mute_button);
  //disconnect(ensembleDisplay, &QListView::clicked, this, &RadioInterface::_slot_select_service);
  disconnect(mConfig.skipList_button, &QPushButton::clicked, this, &RadioInterface::_slot_handle_skip_list_button);
  disconnect(mConfig.skipFile_button, &QPushButton::clicked, this, &RadioInterface::_slot_handle_skip_file_button);
  disconnect(mpServiceListHandler.get(), &ServiceListHandler::signal_selection_changed, this, &RadioInterface::_slot_service_changed);
  disconnect(mpServiceListHandler.get(), &ServiceListHandler::signal_favorite_status, this, &RadioInterface::_slot_favorite_changed);
  disconnect(btnToggleFavorite, &QPushButton::clicked, this, &RadioInterface::_slot_handle_favorite_button);
}

void RadioInterface::closeEvent(QCloseEvent * event)
{
  if (mpSH->read(SettingHelper::cbCloseDirect).toBool())
  {
    _slot_terminate_process();
    event->accept();
    return;
  }

  QMessageBox::StandardButton resultButton = QMessageBox::question(this, PRJ_NAME, tr("Are you sure?\n"), QMessageBox::No | QMessageBox::Yes, QMessageBox::Yes);
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

void RadioInterface::slot_start_announcement(const QString & name, int subChId)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (name == serviceLabel->text())
  {
    _set_status_info_status(mStatusInfo.Announce, true);
    //serviceLabel->setStyleSheet("color : yellow");
    fprintf(stdout, "Announcement for %s (%d) starts\n", name.toUtf8().data(), subChId);
  }
}

void RadioInterface::slot_stop_announcement(const QString & name, int subChId)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (name == serviceLabel->text())
  {
    _set_status_info_status(mStatusInfo.Announce, false);
    fprintf(stdout, "Announcement for %s (%d) stops\n", name.toUtf8().data(), subChId);
  }
}

// called from the service list handler when only a service has changed
void RadioInterface::_slot_select_service(QModelIndex ind)
{
  const QString currentProgram = ind.data(Qt::DisplayRole).toString();
  local_select(mChannel.channelName, currentProgram);
}

// called from the service list handler when channel and service has changed
void RadioInterface::_slot_service_changed(const QString & iChannel, const QString & iService)
{
  local_select(iChannel, iService);
}

// called from the service list handler when when the favorite state has changed
void RadioInterface::_slot_favorite_changed(const bool iIsFav)
{
  mCurFavoriteState = iIsFav;
  set_favorite_button_style();
}

//	selecting from a content description
void RadioInterface::slot_handle_content_selector(const QString & s)
{
  local_select(mChannel.channelName, s);
}

void RadioInterface::local_select(const QString & theChannel, const QString & service)
{
  if (mpDabProcessor == nullptr)  // should not happen
  {
    return;
  }

  stop_service(mChannel.currentService);

  QString serviceName = service;
  serviceName.resize(16, ' '); // fill up to 16 spaces

  // Is it only a service change within the same channel?
  if (theChannel == mChannel.channelName)
  {
    mChannel.currentService.valid = false;
    SDabService s;
    mpDabProcessor->getParameters(serviceName, &s.SId, &s.SCIds);
    if (s.SId == 0)
    {
      write_warning_message("Insufficient data for this program (1)");
      return;
    }
    s.serviceName = service; // TODO: service or serviceName?
    start_service(s);
    return;
  }

  // The hard part is stopping the current service, quitting the current channel, selecting a new channel, waiting a while
  stop_channel();
  //      and start the new channel first
  int k = cmbChannelSelector->findText(theChannel);
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
  mChannel.nextService.channel = theChannel;
  mChannel.nextService.serviceName = serviceName;
  mChannel.nextService.SId = 0;
  mChannel.nextService.SCIds = 0;

  // _trigger_preset_timer();

  start_channel(cmbChannelSelector->currentText());
}

////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////

void RadioInterface::stop_service(SDabService & ioDabService)
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

      for (int32_t i = 0; i < 5; ++i) // TODO: from where the 5?
      {
        Packetdata pd;
        mpDabProcessor->dataforPacketService(ioDabService.serviceName, &pd, i);

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

void RadioInterface::start_service(SDabService & s)
{
  QString serviceName = s.serviceName;

  mChannel.currentService = s;
  mChannel.currentService.frameDumper = nullptr;
  mChannel.currentService.valid = false;
  LOG("start service ", serviceName.toUtf8().data());
  LOG("service has SNR ", QString::number(mChannel.snr));
  //
  //	mark the selected service in the service list
  //int rowCount = model.rowCount();
  //(void)rowCount;

  //colorServiceName(serviceName, Qt::red, 16, true);
  mpServiceListHandler->set_selector(mChannel.channelName, serviceName);

  //	and display the servicename on the serviceLabel
  QFont font = serviceLabel->font();
  font.setPointSize(16);
  font.setBold(true);
  serviceLabel->setStyleSheet("QLabel { color: lightblue; }");
  serviceLabel->setFont(font);
  serviceLabel->setText(serviceName);
  lblDynLabel->setText("");
  //lblDynLabel->setOpenExternalLinks(true); // TODO: make this work (Bayern1 sent link)

  Audiodata ad;
  mpDabProcessor->dataforAudioService(serviceName, &ad);

  if (ad.defined)
  {
    mChannel.currentService.valid = true;
    mChannel.currentService.is_audio = true;
    mChannel.currentService.subChId = ad.subchId;
    if (mpDabProcessor->has_timeTable(ad.SId))
    {
      mpTechDataWidget->show_timetableButton(true);
    }

    startAudioservice(&ad);
    const QString csn = mChannel.channelName + ":" + serviceName;
    mpSH->write(SettingHelper::presetName, csn);

#ifdef HAVE_PLUTO_RXTX
    if (streamerOut != nullptr)
    {
      streamerOut->addRds(std::string(serviceName.toUtf8().data()));
    }
#endif
  }
  else if (mpDabProcessor->is_packetService(serviceName))
  {
    Packetdata pd;
    mpDabProcessor->dataforPacketService(serviceName, &pd, 0);
    mChannel.currentService.valid = true;
    mChannel.currentService.is_audio = false;
    mChannel.currentService.subChId = pd.subchId;
    startPacketservice(serviceName);
  }
  else
  {
    write_warning_message("Insufficient data for this program (2)");
    mpSH->write(SettingHelper::presetName, "");
  }
}

//void RadioInterface::colorService(QModelIndex ind, QColor c, int pt, bool italic)
//{
//  QMap<int, QVariant> vMap = model.itemData(ind);
//  vMap.insert(Qt::ForegroundRole, QVariant(QBrush(c)));
//  model.setItemData(ind, vMap);
//  model.setData(ind, QFont(theFont, pt, -1, italic), Qt::FontRole);
//}

void RadioInterface::startAudioservice(Audiodata * ad)
{
  mChannel.currentService.valid = true;

  (void)mpDabProcessor->set_audio_channel(ad, &mAudioBufferFromDecoder, nullptr, FORE_GROUND);
  for (int i = 1; i < 10; i++)
  {
    Packetdata pd;
    mpDabProcessor->dataforPacketService(ad->serviceName, &pd, i);
    if (pd.defined)
    {
      mpDabProcessor->set_data_channel(&pd, &mDataBuffer, FORE_GROUND);
      fprintf(stdout, "adding %s (%d) as subservice\n", pd.serviceName.toUtf8().data(), pd.subchId);
      break;
    }
  }
  //	activate sound
  mpCurAudioFifo = nullptr; // trigger possible new sample rate setting
  //mpSoundOut->restart();
  programTypeLabel->setText(getProgramType(ad->programType));
  //	show service related data
  mpTechDataWidget->show_serviceData(ad);
  _set_status_info_status(mStatusInfo.BitRate, (int32_t)(ad->bitRate));
}

void RadioInterface::startPacketservice(const QString & s)
{
  Packetdata pd;

  mpDabProcessor->dataforPacketService(s, &pd, 0);
  if ((!pd.defined) || (pd.DSCTy == 0) || (pd.bitRate == 0))
  {
    write_warning_message("Insufficient data for this program (3)");
    return;
  }

  if (!mpDabProcessor->set_data_channel(&pd, &mDataBuffer, FORE_GROUND))
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
void RadioInterface::clean_screen()
{
  serviceLabel->setText("");
  lblDynLabel->setStyleSheet("color: white");
  lblDynLabel->setText("");
  mpTechDataWidget->cleanUp();
  programTypeLabel->setText("");
  mpTechDataWidget->cleanUp();
  _reset_status_info();
}

////////////////////////////////////////////////////////////////////////////
//
//	next and previous service selection
////////////////////////////////////////////////////////////////////////////

void RadioInterface::_slot_handle_prev_service_button()
{
  mpServiceListHandler->jump_entries(-1);
//  disconnect(btnPrevService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_prev_service_button);
//  handle_serviceButton(BACKWARDS);
//  connect(btnPrevService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_prev_service_button);
}

void RadioInterface::_slot_handle_next_service_button()
{
  mpServiceListHandler->jump_entries(+1);
//  disconnect(btnNextService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_next_service_button);
//  handle_serviceButton(FORWARD);
//  connect(btnNextService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_next_service_button);
}

void RadioInterface::_slot_handle_target_service_button()
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
void RadioInterface::_slot_preset_timeout()
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


  QStringList serviceList;

  if ((rand() & 1) == 0) serviceList << QString((char)('a' + (rand() % 26))) + QString::number(rand() & 0xFFFF, 16);

  for (const auto & sl : mServiceList)
  {
    const bool isAudio = mpDabProcessor->is_audioService(sl.name);
    // qDebug() << sl.name << QString::number(sl.SId, 16) << isAudio;

    if (isAudio)
    {
      serviceList << sl.name;
    }
  }

  if ((rand() & 1) == 0) serviceList << QString((char)('A' + (rand() % 26))) + QString::number(rand() & 0xFFFF, 16);

  mpServiceListHandler->update_services_at_channel(mChannel.channelName, serviceList);

  QString presetName = mChannel.nextService.serviceName;
  presetName.resize(16, ' '); // fill up to 16 spaces

  SDabService s;
  s.serviceName = presetName;
  mpDabProcessor->getParameters(presetName, &s.SId, &s.SCIds);

  if (s.SId == 0)
  {
    write_warning_message("Insufficient data for this program (5)");
    return;
  }

  mChannel.nextService.valid = false;
  start_service(s);
}

void RadioInterface::write_warning_message(const QString & iMsg)
{
  lblDynLabel->setStyleSheet("color: #ff9100");
  lblDynLabel->setText(iMsg);
}

///////////////////////////////////////////////////////////////////////////
//
//	Channel basics
///////////////////////////////////////////////////////////////////////////
//	Precondition: no channel should be active
//
void RadioInterface::start_channel(const QString & iChannel)
{
  const int32_t tunedFrequencyHz = mBandHandler.get_frequency_Hz(iChannel);
  LOG("channel starts ", iChannel);
  mSpectrumViewer.show_nominal_frequency_MHz((float)tunedFrequencyHz / 1'000'000.0f);
  mpInputDevice->resetBuffer();
  mServiceList.clear();
  mpInputDevice->restartReader(tunedFrequencyHz);
  mChannel.clean_channel();
  mChannel.channelName = iChannel;
  mChannel.nominalFreqHz = tunedFrequencyHz;

  mpServiceListHandler->set_selector_channel_only(mChannel.channelName);

  if (mShowOnlyCurrTrans && mpHttpHandler != nullptr)
  {
    mpHttpHandler->putData(MAP_RESET, cmplx(0, 0), "", "", "", 0, 0, 0, 0);
  }
  else if (mpHttpHandler != nullptr)
  {
    mpHttpHandler->putData(MAP_FRAME, cmplx(-1, -1), "", "", "", 0, 0, 0, 0);
  }

  enable_ui_elements_for_safety(!mIsScanning);

  mpDabProcessor->start();

  if (!mIsScanning)
  {
    mpSH->write(SettingHelper::channel, iChannel);
    // const int32_t switchDelay = mpSH->read(SettingHelper::switchDelay).toInt();
    mEpgTimer.start(cEpgTimeoutMs);
  }
}

//
//	apart from stopping the reader, a lot of administration
//	is to be done.
void RadioInterface::stop_channel()
{
  if (mpInputDevice == nullptr)
  {    // should not happen
    return;
  }
  stop_etiHandler();
  LOG("channel stops ", mChannel.channelName);
  //
  //	first, stop services in fore and background
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
  //mpSoundOut->stop();

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
    mpDabProcessor->stop_ficDump();
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
  mChannel.targetPos = cmplx(0, 0);
  if (mShowOnlyCurrTrans && mpHttpHandler != nullptr)
  {
    mpHttpHandler->putData(MAP_RESET, mChannel.targetPos, "", "", "", 0, 0, 0, 0);
  }
  transmitter_country->setText("");
  transmitter_coordinates->setText("");

  enable_ui_elements_for_safety(false);  // hide some buttons
  QCoreApplication::processEvents();

  //	no processing left at this time
  usleep(1000);    // may be handling pending signals?
  mChannel.currentService.valid = false;
  mChannel.nextService.valid = false;

  //	all stopped, now look at the GUI elements
  progBarFicError->setValue(0);
  //	the visual elements related to service and channel
  slot_set_synced(false);
  ensembleId->setText("");
  transmitter_coordinates->setText(" ");

  mServiceList.clear();
  clean_screen();
  _show_epg_label(false);
  lblStationLocation->setText("");
}

//
/////////////////////////////////////////////////////////////////////////
//
//	next- and previous channel buttons
/////////////////////////////////////////////////////////////////////////

void RadioInterface::_slot_handle_channel_selector(const QString & channel)
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

void RadioInterface::_slot_handle_scan_button()
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

void RadioInterface::start_scanning()
{
  mPresetTimer.stop();
  mChannelTimer.stop();
  mEpgTimer.stop();
  connect(mpDabProcessor.get(), &DabProcessor::signal_no_signal_found, this, &RadioInterface::slot_no_signal_found);
  stop_channel();
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
  lblDynLabel->setText("Scanning channel " + cmbChannelSelector->currentText());
  btnScanning->start_animation();
  mChannelTimer.start(cChannelTimeoutMs);

  start_channel(cmbChannelSelector->currentText());
}

//	stop_scanning is called
//	1. when the scanbutton is touched during scanning
//	2. on user selection of a service or a channel select
//	3. on device selection
//	4. on handling a reset
void RadioInterface::stop_scanning()
{
  disconnect(mpDabProcessor.get(), &DabProcessor::signal_no_signal_found, this, &RadioInterface::slot_no_signal_found);

  if (mIsScanning.load())
  {
    btnScanning->stop_animation();
    LOG("scanning stops ", "");
    mpDabProcessor->set_scan_mode(false);
    lblDynLabel->setText("Scan ended\nSelect a service on the left");
    mChannelTimer.stop();
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

void RadioInterface::slot_no_signal_found()
{
  disconnect(mpDabProcessor.get(), &DabProcessor::signal_no_signal_found, this, &RadioInterface::slot_no_signal_found);
  mChannelTimer.stop();
  disconnect(&mChannelTimer, &QTimer::timeout, this, &RadioInterface::_slot_channel_timeout);

  if (mIsRunning.load() && mIsScanning.load())
  {
    int cc = cmbChannelSelector->currentIndex();
    // if (!mServiceList.empty())
    // {
    //   showServices();
    // }
    stop_channel();
    cc = mBandHandler.nextChannel(cc);
    fprintf(stdout, "going to channel %d\n", cc);

    if (cc >= cmbChannelSelector->count())
    {
      stop_scanning();
    }
    else
    {  
      //	To avoid reaction of the system on setting a different value:
      _update_channel_selector(cc);

      connect(mpDabProcessor.get(), &DabProcessor::signal_no_signal_found, this, &RadioInterface::slot_no_signal_found);
      connect(&mChannelTimer, &QTimer::timeout, this, &RadioInterface::_slot_channel_timeout);

      lblDynLabel->setText("Scanning channel " + cmbChannelSelector->currentText());
      mChannelTimer.start(cChannelTimeoutMs);
      start_channel(cmbChannelSelector->currentText());
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
std::vector<SServiceId> RadioInterface::insert_sorted(const std::vector<SServiceId> & iList, const SServiceId & iEntry)
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
void RadioInterface::enable_ui_elements_for_safety(const bool iEnable)
{
  mConfig.dumpButton->setEnabled(iEnable);
  btnToggleFavorite->setEnabled(iEnable);
  btnPrevService->setEnabled(iEnable);
  btnNextService->setEnabled(iEnable);
  cmbChannelSelector->setEnabled(iEnable);
  mConfig.contentButton->setEnabled(iEnable);
}

void RadioInterface::_slot_handle_mute_button()
{
  mMutingActive = !mMutingActive;
  btnMuteAudio->setIcon(QIcon(mMutingActive ? ":res/icons/muted24.png" : ":res/icons/unmuted24.png"));
  btnMuteAudio->setIconSize(QSize(24, 24));
  btnMuteAudio->setFixedSize(QSize(32, 32));
  signal_audio_mute(mMutingActive);
}

void RadioInterface::_update_channel_selector(int index)
{
  if (cmbChannelSelector->currentIndex() == index)
  {
    return;
  }

  disconnect(cmbChannelSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_handle_channel_selector);
  cmbChannelSelector->blockSignals(true);
  emit signal_set_new_channel(index);

  while (cmbChannelSelector->currentIndex() != index)
  {
    usleep(2000);
  }
  cmbChannelSelector->blockSignals(false);
  connect(cmbChannelSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_handle_channel_selector);
}


/////////////////////////////////////////////////////////////////////////
//	External configuration items				//////

//-------------------------------------------------------------------------
//------------------------------------------------------------------------
//
//	if configured, the interpretation of the EPG data starts automatically,
//	the servicenames of an SPI/EPG service may differ from one country
//	to another
void RadioInterface::slot_epg_timer_timeout()
{
  mEpgTimer.stop();

  if (!mpSH->read(SettingHelper::epgFlag).toBool())
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
      mpDabProcessor->dataforPacketService(serv.name, &pd, 0);

      if ((!pd.defined) || (pd.DSCTy == 0) || (pd.bitRate == 0))
      {
        continue;
      }

      if (pd.DSCTy == 60)
      {
        LOG("hidden service started ", serv.name);
        _show_epg_label(true);
        fprintf(stdout, "Starting hidden service %s\n", serv.name.toUtf8().data());
        mpDabProcessor->set_data_channel(&pd, &mDataBuffer, BACK_GROUND);
        SDabService s;
        s.channel = pd.channel;
        s.serviceName = pd.serviceName;
        s.SId = pd.SId;
        s.SCIds = pd.SCIds;
        s.subChId = pd.subchId;
        s.fd = nullptr;
        mChannel.backgroundServices.push_back(s);

//        for (int j = 0; j < model.rowCount(); j++)
//        {
//          QString itemText = model.index(j, 0).data(Qt::DisplayRole).toString();
//          if (itemText == pd.serviceName)
//          {
//            colorService(model.index(j, 0), Qt::blue, fontSize + 2, true);
//            break;
//          }
//        }
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

uint32_t RadioInterface::extract_epg(const QString& iName, const std::vector<SServiceId> & iServiceList, uint32_t ensembleId)
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

void RadioInterface::slot_set_epg_data(int SId, int theTime, const QString & theText, const QString & theDescr)
{
  if (mpDabProcessor != nullptr)
  {
    mpDabProcessor->set_epgData(SId, theTime, theText, theDescr);
  }
}

void RadioInterface::_slot_handle_time_table()
{
  int epgWidth;
  if (!mChannel.currentService.valid || !mChannel.currentService.is_audio)
  {
    return;
  }

  mpTimeTable->clear();
  epgWidth = mpSH->read(SettingHelper::epgWidth).toInt();
  if (epgWidth < 50)
  {
    epgWidth = 50;
  }
  std::vector<SEpgElement> res = mpDabProcessor->find_epgData(mChannel.currentService.SId);
  for (const auto & element: res)
  {
    mpTimeTable->addElement(element.theTime, epgWidth, element.theText, element.theDescr);
  }
}

void RadioInterface::_slot_handle_skip_list_button()
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

void RadioInterface::_slot_handle_skip_file_button()
{
  const QString fileName = mOpenFileDialog.get_skip_file_file_name();
  mBandHandler.saveSettings();
  mBandHandler.setup_skipList(fileName);
}

void RadioInterface::slot_handle_tii_detector_mode(bool iIsChecked)
{
  assert(mpDabProcessor != nullptr);
  mpDabProcessor->set_tiiDetectorMode(iIsChecked);
  mChannel.transmitters.clear();
}

void RadioInterface::slot_handle_dc_avoidance_algorithm(bool iIsChecked)
{
  assert(mpDabProcessor != nullptr);
  mpDabProcessor->set_dc_avoidance_algorithm(iIsChecked);
}

void RadioInterface::slot_handle_dc_removal(bool iIsChecked)
{
  assert(mpDabProcessor != nullptr);
  mpDabProcessor->set_dc_removal(iIsChecked);
}

void RadioInterface::slot_handle_dl_text_button()
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


void RadioInterface::_slot_handle_config_button()
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

void RadioInterface::slot_nr_services(int n)
{
  mChannel.serviceCount = n; // only a minor channel transmit this information, so do not rely on this
}

void RadioInterface::LOG(const QString & a1, const QString & a2)
{
  if (mpLogFile == nullptr)
  {
    return;
  }

  QString theTime;
  if (mpSH->read(SettingHelper::cbUseUtcTime).toBool())
  {
    theTime = convertTime(mUTC.year, mUTC.month, mUTC.day, mUTC.hour, mUTC.minute);
  }
  else
  {
    theTime = QDateTime::currentDateTime().toString();
  }

  fprintf(mpLogFile, "at %s: %s %s\n", theTime.toUtf8().data(), a1.toUtf8().data(), a2.toUtf8().data());
}

void RadioInterface::slot_handle_logger_button(int)
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

void RadioInterface::slot_handle_set_coordinates_button()
{
  Coordinates theCoordinator(mpSH->get_settings());
  (void)theCoordinator.QDialog::exec();
  float local_lat = mpSH->read(SettingHelper::latitude).toFloat();
  float local_lon = mpSH->read(SettingHelper::longitude).toFloat();
  mChannel.localPos = cmplx(local_lat, local_lon);
}

void RadioInterface::slot_load_table()
{
  QString tableFile = mpSH->read(SettingHelper::tiiFile).toString();

  if (tableFile.isEmpty())
  {
    tableFile = QDir::homePath() + "/.txdata.tii";
    mpSH->write(SettingHelper::tiiFile, tableFile);
  }

  mTiiHandler.loadTable(tableFile);

  if (mTiiHandler.is_valid())
  {
    QMessageBox::information(this, tr("success"), tr("Loading and installing database complete\n"));
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
void RadioInterface::_slot_handle_http_button()
{
  if (real(mChannel.localPos) == 0)
  {
    QMessageBox::information(this, "Data missing", "Provide your coordinates first on the configuration window!");
    return;
  }

  if (mpHttpHandler == nullptr)
  {
    QString browserAddress = mpSH->read(SettingHelper::browserAddress).toString();
    QString mapPort = mpSH->read(SettingHelper::mapPort).toString();

    QString mapFile;
    if (mpSH->read(SettingHelper::cbSaveTransToCsv).toBool())
    {
      mapFile = mOpenFileDialog.get_maps_file_name();
    }
    else
    {
      mapFile = "";
    }
    mpHttpHandler = new httpHandler(this,
                                    mapPort,
                                    browserAddress,
                                    mChannel.localPos,
                                    mapFile,
                                    mpSH->read(SettingHelper::cbManualBrowserStart).toBool());
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

void RadioInterface::slot_handle_transmitter_tags(int /*d*/)
{
  mMaxDistance = -1;
  mShowOnlyCurrTrans = mpSH->read(SettingHelper::cbShowOnlyCurrTrans).toBool();
  mChannel.targetPos = cmplx(0, 0);

  if (mShowOnlyCurrTrans && mpHttpHandler != nullptr)
  {
    mpHttpHandler->putData(MAP_RESET, mChannel.targetPos, "", "", "", 0, 0, 0, 0);
  }
}

void RadioInterface::show_pause_slide()
{
  QPixmap p;
  const char * const picFile = ((rand() & 1) != 0 ? ":res/logo/dabinvlogo.png" : ":res/logo/dabstar320x240.png");
  if (p.load(picFile, "png"))
  {
    write_picture(p);
  }
}

void RadioInterface::slot_handle_port_selector()
{
  mapPortHandler theHandler(mpSH->get_settings());
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

void RadioInterface::_slot_handle_eti_handler()
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

void RadioInterface::stop_etiHandler()
{
  if (!mChannel.etiActive)
  {
    return;
  }

  mpDabProcessor->stop_etiGenerator();
  mChannel.etiActive = false;
  btnScanning->setText("ETI");
}

void RadioInterface::start_etiHandler()
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
  mChannel.etiActive = mpDabProcessor->start_etiGenerator(etiFile);
  if (mChannel.etiActive)
  {
    btnScanning->setText("eti runs");
  }
}

void RadioInterface::slot_handle_eti_active_selector(int /*k*/)
{
  if (mpInputDevice == nullptr)
  {
    return;
  }

  if (mConfig.cbActivateEti->isChecked())
  {
    stop_scanning();
    disconnect(btnScanning, &QPushButton::clicked, this, &RadioInterface::_slot_handle_scan_button);
    connect(btnScanning, &QPushButton::clicked, this, &RadioInterface::_slot_handle_eti_handler);
    btnScanning->setText("ETI");

    if (mpInputDevice->isFileInput()) // restore the button's visibility
    {
      btnScanning->show();
    }

    return;
  }
  //	otherwise, disconnect the eti handling and reconnect scan
  //	be careful, an ETI session may be going on
  stop_etiHandler();    // just in case
  disconnect(btnScanning, &QPushButton::clicked, this, &RadioInterface::_slot_handle_eti_handler);
  connect(btnScanning, &QPushButton::clicked, this, &RadioInterface::_slot_handle_scan_button);
  btnScanning->setText("Scan");
  if (mpInputDevice->isFileInput())
  {  // hide the button now
    btnScanning->hide();
  }
}

void RadioInterface::slot_test_slider(int iVal) // iVal 0..1000
{
  emit signal_test_slider_changed(iVal);
}

QStringList RadioInterface::get_soft_bit_gen_names()
{
  QStringList sl;

  // ATTENTION: use same sequence as in ESoftBitType
  sl << "Log Likelihood Ratio";        // ESoftBitType::LLR
  sl << "Avr. Soft-Bit Gen.";          // ESoftBitType::AVER
  sl << "Fast Soft-Bit Gen.";          // ESoftBitType::FAST
  sl << "Euclidean distance";          // ESoftBitType::EUCL_DIST
  sl << "Eucl. dist. (ext. gain)";     // ESoftBitType::EUCL_DIST_2
  sl << "Max. IQ distance";            // ESoftBitType::MAX_DIST_IQ
  sl << "Hard decision";               // ESoftBitType::HARD

  return sl;
}

QString RadioInterface::get_bg_style_sheet(const QColor & iBgBaseColor, const char * const iWidgetType /*= nullptr*/)
{
  const float fac = 0.6f;
  const int32_t r1 = iBgBaseColor.red();
  const int32_t g1 = iBgBaseColor.green();
  const int32_t b1 = iBgBaseColor.blue();
  const int32_t r2 = (int32_t)((float)r1 * fac);
  const int32_t g2 = (int32_t)((float)g1 * fac);
  const int32_t b2 = (int32_t)((float)b1 * fac);
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

void RadioInterface::setup_ui_colors()
{
  btnMuteAudio->setStyleSheet(get_bg_style_sheet({ 255, 60, 60 }));

  btnScanning->setStyleSheet(get_bg_style_sheet({ 100, 100, 255 }));
  btnDeviceWidget->setStyleSheet(get_bg_style_sheet({ 87, 230, 236 }));
  configButton->setStyleSheet(get_bg_style_sheet({ 80, 155, 80 }));
  btnTechDetails->setStyleSheet(get_bg_style_sheet({ 255, 255, 100 }));
  btnSpectrumScope->setStyleSheet(get_bg_style_sheet({ 197, 69, 240 }));
  btnPrevService->setStyleSheet(get_bg_style_sheet({ 200, 97, 40 }));
  btnNextService->setStyleSheet(get_bg_style_sheet({ 200, 97, 40 }));
  btnTargetService->setStyleSheet(get_bg_style_sheet({ 33, 106, 105 }));
  btnToggleFavorite->setStyleSheet(get_bg_style_sheet({ 100, 100, 255 }));

  _set_http_server_button(false);
}

void RadioInterface::_set_http_server_button(const bool iActive)
{
  btnHttpServer->setStyleSheet(get_bg_style_sheet((iActive ? 0xf97903 : 0x45bb24)));
  btnHttpServer->setFixedSize(QSize(32, 32));
}

void RadioInterface::_slot_handle_favorite_button(bool iClicked)
{
  mCurFavoriteState = !mCurFavoriteState;
  set_favorite_button_style();
  mpServiceListHandler->set_favorite_state(mCurFavoriteState);
}

void RadioInterface::set_favorite_button_style()
{
  btnToggleFavorite->setIcon(QIcon(mCurFavoriteState ? ":res/icons/starfilled24.png" : ":res/icons/starempty24.png"));
  btnToggleFavorite->setIconSize(QSize(24, 24));
  btnToggleFavorite->setFixedSize(QSize(32, 32));
}

void RadioInterface::_slot_set_static_button_style()
{
  btnPrevService->setIconSize(QSize(24, 24));
  btnPrevService->setFixedSize(QSize(32, 32));
  btnNextService->setIconSize(QSize(24, 24));
  btnNextService->setFixedSize(QSize(32, 32));
  btnTargetService->setIconSize(QSize(24, 24));
  btnTargetService->setFixedSize(QSize(32, 32));
  btnTechDetails->setIconSize(QSize(24, 24));
  btnTechDetails->setFixedSize(QSize(32, 32));
  btnHttpServer->setIconSize(QSize(24, 24));
  btnHttpServer->setFixedSize(QSize(32, 32));
  btnDeviceWidget->setIconSize(QSize(24, 24));
  btnDeviceWidget->setFixedSize(QSize(32, 32));
  btnSpectrumScope->setIconSize(QSize(24, 24));
  btnSpectrumScope->setFixedSize(QSize(32, 32));
  configButton->setIconSize(QSize(24, 24));
  configButton->setFixedSize(QSize(32, 32));
  btnScanning->setIconSize(QSize(24, 24));
  btnScanning->setFixedSize(QSize(32, 32));
  btnScanning->init(":res/icons/scan24.png", 3, 1);
}

void RadioInterface::_create_status_info()
{
  layoutStatus->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

  _add_status_label_elem(mStatusInfo.BitRate,  0x40c6db, "0 kBit/s", "Input bitrate of AAC audio decoder");
  _add_status_label_elem(mStatusInfo.Stereo,   0xf2c629, "Stereo",   "Stereo");
  _add_status_label_elem(mStatusInfo.EPG,      0xf2c629, "EPG",      "Electronic Program Guide");
  _add_status_label_elem(mStatusInfo.SBR,      0xf2c629, "SBR",      "Spectral Band Replication");
  _add_status_label_elem(mStatusInfo.PS,       0xf2c629, "PS",       "Parametric Stereo");
  _add_status_label_elem(mStatusInfo.Announce, 0xf2c629, "Announce", "Announcement");

  layoutStatus->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

  _reset_status_info();
}

template<typename T>
void RadioInterface::_add_status_label_elem(StatusInfoElem<T> & ioElem, const uint32_t iColor, const QString & iName, const QString & iToolTip)
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

  layoutStatus->addWidget(ioElem.pLbl);
}

template<typename T>
void RadioInterface::_set_status_info_status(StatusInfoElem<T> & iElem, const T iValue)
{
  if (iElem.value != iValue)
  {
    iElem.value = iValue;
    iElem.pLbl->setStyleSheet(iElem.value ? "QLabel { color: " + iElem.colorOn.name() + " }"
                                          : "QLabel { color: " + iElem.colorOff.name() + " }");

    if (!std::is_same<T, bool>::value)
    {
      iElem.pLbl->setText(QString::number(iValue) + " kBit/s"); // not clean to place the unit here but it is the only one yet
    }
  }
}

void RadioInterface::_reset_status_info()
{
  _set_status_info_status(mStatusInfo.BitRate, 0);
  _set_status_info_status(mStatusInfo.Stereo, false);
  _set_status_info_status(mStatusInfo.EPG, false);
  _set_status_info_status(mStatusInfo.SBR, false);
  _set_status_info_status(mStatusInfo.PS, false);
  _set_status_info_status(mStatusInfo.Announce, false);
}

void RadioInterface::_set_device_to_file_mode(const bool iDataFromFile)
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

void RadioInterface::_setup_audio_output(const uint32_t iSampleRate)
{
  mAudioBufferFromDecoder.flush_ring_buffer();
  mAudioBufferToOutput.flush_ring_buffer();
  mpCurAudioFifo = &mAudioFifo;
  mpCurAudioFifo->sampleRate = iSampleRate;
  mpCurAudioFifo->pRingbuffer = &mAudioBufferToOutput;

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

void RadioInterface::_slot_load_audio_device_list(const QList<QAudioDevice> & iDeviceList) const
{
  mConfig.streamoutSelector->clear();
  for (const QAudioDevice &device : iDeviceList)
  {
    mConfig.streamoutSelector->addItem(device.description(), QVariant::fromValue(device.id()));
  }
}

void RadioInterface::slot_show_audio_peak_level(const float iPeakLeft, const float iPeakRight)
{
  auto decay = [](float iPeak, float & ioPeakAvr) -> void
  {
    ioPeakAvr = (iPeak >= ioPeakAvr ? iPeak : ioPeakAvr - 0.5f /*decay*/);
  };

  decay(iPeakLeft, mPeakLeftDamped);
  decay(iPeakRight, mPeakRightDamped);

  thermoPeakLevelLeft->setValue(mPeakLeftDamped);
  thermoPeakLevelRight->setValue(mPeakRightDamped);
}

void RadioInterface::clean_up()
{
  // TODO: resetting peak meters is not working well after service change
  mPeakLeftDamped = mPeakRightDamped = -40.0f;
  slot_show_audio_peak_level(-40.0f, -40.0);
  progBarFicError->setValue(0);
  progBarAudioBuffer->setValue(0);
}
