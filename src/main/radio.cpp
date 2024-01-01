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
#include  <fstream>
#include  <numeric>
#include  <unistd.h>
#include  <vector>
#include  <QCoreApplication>
#include  <QSettings>
#include  <QMessageBox>
#include  <QFileDialog>
#include  <QDateTime>
#include  <QStringList>
#include  <QStringListModel>
#include  <QMouseEvent>
#include  <QDir>
#include  <QSpacerItem>
#include  "dab-constants.h"
#include  "mot-content-types.h"
#include  "radio.h"
#include  "rawfiles.h"
#include  "wavfiles.h"
#include  "xml-filereader.h"
#include  "dab-tables.h"
#include  "ITU_Region_1.h"
#include  "coordinates.h"
#include  "mapport.h"
#include  "techdata.h"
#include  "dummy-handler.h"
#include  "service-list-handler.h"

#ifdef  TCP_STREAMER
  #include "tcp-streamer.h"
#elif  QT_AUDIO
  #include "Qt-audio.h"
#else
  #include "audiosink.h"
#endif

#include  "time-table.h"
#include  "device-exceptions.h"

#ifdef  __MINGW32__
#include <windows.h>

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

static const char sDEFAULT_SERVICE_LABEL_STYLE[] = "QLabel { color: lightblue; }";

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

//static int32_t sFreqOffHz = 0; // for test

RadioInterface::RadioInterface(QSettings * Si, const QString & dbFileName, const QString & freqExtension, bool error_report, int32_t dataPort, int32_t clockPort, int fmFrequency, QWidget * parent)
  : QWidget(parent),
    spectrumBuffer(2048),
    iqBuffer(2 * 1536),
    carrBuffer(2 * 1536),
    responseBuffer(32768),
    frameBuffer(2 * 32768),
    dataBuffer(32768),
    audioBuffer(8 * 32768),
    my_spectrumViewer(this, Si, &spectrumBuffer, &iqBuffer, &carrBuffer, &responseBuffer),
    theBand(freqExtension, Si),
    configDisplay(nullptr),
    the_dlCache(10),
    tiiHandler(),
    filenameFinder(Si),
    theTechData(16 * 32768)
{
  int16_t k;
  QString h;

  dabSettings = Si;
  this->error_report = error_report;
  this->fmFrequency = fmFrequency;
  this->dlTextFile = nullptr;
  this->ficDumpPointer = nullptr;
  running.store(false);
  scanning.store(false);
  my_dabProcessor = nullptr;
  maxDistance = -1;
  my_contentTable = nullptr;
  //my_scanTable = nullptr;
  mapHandler = nullptr;
  //	"globals" is introduced to reduce the number of parameters
  //	for the dabProcessor
  globals.spectrumBuffer = &spectrumBuffer;
  globals.iqBuffer = &iqBuffer;
  globals.carrBuffer = &carrBuffer;
  globals.responseBuffer = &responseBuffer;
  //globals.snrBuffer = &snrBuffer;
  globals.frameBuffer = &frameBuffer;

  const int16_t latency = dabSettings->value("latency", 5).toInt();

  globals.dabMode = dabSettings->value("dabMode", 1).toInt();
  globals.threshold = dabSettings->value("threshold", 3).toInt();
  globals.diff_length = dabSettings->value("diff_length", DIFF_LENGTH).toInt();
  globals.tii_delay = dabSettings->value("tii_delay", 5).toInt();
  if (globals.tii_delay < 2)
  {
    globals.tii_delay = 2;
  }
  globals.tii_depth = dabSettings->value("tii_depth", 4).toInt();
  globals.echo_depth = dabSettings->value("echo_depth", 1).toInt();

  theFont = dabSettings->value("theFont", "Times").toString();
  fontSize = dabSettings->value("fontSize", 12).toInt();

  //	set on top or not? checked at start up
  if (dabSettings->value("onTop", 0).toInt() == 1)
  {
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
  }

  //	The settings are done, now creation of the GUI parts
  setupUi(this);
  setFixedSize(710, 470+20);
  setup_ui_colors();
  _create_status_info();

  // only the queued call will consider the button size?!
  QMetaObject::invokeMethod(this, &RadioInterface::_slot_handle_mute_button, Qt::QueuedConnection);
  QMetaObject::invokeMethod(this, &RadioInterface::_slot_set_static_button_style, Qt::QueuedConnection);
  QMetaObject::invokeMethod(this, "_slot_favorite_changed", Qt::QueuedConnection, Q_ARG(bool, false)); // only this works with arguments

  //setWindowIcon(QIcon(":res/DABplusLogoWB.png"));

  //setWindowTitle(QString(PRJ_NAME) + QString(" (V" PRJ_VERS ")"));
  setWindowTitle(PRJ_NAME);
  int x = dabSettings->value("mainWidget-x", 100).toInt();
  int y = dabSettings->value("mainWidget-y", 100).toInt();
  int wi = dabSettings->value("main-widget-w", 300).toInt();
  int he = dabSettings->value("main-widget-h", 200).toInt();
  this->resize(QSize(wi, he));
  this->move(QPoint(x, y));

  configWidget.setupUi(&configDisplay);
  configDisplay.setFixedSize(455, 350);

  x = dabSettings->value("configWidget-x", 200).toInt();
  y = dabSettings->value("configWidget-y", 200).toInt();
  wi = dabSettings->value("configWidget-w", 200).toInt();
  he = dabSettings->value("configWidget-h", 150).toInt();
  configDisplay.resize(QSize(wi, he));
  configDisplay.move(QPoint(x, y));
  configDisplay.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon

  configWidget.sliderTest->hide(); // only used for test

  theTechWindow = new TechData(this, dabSettings, &theTechData);
  //
  //	Now we can set the checkbox as saved in the settings
  if (dabSettings->value("onTop", 0).toInt() == 1)
  {
    configWidget.onTop->setChecked(true);
  }

  if (dabSettings->value("saveLocations", 0).toInt() == 1)
  {
    configWidget.transmSelector->setChecked(true);
  }

  if (dabSettings->value("saveSlides", 0).toInt() == 1)
  {
    configWidget.saveSlides->setChecked(true);
  }

  _show_epg_label(false);

  x = dabSettings->value("switchDelay", SWITCH_DELAY).toInt();
  configWidget.switchDelaySetting->setValue(x);

  bool b = dabSettings->value("utcSelector", 0).toInt() == 1;
  configWidget.utcSelector->setChecked(b);
  channel.currentService.valid = false;
  channel.nextService.valid = false;
  channel.serviceCount = -1;
  if (dabSettings->value("has-presetName", 0).toInt() != 0)
  {
    configWidget.saveServiceSelector->setChecked(true);
    QString presetName = dabSettings->value("presetname", "").toString();
    if (presetName != "")
    {
      QStringList ss = presetName.split(":");
      if (ss.size() == 2)
      {
        channel.nextService.channel = ss.at(0);
        channel.nextService.serviceName = ss.at(1);
      }
      else
      {
        channel.nextService.channel = "";
        channel.nextService.serviceName = presetName;
      }
      channel.nextService.SId = 0;
      channel.nextService.SCIds = 0;
      channel.nextService.valid = true;
    }
  }
  channel.targetPos = cmplx(0, 0);
  float local_lat = dabSettings->value("latitude", 0).toFloat();
  float local_lon = dabSettings->value("longitude", 0).toFloat();
  channel.localPos = cmplx(local_lat, local_lon);

  configWidget.cmbSoftBitGen->addItems(get_soft_bit_gen_names()); // fill soft-bit-type combobox with text elements

  connect(configWidget.loadTableButton, &QPushButton::clicked, this, &RadioInterface::_slot_load_table);
  connect(configWidget.onTop, &QCheckBox::stateChanged, this, &RadioInterface::_slot_handle_on_top);
  connect(configWidget.transmSelector, &QCheckBox::stateChanged, this, &RadioInterface::_slot_handle_transm_selector);
  connect(configWidget.saveSlides, &QCheckBox::stateChanged, this, &RadioInterface::_slot_handle_save_slides);
  connect(configWidget.sliderTest, &QSlider::valueChanged, this, &RadioInterface::slot_test_slider);

  logFile = nullptr;

  x = dabSettings->value("closeDirect", 0).toInt();
  if (x != 0)
  {
    configWidget.closeDirect->setChecked(true);
  }

  x = dabSettings->value("epg2xml", 0).toInt();
  if (x != 0)
  {
    configWidget.epg2xmlSelector->setChecked(true);
  }

  if (dabSettings->value("autoBrowser", 1).toInt() == 1)
  {
    configWidget.autoBrowser->setChecked(true);
  }

  if (dabSettings->value("transmitterTags", 1).toInt() == 1)
  {
    configWidget.transmitterTags->setChecked(true);
  }
  connect(configWidget.autoBrowser, &QCheckBox::stateChanged, this, &RadioInterface::_slot_handle_auto_browser);
  connect(configWidget.transmitterTags, &QCheckBox::stateChanged, this, &RadioInterface::_slot_handle_transmitter_tags);

  transmitterTags_local = configWidget.transmitterTags->isChecked();
  theTechWindow->hide();  // until shown otherwise
  serviceList.clear();
  //model.clear();
  //ensembleDisplay->setModel(&model);
  /*
 */
#ifdef  DATA_STREAMER
  dataStreamer		= new tcpServer (dataPort);
#else
  (void)dataPort;
#endif
#ifdef  CLOCK_STREAMER
  clockStreamer		= new tcpServer (clockPort);
#else
  (void)clockPort;
#endif

  //	Where do we leave the audio out?
  configWidget.streamoutSelector->hide();
#ifdef TCP_STREAMER
  soundOut = new tcpStreamer(20040);
  theTechWindow->hide();
  (void)latency;
#elif QT_AUDIO
  soundOut = new QtAudio();
  theTechWindow->hide();
  (void)latency;
#else
  //	just sound out
  soundOut = new AudioSink(latency);
  ((AudioSink *)soundOut)->setupChannels(configWidget.streamoutSelector);
  configWidget.streamoutSelector->show();
  bool err = false;
  h = dabSettings->value("soundchannel", "default").toString();

  k = configWidget.streamoutSelector->findText(h);
  if (k != -1)
  {
    configWidget.streamoutSelector->setCurrentIndex(k);
    err = !((AudioSink *)soundOut)->selectDevice(k);
  }

  if ((k == -1) || err)
  {
    ((AudioSink *)soundOut)->selectDefaultDevice();
  }
#endif

#ifndef  __MINGW32__
  picturesPath = checkDir(QDir::tempPath());
#else
  picturesPath	= checkDir (QDir::homePath ());
#endif
  picturesPath += PRJ_NAME "/PIC/";
  picturesPath = dabSettings->value("picturesPath", picturesPath).toString();
  picturesPath = checkDir(picturesPath);
  filePath = dabSettings->value("filePath", picturesPath).toString();
  if (filePath != "")
  {
    filePath = checkDir(filePath);
  }

#ifndef  __MINGW32__
  epgPath = checkDir(QDir::tempPath());
#else
  epgPath		= checkDir (QDir::homePath ());
#endif
  epgPath += PRJ_NAME "/EPG/";
  epgPath = dabSettings->value("epgPath", epgPath).toString();
  epgPath = checkDir(epgPath);
  connect(&epgProcessor, &epgDecoder::signal_set_epg_data, this, &RadioInterface::slot_set_epg_data);
  //	timer for autostart epg service
  epgTimer.setSingleShot(true);
  connect(&epgTimer, &QTimer::timeout, this,  &RadioInterface::slot_epg_timer_timeout);

  QString historyFile = QDir::toNativeSeparators(QDir::homePath() + "/.config/" APP_NAME "/stationlist.xml");
  historyFile = dabSettings->value("history", historyFile).toString();
  historyFile = QDir::toNativeSeparators(historyFile);
  my_timeTable = new timeTableHandler(this);
  my_timeTable->hide();

  connect(this, &RadioInterface::signal_set_new_channel, channelSelector, &QComboBox::setCurrentIndex);
  connect(configWidget.dlTextButton, &QPushButton::clicked, this,  &RadioInterface::_slot_handle_dl_text_button);
  connect(configWidget.loggerButton, &QCheckBox::stateChanged, this, &RadioInterface::_slot_handle_logger_button);
  connect(btnHttpServer, &QPushButton::clicked, this,  &RadioInterface::_slot_handle_http_button);

  //	restore some settings from previous incarnations
  QString t = dabSettings->value("dabBand", "VHF Band III").toString();
  const uint8_t dabBand = (t == "VHF Band III" ? BAND_III : L_BAND);

  theBand.setupChannels(channelSelector, dabBand);
  QString skipfileName = dabSettings->value("skipFile", "").toString();
  theBand.setup_skipList(skipfileName);

  QPalette p = ficError_display->palette();
  p.setColor(QPalette::Highlight, Qt::red);
  ficError_display->setPalette(p);
  p.setColor(QPalette::Highlight, Qt::green);

  audioDumper = nullptr;
  rawDumper = nullptr;
  ficBlocks = 0;
  ficSuccess = 0;
  total_ficError = 0;
  total_fics = 0;

  connect(configWidget.streamoutSelector, qOverload<int>(&QComboBox::activated), this, &RadioInterface::slot_set_stream_selector);
  connect(theTechWindow, &TechData::signal_handle_timeTable, this, &RadioInterface::_slot_handle_time_table);

  copyrightLabel->setText(QString("<html><body><img src='qrc:/res/icons/copyright24.png'/> V" + mVersionStr + "</body></html>"));
  copyrightLabel->setToolTip(get_copyright_text());
  copyrightLabel->setOpenExternalLinks(true);

  connect(configWidget.portSelector, &QPushButton::clicked, this, &RadioInterface::_slot_handle_port_selector);
  connect(configWidget.set_coordinatesButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_set_coordinates_button);
  QString tiiFileName = dabSettings->value("tiiFile", "").toString();
  channel.tiiFile = false;
  if (tiiFileName != "")
  {
    channel.tiiFile = tiiHandler.fill_cache_from_tii_file(tiiFileName);
    if (!channel.tiiFile)
    {
      btnHttpServer->setToolTip("File '" + tiiFileName + "' could not be loaded. So this feature is switched off.");
      btnHttpServer->setEnabled(false);
    }
  }

  connect(configWidget.eti_activeSelector, &QCheckBox::stateChanged, this, &RadioInterface::_slot_handle_eti_active_selector);
  connect(this, &RadioInterface::signal_dab_processor_started, &my_spectrumViewer, &SpectrumViewer::slot_update_settings);
  connect(&my_spectrumViewer, &SpectrumViewer::signal_window_closed, this, &RadioInterface::_slot_handle_spectrum_button);
  connect(theTechWindow, &TechData::signal_window_closed, this, &RadioInterface::_slot_handle_tech_detail_button);

  channel.etiActive = false;
  show_pause_slide();

  //	and start the timer(s)
  //	The displaytimer is there to show the number of
  //	seconds running and handle - if available - the tii data
  displayTimer.setInterval(1000);
  connect(&displayTimer, &QTimer::timeout, this, &RadioInterface::_slot_update_time_display);
  displayTimer.start(1000);
  numberofSeconds = 0;

  //	timer for scanning
  channelTimer.setSingleShot(true);
  channelTimer.setInterval(10000);
  connect(&channelTimer, &QTimer::timeout, this, &RadioInterface::_slot_channel_timeout);
  //
  //	presetTimer
  presetTimer.setSingleShot(true);
  connect(&presetTimer, &QTimer::timeout, this, &RadioInterface::_slot_set_preset_service);

  QPalette lcdPalette;
#ifndef __MAC__
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
  lcdPalette.setColor(QPalette::Window, Qt::white);
#else
  lcdPalette. setColor (QPalette::Background, Qt::white);
#endif
  lcdPalette.setColor(QPalette::Base, Qt::black);
#endif
  _set_clock_text();
  mClockResetTimer.setSingleShot(true);
  connect(&mClockResetTimer, &QTimer::timeout, [this](){ _set_clock_text(); });

  configWidget.cpuMonitor->setPalette(lcdPalette);
  configWidget.cpuMonitor->setAutoFillBackground(true);
  configWidget.deviceSelector->addItems(get_device_name_list());

#ifdef  HAVE_PLUTO_RXTX
	streamerOut	= nullptr;
#endif
  mpInputDevice = nullptr;

  h = dabSettings->value("device", "no device").toString();
  k = configWidget.deviceSelector->findText(h);
  if (k != -1)
  {
    configWidget.deviceSelector->setCurrentIndex(k);
    mpInputDevice = create_device(configWidget.deviceSelector->currentText());
    if (mpInputDevice != nullptr)
    {
      dabSettings->setValue("device", configWidget.deviceSelector->currentText());
    }
  }

  connect(configButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_config_button);

  if (mpInputDevice != nullptr)
  {
    if (dabSettings->value("deviceVisible", 1).toInt() != 0)
    {
      mpInputDevice->show();
    }
    else
    {
      mpInputDevice->hide();
    }
  }

  if (dabSettings->value("spectrumVisible", 0).toInt() == 1)
  {
    my_spectrumViewer.show();
  }

  if (dabSettings->value("techDataVisible", 0).toInt() == 1)
  {
    theTechWindow->show();
  }

  mpServiceListHandler = std::make_unique<ServiceListHandler>(dabSettings, dbFileName, tblServiceList);

  //	if a device was selected, we just start, otherwise
  //	we wait until one is selected

  if (mpInputDevice != nullptr)
  {
    if (doStart())
    {
      qApp->installEventFilter(this);
      return;
    }
    else
    {
      delete mpInputDevice;
      mpInputDevice = nullptr;
    }
  }

  configDisplay.show();
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
    connect(configWidget.deviceSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_do_start);
#else
    connect(configWidget.deviceSelector, qOverload<const QString &>(&QComboBox::activated), this, &RadioInterface::_slot_do_start);
#endif
  qApp->installEventFilter(this);
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

RadioInterface::~RadioInterface()
{
  fprintf(stdout, "RadioInterface is deleted\n");
}

QString RadioInterface::get_copyright_text() const
{
  QString versionText = "<html><head/><body><p>";
  versionText = "<h3>" + QString(PRJ_NAME) + " V" + mVersionStr + " (Qt " QT_VERSION_STR ")</h3>";
  versionText += "<p><b>Built on " + QString(__TIMESTAMP__) + QString("<br/>Commit ") + QString(GITHASH) + ".</b></p>";
  versionText += "<p>Forked and partly extensive changed and extended by Thomas Neder<br/>"
                 "(<a href=\"https://github.com/tomneda/DABstar\">https://github.com/tomneda/DABstar</a>) from Qt-DAB<br/>"
                 "(<a href=\"https://github.com/JvanKatwijk/qt-dab\">https://github.com/JvanKatwijk/qt-dab</a>) by Jan van Katwijk<br/>"
                 "(<a href=\"mailto:J.vanKatwijk@gmail.com\">J.vanKatwijk@gmail.com</a>).</p>";
  versionText += "<p>Rights of Qt, FFTW, portaudio, libfaad, libsamplerate and libsndfile gratefully acknowledged.<br/>"
                 "Rights of developers of RTLSDR library, SDRplay libraries, AIRspy library and others gratefully acknowledged.<br/>"
                 "Rights of other contributors gratefully acknowledged.</p>";
  versionText += "</p></body></html>";
  return versionText;
}

void RadioInterface::_show_epg_label(const bool iShowLabel)
{
  _set_status_info_status(mStatusInfo.EPG, iShowLabel);
}

// _slot_do_start(QString) is called when - on startup - NO device was registered to be used, and the user presses the selectDevice comboBox
void RadioInterface::_slot_do_start(const QString & dev)
{
  mpInputDevice = create_device(dev);

  if (mpInputDevice == nullptr)
  {
    disconnectGUI();
    return;
  }

  doStart();
}

//
//	when doStart is called, a device is available and selected
bool RadioInterface::doStart()
{
  if (channel.nextService.channel != "")
  {
    int k = channelSelector->findText(channel.nextService.channel);
    if (k != -1)
    {
      channelSelector->setCurrentIndex(k);
    }
  }
  else
  {
    channelSelector->setCurrentIndex(0);
  }
  my_dabProcessor = new DabProcessor(this, mpInputDevice, &globals);
  channel.clean_channel();

  //	Some hidden buttons can be made visible now
  connectGUI();

  if (dabSettings->value("showDeviceWidget", 0).toInt() != 0)
  {
    mpInputDevice->show();
  }

  //	we avoided till now connecting the channel selector
  //	to the slot since that function does a lot more, things we
  //	do not want here
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
  connect(channelSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_handle_channel_selector);
#else
  connect(channelSelector, qOverload<const QString &>(&QComboBox::activated), this, &RadioInterface::_slot_handle_channel_selector);
#endif
  connect(&my_spectrumViewer, &SpectrumViewer::signal_cb_nom_carrier_changed, my_dabProcessor, &DabProcessor::slot_show_nominal_carrier);
  connect(&my_spectrumViewer, &SpectrumViewer::signal_cmb_carrier_changed, my_dabProcessor, &DabProcessor::slot_select_carrier_plot_type);
  connect(&my_spectrumViewer, &SpectrumViewer::signal_cmb_iqscope_changed, my_dabProcessor, &DabProcessor::slot_select_iq_plot_type);
  connect(configWidget.cmbSoftBitGen, qOverload<int>(&QComboBox::currentIndexChanged), my_dabProcessor, [this](int idx){ my_dabProcessor->slot_soft_bit_gen_type((ESoftBitType)idx);});

  //
  //	Just to be sure we disconnect here.
  //	It would have been helpful to have a function
  //	testing whether or not a connection exists, we need a kind
  //	of "reset"
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
  disconnect(configWidget.deviceSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_do_start);
  disconnect(configWidget.deviceSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_new_device);
  connect(configWidget.deviceSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_new_device);
#else
  disconnect(configWidget.deviceSelector, qOverload<const QString &>(&QComboBox::activated), this, &RadioInterface::_slot_do_start);
  disconnect(configWidget.deviceSelector, qOverload<const QString &>(&QComboBox::activated), this, &RadioInterface::_slot_new_device);
  connect(configWidget.deviceSelector, qOverload<const QString &>(&QComboBox::activated), this, &RadioInterface::_slot_new_device);
#endif
  //
  if (channel.nextService.valid)
  {
    const int32_t switchDelay = dabSettings->value("switchDelay", SWITCH_DELAY).toInt();
    presetTimer.setSingleShot(true);
    presetTimer.setInterval(switchDelay * 1000);
    presetTimer.start(switchDelay * 1000);
  }

  {
    const bool b = (dabSettings->value("tii_detector", 0).toInt() == 1);
    configWidget.tii_detectorMode->setChecked(b);
    my_dabProcessor->set_tiiDetectorMode(b);
    connect(configWidget.tii_detectorMode, &QCheckBox::stateChanged, this, &RadioInterface::_slot_handle_tii_detector_mode);
  }

  {
    const bool b = (dabSettings->value("dcAvoidance", 0).toInt() == 1);
    configWidget.cbDcAvoidance->setChecked(b);
    my_dabProcessor->set_dc_avoidance_algorithm(b);
    connect(configWidget.cbDcAvoidance, &QCheckBox::clicked, this, &RadioInterface::_slot_handle_dc_avoidance_algorithm);
  }

  {
    const bool b = (dabSettings->value("dcRemoval", 0).toInt() == 1);
    configWidget.cbDcRemovalFilter->setChecked(b);
    my_dabProcessor->set_dc_removal(b);
    connect(configWidget.cbDcRemovalFilter, &QCheckBox::clicked, this, &RadioInterface::_slot_handle_dc_removal);
  }

  emit signal_dab_processor_started();

  //	after the preset timer signals, the service will be started
  startChannel(channelSelector->currentText());
  running.store(true);
  return true;
}

//
///////////////////////////////////////////////////////////////////////////////
//
//	a slot called by the DAB-processor
void RadioInterface::slot_set_and_show_freq_corr_rf_Hz(int iFreqCorrRF)
{
  if (mpInputDevice != nullptr && channel.nominalFreqHz > 0)
  {
    mpInputDevice->setVFOFrequency(channel.nominalFreqHz + iFreqCorrRF);
    //sFreqOffHz = channel.nominalFreqHz;
  }

  my_spectrumViewer.show_freq_corr_rf_Hz(iFreqCorrRF);
}

void RadioInterface::slot_show_freq_corr_bb_Hz(int iFreqCorrBB)
{
  my_spectrumViewer.show_freq_corr_bb_Hz(iFreqCorrBB);
}

//
//	might be called when scanning only
void RadioInterface::_slot_channel_timeout()
{
  slot_no_signal_found();
}

///////////////////////////////////////////////////////////////////////////
//
//	a slot, called by the fic/fib handlers
void RadioInterface::slot_add_to_ensemble(const QString & serviceName, int32_t SId)
{
  if (!running.load())
  {
    return;
  }

  if (my_dabProcessor->getSubChId(serviceName, SId) < 0)
  {
    return;
  }

  serviceId ed;
  ed.name = serviceName;
  ed.SId = SId;

  if (std::any_of(serviceList.cbegin(), serviceList.cend(), [& ed](const serviceId & sid) { return sid.name == ed.name; } ))
  {
    return; // service already in service list
  }

  ed.subChId = my_dabProcessor->getSubChId(serviceName, SId);

  serviceList = insert_sorted(serviceList, ed);
  //serviceList.push_back(ed);
  
  mpServiceListHandler->add_entry(channel.channelName, ed.name);

//  model.clear();
//  for (const auto & serv: serviceList)
//  {
//    model.appendRow(new QStandardItem(serv.name));
//  }
#ifdef  __MINGW32__
                                                                                                                          for (int i = model. rowCount (); i < 12; i ++)
	   model. appendRow (new QStandardItem ("      "));
#endif
//  for (int i = 0; i < model.rowCount(); i++)
//  {
//    model.setData(model.index(i, 0), QFont(theFont, fontSize), Qt::FontRole);
//  }

  //ensembleDisplay->setModel(&model);
//  if (channel.serviceCount == model.rowCount() && !scanning)  // channel.serviceCount is only set in minor channels (here around Munich only 1 from 9)
//  {
//    presetTimer.stop();
//    _slot_set_preset_service();
//  }
}

//
//	The ensembleId is written as hexadecimal, however, the
//	number display of Qt is only 7 segments ...
static QString hextoString(int v)
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
  if (!running.load())
  {
    return;
  }

  QFont font = ensembleId->font();
  font.setPointSize(14);
  ensembleId->setFont(font);
  ensembleId->setText(v + QString("(") + hextoString(id) + QString(")"));

  channel.ensembleName = v;
  channel.Eid = id;
}

void RadioInterface::_slot_handle_content_button()
{
  QStringList s = my_dabProcessor->basicPrint();

  if (my_contentTable != nullptr)
  {
    my_contentTable->hide();
    delete my_contentTable;
    my_contentTable = nullptr;
    return;
  }
  QString theTime;
  QString SNR = "SNR " + QString::number(channel.snr);
  if (configWidget.utcSelector->isChecked())
  {
    theTime = convertTime(mUTC.year, mUTC.month, mUTC.day, mUTC.hour, mUTC.minute);
  }
  else
  {
    theTime = convertTime(mLocalTime.year, mLocalTime.month, mLocalTime.day, mLocalTime.hour, mLocalTime.minute);
  }

  QString header = channel.ensembleName + ";" + channel.channelName + ";" + QString::number(channel.nominalFreqHz / 1000) + ";"
                 + hextoString(channel.Eid) + " " + ";" + transmitter_coordinates->text() + " " + ";" + theTime + ";" + SNR + ";"
                 + QString::number(serviceList.size()) + ";" + lblStationLocation->text() + "\n";

  my_contentTable = new ContentTable(this, dabSettings, channel.channelName, my_dabProcessor->scanWidth());
  connect(my_contentTable, &ContentTable::signal_go_service, this, &RadioInterface::slot_handle_content_selector);

  my_contentTable->addLine(header);
  //	my_contentTable		-> addLine ("\n");
  for (int i = 0; i < s.size(); i++)
  {
    my_contentTable->addLine(s.at(i));
  }
  my_contentTable->show();
}

QString RadioInterface::checkDir(const QString & s)
{
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
    if (epgPath == "")
    {
      return;
    }

    if (objectName == QString(""))
    {
      objectName = "epg file";
    }
    objectName = epgPath + objectName;

    {
      QString temp = objectName;
      temp = temp.left(temp.lastIndexOf(QChar('/')));
      if (!QDir(temp).exists())
      {
        QDir().mkpath(temp);
      }

      std::vector<uint8_t> epgData(result.begin(), result.end());
      uint32_t ensembleId = my_dabProcessor->get_ensembleId();
      uint32_t currentSId = extract_epg(objectName, serviceList, ensembleId);
      uint32_t julianDate = my_dabProcessor->julianDate();
      int subType = getContentSubType((MOTContentType)contentType);
      epgProcessor.process_epg(epgData.data(), (int32_t)epgData.size(), currentSId, subType, julianDate);
      if (configWidget.epg2xmlSelector->isChecked())
      {
        epgHandler.decode(epgData, QDir::toNativeSeparators(objectName));
      }
    }
    return;

  case MOTBaseTypeProprietary: break;
  }
}

void RadioInterface::save_MOTtext(QByteArray & result, int contentType, QString name)
{
  (void)contentType;
  if (filePath == "")
  {
    return;
  }

  QString textName = QDir::toNativeSeparators(filePath + name);

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
  if (filePath == "")
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
  if (!running.load() || (pictureName == QString("")))
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

  if (configWidget.saveSlides->isChecked() && (picturesPath != ""))
  {
    QString pict = picturesPath + pictureName;
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

  if (!channel.currentService.is_audio)
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
    qDebug("MOT w: %d, h: %d (scale)", iPixMap.width(), iPixMap.height());
    pictureLabel->setPixmap(iPixMap.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  else
  {
    qDebug("MOT w: %d, h: %d", iPixMap.width(), iPixMap.height());
    pictureLabel->setPixmap(iPixMap);
  }

  pictureLabel->setAlignment(Qt::AlignCenter);
  pictureLabel->show();
}

//
//	sendDatagram is triggered by the ip handler,
void RadioInterface::slot_send_datagram(int length)
{
  uint8_t localBuffer[length];

  if (dataBuffer.get_ring_buffer_read_available() < length)
  {
    fprintf(stderr, "Something went wrong\n");
    return;
  }

  dataBuffer.get_data_from_ring_buffer(localBuffer, length);
}

//
//	tdcData is triggered by the backend.
void RadioInterface::slot_handle_tdc_data(int frametype, int length)
{
#ifdef DATA_STREAMER
  uint8_t localBuffer [length + 8];
#endif
  (void)frametype;
  if (!running.load())
  {
    return;
  }
  if (dataBuffer.get_ring_buffer_read_available() < length)
  {
    fprintf(stderr, "Something went wrong\n");
    return;
  }
#ifdef  DATA_STREAMER
                                                                                                                          dataBuffer. get_data_from_ring_buffer (&localBuffer [8], length);
	localBuffer [0] = 0xFF;
	localBuffer [1] = 0x00;
	localBuffer [2] = 0xFF;
	localBuffer [3] = 0x00;
	localBuffer [4] = (length & 0xFF) >> 8;
	localBuffer [5] = length & 0xFF;
	localBuffer [6] = 0x00;
	localBuffer [7] = frametype == 0 ? 0 : 0xFF;
	if (running. load())
	   dataStreamer -> sendData (localBuffer, length + 8);
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
  if (!running.load() || my_dabProcessor == nullptr)
  {
    return;
  }
  DabService s;
  if (channel.currentService.valid)
  {
    s = channel.currentService;
  }
  stopService(s);
  stopScanning(false);
  //
  //
  //	we stop all secondary services as well, but we maintain theer
  //	description, file descriptors remain of course
  //
  for (auto & backgroundService : channel.backgroundServices)
  {
    my_dabProcessor->stop_service(backgroundService.subChId, BACK_GROUND);
  }

  fprintf(stdout, "change will be effected\n");


  //	we rebuild the services list from the fib and
  //	then we (try to) restart the service
  serviceList = my_dabProcessor->getServices();
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
  if (channel.etiActive)
  {
    my_dabProcessor->reset_etiGenerator();
  }

  //	Of course, it may be disappeared
  if (s.valid)
  {
    QString ss = my_dabProcessor->findService(s.SId, s.SCIds);
    if (ss != "")
    {
      startService(s);
      return;
    }
    //
    //	The service is gone, it may be the subservice of another one
    s.SCIds = 0;
    s.serviceName = my_dabProcessor->findService(s.SId, s.SCIds);
    if (s.serviceName != "")
    {
      startService(s);
    }
  }
  //
  //	we also have to restart all background services,
  for (uint16_t i = 0; i < channel.backgroundServices.size(); i++)
  {
    QString ss = my_dabProcessor->findService(s.SId, s.SCIds);
    if (ss == "")
    {  // it is gone, close the file if any
      if (channel.backgroundServices.at(i).fd != nullptr)
      {
        fclose(channel.backgroundServices.at(i).fd);
      }
      channel.backgroundServices.erase(channel.backgroundServices.begin() + i);
    }
    else
    {  // (re)start the service
      if (my_dabProcessor->is_audioService(ss))
      {
        Audiodata ad;
        FILE * f = channel.backgroundServices.at(i).fd;
        my_dabProcessor->dataforAudioService(ss, &ad);
        my_dabProcessor->set_audioChannel(&ad, &audioBuffer, f, BACK_GROUND);
        channel.backgroundServices.at(i).subChId = ad.subchId;
      }
      else
      {
        Packetdata pd;
        my_dabProcessor->dataforPacketService(ss, &pd, 0);
        my_dabProcessor->set_dataChannel(&pd, &dataBuffer, BACK_GROUND);
        channel.backgroundServices.at(i).subChId = pd.subchId;
      }

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
void RadioInterface::slot_new_audio(int iAmount, int iSR, int iAudioFlags)
{
  if (!running.load())
  {
    return;
  }

  mAudioFrameCnt++;

  if (mAudioFrameCnt > 10)
  {
    mAudioFrameCnt = 0;

    const bool sbrUsed = ((iAudioFlags & RadioInterface::AFL_SBR_USED) != 0);
    const bool psUsed  = ((iAudioFlags & RadioInterface::AFL_PS_USED) != 0);
    _set_status_info_status(mStatusInfo.SBR, sbrUsed);
    _set_status_info_status(mStatusInfo.PS, psUsed);

    if (!theTechWindow->isHidden())
    {
      theTechWindow->show_sample_rate_and_audio_flags(iSR, sbrUsed, psUsed);
    }
  }
  int16_t vec[iAmount];
  while (audioBuffer.get_ring_buffer_read_available() > iAmount)
  {
    audioBuffer.get_data_from_ring_buffer(vec, iAmount);
    if (!mutingActive)
    {
      soundOut->audioOut(vec, iAmount, iSR);
    }
#ifdef HAVE_PLUTO_RXTX
    if (streamerOut != nullptr)
    {
      streamerOut->audioOut(vec, amount, rate);
    }
#endif
    if (!theTechWindow->isHidden())
    {
      theTechData.put_data_into_ring_buffer(vec, iAmount);
      theTechWindow->audioDataAvailable(iAmount, iSR);
    }
  }
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
  if (scanning.load())
  {
    stopScanning(false);
  }
  running.store(false);
  hideButtons();

  dabSettings->setValue("mainWidget-x", this->pos().x());
  dabSettings->setValue("mainWidget-y", this->pos().y());
  QSize size = this->size();
  dabSettings->setValue("mainwidget-w", size.width());
  dabSettings->setValue("mainwidget-h", size.height());
  dabSettings->setValue("configWidget-x", configDisplay.pos().x());
  dabSettings->setValue("configWidget-y", configDisplay.pos().y());
  size = configDisplay.size();
  dabSettings->setValue("configWidget-w", size.width());
  dabSettings->setValue("configWidget-h", size.height());

  //
#ifdef  DATA_STREAMER
  fprintf (stdout, "going to close the dataStreamer\n");
	delete		dataStreamer;
#endif
#ifdef  CLOCK_STREAMER
  fprintf (stdout, "going to close the clockstreamer\n");
	delete	clockStreamer;
#endif
  if (mapHandler != nullptr)
  {
    mapHandler->stop();
  }
  displayTimer.stop();
  channelTimer.stop();
  presetTimer.stop();
  epgTimer.stop();
  soundOut->stop();
  if (dlTextFile != nullptr)
  {
    fclose(dlTextFile);
  }
#ifdef HAVE_PLUTO_RXTX
  if (streamerOut != nullptr)
  {
    streamerOut->stop();
  }
#endif
  if (my_dabProcessor != nullptr)
  {
    my_dabProcessor->stop();
  }
  theTechWindow->hide();
  delete theTechWindow;
  if (my_contentTable != nullptr)
  {
    my_contentTable->clearTable();
    my_contentTable->hide();
    delete my_contentTable;
  }
  //	just save a few checkbox settings that are not
  //	bound by signa;/slots, but just read if needed
  dabSettings->setValue("utcSelector", configWidget.utcSelector->isChecked() ? 1 : 0);
  dabSettings->setValue("epg2xml", configWidget.epg2xmlSelector->isChecked() ? 1 : 0);

  theBand.saveSettings();
  stopFramedumping();
  stopSourcedumping();
  stopAudiodumping();
  theBand.hide();
  configDisplay.hide();
  LOG("terminating ", "");
  usleep(1000);    // pending signals
  if (logFile != nullptr)
  {
    fclose(logFile);
  }
  logFile = nullptr;
  //	everything should be halted by now

  dabSettings->sync();
  my_spectrumViewer.hide();
  //my_snrViewer.hide();
  if (my_dabProcessor != nullptr)
  {
    delete my_dabProcessor;
  }
  if (mpInputDevice != nullptr)
  {
    delete mpInputDevice;
  }
  delete soundOut;
  //delete my_history;
  delete my_timeTable;

  //	close();
  fprintf(stdout, ".. end the radio silences\n");
}

void RadioInterface::_slot_update_time_display()
{
  if (!running.load())
  {
    return;
  }

  numberofSeconds++;

  if ((numberofSeconds % 2) == 0)
  {
    size_t idle_time, total_time;
    if (get_cpu_times(idle_time, total_time))
    {
      const float idle_time_delta = static_cast<float>(idle_time - previous_idle_time);
      const float total_time_delta = static_cast<float>(total_time - previous_total_time);
      const float utilization = 100.0f * (1.0f - idle_time_delta / total_time_delta);
      configWidget.cpuMonitor->display(QString("%1").arg(utilization, 0, 'f', 2));
      previous_idle_time = idle_time;
      previous_total_time = total_time;
    }
  }
  
  //	The timer runs autonomously, so it might happen
  //	that it rings when there is no processor running
  if (my_dabProcessor == nullptr)
  {
    return;
  }
  if (soundOut->hasMissed())
  {
#if !defined(TCP_STREAMER) && !defined(QT_AUDIO)
    int xxx = ((AudioSink *)soundOut)->missed();
    if (!theTechWindow->isHidden())
    {
      theTechWindow->showMissed(xxx);
    }
#endif
  }
  if (error_report && (numberofSeconds % 10) == 0)
  {
    //	   int	totalFrames;
    //	   int	goodFrames;
    //	   int	badFrames;
    //	   my_dabProcessor	-> get_frameQuality (&totalFrames,
    //	                                             &goodFrames,
    //	                                             &badFrames);
    //	   fprintf (stdout, "total %d, good %d bad %d ficRatio %f\n",
    //	                     totalFrames, goodFrames, badFrames,
    //	                                            total_ficError * 100.0 / total_fics);
    total_ficError = 0;
    total_fics = 0;
#ifndef TCP_STREAMER
#ifndef  QT_AUDIO
    if (configWidget.streamoutSelector->isVisible())
    {
      int xxx = ((AudioSink *)soundOut)->missed();
      fprintf(stderr, "missed %d\n", xxx);
    }
#endif
#endif
  }
}

//
//	newDevice is called from the GUI when selecting a device
//	with the selector
void RadioInterface::_slot_new_device(const QString & deviceName)
{
  //	Part I : stopping all activities
  running.store(false);
  stopScanning(false);
  stopChannel();
  disconnectGUI();
  if (mpInputDevice != nullptr)
  {
    delete mpInputDevice;
    fprintf(stderr, "device is deleted\n");
    mpInputDevice = nullptr;
  }

  LOG("selecting ", deviceName);
  mpInputDevice = create_device(deviceName);
  if (mpInputDevice == nullptr)
  {
    mpInputDevice = new DummyHandler();
    return;    // nothing will happen
  }

  if (dabSettings->value("deviceVisible", 1).toInt() != 0)
  {
    mpInputDevice->show();
  }
  else
  {
    mpInputDevice->hide();
  }
  doStart();    // will set running
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

  dabSettings->setValue("deviceVisible", mpInputDevice->isHidden() ? 0 : 1);
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

#ifdef CLOCK_STREAMER
  uint8_t localBuffer[10];
  localBuffer[0] = 0xFF;
  localBuffer[1] = 0x00;
  localBuffer[2] = 0xFF;
  localBuffer[3] = 0x00;
  localBuffer[4] = (year & 0xFF00) >> 8;
  localBuffer[5] = year & 0xFF;
  localBuffer[6] = month;
  localBuffer[7] = day;
  localBuffer[8] = minutes;
  localBuffer[9] = seconds;
  if (running.load())
  {
    clockStreamer->sendData(localBuffer, 10);
  }
#endif
  mUTC.year = year;
  mUTC.month = month;
  mUTC.day = utc_day;
  mUTC.hour = utc_hour;
  mUTC.minute = utc_min;

  QString result;

  if (configWidget.utcSelector->isChecked())
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
  if (!running.load())
  {
    return;
  }
  if (!theTechWindow->isHidden())
  {
    theTechWindow->show_frameErrors(s);
  }
}

//
//	called from the MP4 decoder
void RadioInterface::slot_show_rs_errors(int s)
{
  if (!running.load())
  {    // should not happen
    return;
  }
  if (!theTechWindow->isHidden())
  {
    theTechWindow->show_rsErrors(s);
  }
}

//
//	called from the aac decoder
void RadioInterface::slot_show_aac_errors(int s)
{
  if (!running.load())
  {
    return;
  }

  if (!theTechWindow->isHidden())
  {
    theTechWindow->show_aacErrors(s);
  }
}

//
//	called from the ficHandler
void RadioInterface::slot_show_fic_success(bool b)
{
  if (!running.load())
  {
    return;
  }

  if (b)
  {
    ficSuccess++;
  }

  if (++ficBlocks >= 100)
  {
    QPalette p = ficError_display->palette();
    if (ficSuccess < 85)
    {
      p.setColor(QPalette::Highlight, Qt::red);
    }
    else
    {
      p.setColor(QPalette::Highlight, Qt::green);
    }

    ficError_display->setPalette(p);
    ficError_display->setValue(ficSuccess);
    total_ficError += 100 - ficSuccess;
    total_fics += 100;
    ficSuccess = 0;
    ficBlocks = 0;
  }
}

//
//	called from the PAD handler
void RadioInterface::slot_show_mot_handling(bool b)
{
  if (!running.load() || !b)
  {
    return;
  }
  theTechWindow->show_motHandling(b);
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
  if (running.load())
  {
    lblDynLabel->setText(s);
  }
  //	if we dtText is ON, some work is still to be done
  if ((dlTextFile == nullptr) || (the_dlCache.addifNew(s)))
  {
    return;
  }

  QString currentChannel = channel.channelName;
  QDateTime theDateTime = QDateTime::currentDateTime();
  fprintf(dlTextFile,
          "%s.%s %4d-%02d-%02d %02d:%02d:%02d  %s\n",
          currentChannel.toUtf8().data(),
          channel.currentService.serviceName.toUtf8().data(),
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

  ChannelDescriptor::STiiId id(mainId, subId);
  channel.transmitters.insert(id.FullId);

  if (!running.load())
  {
    return;
  }

  if ((mainId != channel.mainId) || (subId != channel.subId))
  {
    LOG("tii numbers", tiiNumber(mainId) + " " + tiiNumber(subId));
    tiiChange = true;
  }

  channel.mainId = mainId;
  channel.subId = subId;

  QString a; // = "TII:";
  uint16_t cnt = 0;
  for (const auto & tr : channel.transmitters)
  {
    ChannelDescriptor::STiiId id(tr);
    a = a + " " + tiiNumber(id.MainId) + "-" + tiiNumber(id.SubId);
    if (++cnt >= 4) break; // forbid doing a too long GUI output, due to noise reception
  }
  transmitter_coordinates->setAlignment(Qt::AlignRight);
  transmitter_coordinates->setText(a);

  //	if - for the first time now - we see an ecc value,
  //	we check whether or not a tii files is available
  if (!channel.has_ecc && (my_dabProcessor->get_ecc() != 0))
  {
    channel.ecc_byte = my_dabProcessor->get_ecc();
    country = find_ITU_code(channel.ecc_byte, (channel.Eid >> 12) & 0xF);
    channel.has_ecc = true;
    channel.transmitterName = "";
  }

  if ((country != "") && (country != channel.countryName))
  {
    transmitter_country->setText(country);
    channel.countryName = country;
    LOG("country", channel.countryName);
  }

  if (!channel.tiiFile)
  {
    return;
  }

  if (!(tiiChange || (channel.transmitterName == "")))
  {
    return;
  }

  if (tiiHandler.is_black(channel.Eid, mainId, subId))
  {
    return;
  }

  const QString theName = tiiHandler.get_transmitter_name((channel.realChannel ? channel.channelName : "any"), channel.Eid, mainId, subId);

  if (theName == "")
  {
    tiiHandler.set_black(channel.Eid, mainId, subId);
    LOG("Not found ", QString::number(channel.Eid, 16) + " " + QString::number(mainId) + " " + QString::number(subId));
    return;
  }

  channel.transmitterName = theName;
  float latitude, longitude, power;
  tiiHandler.get_coordinates(&latitude, &longitude, &power, channel.realChannel ? channel.channelName : "any", theName);
  channel.targetPos = cmplx(latitude, longitude);
  LOG("transmitter ", channel.transmitterName);
  LOG("coordinates ", QString::number(latitude) + " " + QString::number(longitude));
  LOG("current SNR ", QString::number(channel.snr));
  QString labelText = channel.transmitterName;
  //
  //      if our own position is known, we show the distance
  //
  float ownLatitude = real(channel.localPos);
  float ownLongitude = imag(channel.localPos);

  if ((ownLatitude == 0) || (ownLongitude == 0))
  {
    return;
  }

  const float distance = tiiHandler.distance(latitude, longitude, ownLatitude, ownLongitude);
  const float corner = tiiHandler.corner(latitude, longitude, ownLatitude, ownLongitude);
  const QString distanceStr = QString::number(distance, 'f', 1);
  const QString cornerStr = QString::number(corner, 'f', 1);
  LOG("distance ", distanceStr);
  LOG("corner ", cornerStr);
  labelText += +" " + distanceStr + " km" + " " + cornerStr + QString::fromLatin1("\xb0 ");
  fprintf(stdout, "%s\n", labelText.toUtf8().data());
  lblStationLocation->setText(labelText);

  //	see if we have a map
  if (mapHandler == nullptr)
  {
    return;
  }

  uint8_t key = MAP_NORM_TRANS;
  if ((!transmitterTags_local) && (distance > maxDistance))
  {
    maxDistance = (int)distance;
    key = MAP_MAX_TRANS;
  }

  //	to be certain, we check
  if (channel.targetPos == cmplx(0, 0) || (distance == 0) || (corner == 0))
  {
    return;
  }

  const QDateTime theTime = (configWidget.utcSelector->isChecked() ? QDateTime::currentDateTimeUtc() : QDateTime::currentDateTime());

  mapHandler->putData(key,
                      channel.targetPos,
                      channel.transmitterName,
                      channel.channelName,
                      theTime.toString(Qt::TextDate),
                      channel.mainId * 100 + channel.subId,
                      (int)distance,
                      (int)corner,
                      power);
}

void RadioInterface::slot_show_spectrum(int32_t amount)
{
  if (!running.load())
  {
    return;
  }

  my_spectrumViewer.show_spectrum(mpInputDevice->getVFOFrequency());
}

void RadioInterface::slot_show_iq(int iAmount, float iAvg)
{
  if (!running.load())
  {
    return;
  }

  my_spectrumViewer.show_iq(iAmount, iAvg);
}

void RadioInterface::slot_show_mod_quality_data(const OfdmDecoder::SQualityData * pQD)
{
  if (!running.load())
  {
    return;
  }

  if (!my_spectrumViewer.is_hidden())
  {
    my_spectrumViewer.show_quality(pQD->CurOfdmSymbolNo, pQD->ModQuality, pQD->TimeOffset, pQD->FreqOffset, pQD->PhaseCorr, pQD->SNR);
  }
}

void RadioInterface::slot_show_peak_level(float iPeakLevel)
{
  if (!running.load())
  {
    return;
  }

  my_spectrumViewer.show_digital_peak_level(iPeakLevel);
}

//	called from the MP4 decoder
void RadioInterface::slot_show_rs_corrections(int c, int ec)
{
  if (!running)
  {
    return;
  }
  if (!theTechWindow->isHidden())
  {
    theTechWindow->show_rsCorrections(c, ec);
  }
}

//
//	called from the DAB processor
void RadioInterface::slot_show_clock_error(float e)
{
  if (!running.load())
  {
    return;
  }
  if (!my_spectrumViewer.is_hidden())
  {
    my_spectrumViewer.show_clock_error(e);
  }
}

//
//	called from the phasesynchronizer
void RadioInterface::slot_show_correlation(int amount, int marker, const QVector<int> & v)
{
  if (!running.load())
  {
    return;
  }

  my_spectrumViewer.show_correlation(amount, marker, v);
  channel.nrTransmitters = v.size();
}

////////////////////////////////////////////////////////////////////////////

void RadioInterface::slot_set_stream_selector(int k)
{
  if (!running.load())
  {
    return;
  }
#if !defined(TCP_STREAMER) && !defined(QT_AUDIO)
  ((AudioSink *)(soundOut))->selectDevice(k);
  dabSettings->setValue("soundchannel", configWidget.streamoutSelector->currentText());
#else
  (void)k;
#endif
}

void RadioInterface::_slot_handle_tech_detail_button()
{
  if (!running.load())
  {
    return;
  }

  if (theTechWindow->isHidden())
  {
    theTechWindow->show();
  }
  else
  {
    theTechWindow->hide();
  }
  dabSettings->setValue("techDataVisible", theTechWindow->isHidden() ? 0 : 1);
}

//	Whenever the input device is a file, some functions,
//	e.g. selecting a channel, setting an alarm, are not
//	meaningful
void RadioInterface::showButtons()
{
#if 1
  configWidget.dumpButton->show();
  btnScanning->show();
  channelSelector->show();
#else
  configWidget.dumpButton->setEnabled(true);
  configWidget.frequencyDisplay->setEnabled(true);
  btnScanning->setEnabled(true);
  channelSelector->setEnabled(true);
#endif
}

void RadioInterface::hideButtons()
{
#if 1
  configWidget.dumpButton->hide();
  btnScanning->hide();
  channelSelector->hide();
#else
  configWidget.dumpButton->setEnabled(false);
  configWidget.frequencyDisplay->setEnabled(false);
  btnScanning->setEnabled(false);
  channelSelector->setEnabled(false);
#endif
}

void RadioInterface::slot_set_sync_lost()
{
}

void RadioInterface::_slot_handle_reset_button()
{
  if (!running.load())
  {
    return;
  }
  QString channelName = channel.channelName;
  stopScanning(false);
  stopChannel();
  startChannel(channelName);
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

void RadioInterface::stopSourcedumping()
{
  if (rawDumper == nullptr)
  {
    return;
  }

  LOG("source dump stops ", "");
  my_dabProcessor->stopDumping();
  sf_close(rawDumper);
  rawDumper = nullptr;
  setButtonFont(configWidget.dumpButton, "Raw dump", 10);
}

//
void RadioInterface::startSourcedumping()
{
  QString deviceName = mpInputDevice->deviceName();
  QString channelName = channel.channelName;

  if (scanning.load())
  {
    return;
  }

  rawDumper = filenameFinder.findRawDump_fileName(deviceName, channelName);
  if (rawDumper == nullptr)
  {
    return;
  }

  LOG("source dump starts ", channelName);
  setButtonFont(configWidget.dumpButton, "writing", 12);
  my_dabProcessor->startDumping(rawDumper);
}

void RadioInterface::_slot_handle_source_dump_button()
{
  if (!running.load() || scanning.load())
  {
    return;
  }

  if (rawDumper != nullptr)
  {
    stopSourcedumping();
  }
  else
  {
    startSourcedumping();
  }
}

void RadioInterface::_slot_handle_audio_dump_button()
{
  if (!running.load() || scanning.load())
  {
    return;
  }

  if (audioDumper != nullptr)
  {
    stopAudiodumping();
  }
  else
  {
    startAudiodumping();
  }
}

void RadioInterface::stopAudiodumping()
{
  if (audioDumper == nullptr)
  {
    return;
  }

  LOG("audiodump stops", "");
  soundOut->stopDumping();
  sf_close(audioDumper);
  audioDumper = nullptr;
  theTechWindow->audiodumpButton_text("Dump WAV", 10);
}

void RadioInterface::startAudiodumping()
{
  audioDumper = filenameFinder.findAudioDump_fileName(channel.currentService.serviceName, true);
  if (audioDumper == nullptr)
  {
    return;
  }

  LOG("audiodump starts ", serviceLabel->text());
  theTechWindow->audiodumpButton_text("Recording", 12);
  soundOut->startDumping(audioDumper);
}

void RadioInterface::_slot_handle_frame_dump_button()
{
  if (!running.load() || scanning.load())
  {
    return;
  }

  if (channel.currentService.frameDumper != nullptr)
  {
    stopFramedumping();
  }
  else
  {
    startFramedumping();
  }
}

void RadioInterface::stopFramedumping()
{
  if (channel.currentService.frameDumper == nullptr)
  {
    return;
  }

  fclose(channel.currentService.frameDumper);
  theTechWindow->framedumpButton_text("Dump AAC", 10);
  channel.currentService.frameDumper = nullptr;
}

void RadioInterface::startFramedumping()
{
  channel.currentService.frameDumper = filenameFinder.findFrameDump_fileName(channel.currentService.serviceName, true);
  if (channel.currentService.frameDumper == nullptr)
  {
    return;
  }
  theTechWindow->framedumpButton_text("Recording", 12);
}

//	called from the mp4 handler, using a signal
void RadioInterface::slot_new_frame(int amount)
{
  uint8_t buffer[amount];
  if (!running.load())
  {
    return;
  }

  if (channel.currentService.frameDumper == nullptr)
  {
    frameBuffer.flush_ring_buffer();
  }
  else
  {
    while (frameBuffer.get_ring_buffer_read_available() >= amount)
    {
      frameBuffer.get_data_from_ring_buffer(buffer, amount);
      if (channel.currentService.frameDumper != nullptr)
      {
        fwrite(buffer, amount, 1, channel.currentService.frameDumper);
      }
    }
  }
}

void RadioInterface::_slot_handle_spectrum_button()
{
  if (!running.load())
  {
    return;
  }

  if (my_spectrumViewer.is_hidden())
  {
    my_spectrumViewer.show();
  }
  else
  {
    my_spectrumViewer.hide();
  }
  dabSettings->setValue("spectrumVisible", my_spectrumViewer.is_hidden() ? 0 : 1);
}

//	When changing (or setting) a device, we do not want anybody
//	to have the buttons on the GUI touched, so
//	we just disconnect them and (re)connect them as soon as
//	a device is operational
void RadioInterface::connectGUI()
{
  connect(configWidget.contentButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_content_button);
  connect(btnTechDetails, &QPushButton::clicked, this, &RadioInterface::_slot_handle_tech_detail_button);
  connect(configWidget.resetButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_reset_button);
  connect(btnScanning, &QPushButton::clicked, this, &RadioInterface::_slot_handle_scan_button);
  connect(btnSpectrumScope, &QPushButton::clicked, this, &RadioInterface::_slot_handle_spectrum_button);
  connect(btnDeviceWidget, &QPushButton::clicked, this, &RadioInterface::_slot_handle_device_widget_button);
  connect(configWidget.dumpButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_source_dump_button);
  connect(btnPrevService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_prev_service_button);
  connect(btnNextService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_next_service_button);
  connect(btnTargetService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_target_service_button);
  connect(theTechWindow, &TechData::signal_handle_audioDumping, this, &RadioInterface::_slot_handle_audio_dump_button);
  connect(theTechWindow, &TechData::signal_handle_frameDumping, this, &RadioInterface::_slot_handle_frame_dump_button);
  connect(btnMuteAudio, &QPushButton::clicked, this, &RadioInterface::_slot_handle_mute_button);
  //connect(ensembleDisplay, &QListView::clicked, this, &RadioInterface::_slot_select_service);
  connect(configWidget.switchDelaySetting, qOverload<int>(&QSpinBox::valueChanged), this, &RadioInterface::_slot_handle_switch_delay_setting);
  connect(configWidget.saveServiceSelector, &QCheckBox::stateChanged, this, &RadioInterface::_slot_handle_save_service_selector);
  connect(configWidget.skipList_button, &QPushButton::clicked, this, &RadioInterface::_slot_handle_skip_list_button);
  connect(configWidget.skipFile_button, &QPushButton::clicked, this, &RadioInterface::_slot_handle_skip_file_button);
  connect(mpServiceListHandler.get(), &ServiceListHandler::signal_selection_changed, this, &RadioInterface::_slot_service_changed);
  connect(mpServiceListHandler.get(), &ServiceListHandler::signal_favorite_status, this, &RadioInterface::_slot_favorite_changed);
  connect(btnToggleFavorite, &QPushButton::clicked, this, &RadioInterface::_slot_handle_favorite_button);
}

void RadioInterface::disconnectGUI()
{
  disconnect(configWidget.contentButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_content_button);
  disconnect(btnTechDetails, &QPushButton::clicked, this, &RadioInterface::_slot_handle_tech_detail_button);
  disconnect(configWidget.resetButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_reset_button);
  disconnect(btnScanning, &QPushButton::clicked, this, &RadioInterface::_slot_handle_scan_button);
  disconnect(btnSpectrumScope, &QPushButton::clicked, this, &RadioInterface::_slot_handle_spectrum_button);
  disconnect(btnDeviceWidget, &QPushButton::clicked, this, &RadioInterface::_slot_handle_device_widget_button);
  disconnect(configWidget.dumpButton, &QPushButton::clicked, this, &RadioInterface::_slot_handle_source_dump_button);
  disconnect(btnPrevService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_prev_service_button);
  disconnect(btnNextService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_next_service_button);
  disconnect(btnTargetService, &QPushButton::clicked, this, &RadioInterface::_slot_handle_target_service_button);
  disconnect(theTechWindow, &TechData::signal_handle_audioDumping, this, &RadioInterface::_slot_handle_audio_dump_button);
  disconnect(theTechWindow, &TechData::signal_handle_frameDumping, this, &RadioInterface::_slot_handle_frame_dump_button);
  disconnect(btnMuteAudio, &QPushButton::clicked, this, &RadioInterface::_slot_handle_mute_button);
  //disconnect(ensembleDisplay, &QListView::clicked, this, &RadioInterface::_slot_select_service);
  disconnect(configWidget.switchDelaySetting, qOverload<int>(&QSpinBox::valueChanged), this, &RadioInterface::_slot_handle_switch_delay_setting);
  disconnect(configWidget.saveServiceSelector, &QCheckBox::stateChanged, this, &RadioInterface::_slot_handle_save_service_selector);
  disconnect(configWidget.skipList_button, &QPushButton::clicked, this, &RadioInterface::_slot_handle_skip_list_button);
  disconnect(configWidget.skipFile_button, &QPushButton::clicked, this, &RadioInterface::_slot_handle_skip_file_button);
  disconnect(mpServiceListHandler.get(), &ServiceListHandler::signal_selection_changed, this, &RadioInterface::_slot_service_changed);
  disconnect(mpServiceListHandler.get(), &ServiceListHandler::signal_favorite_status, this, &RadioInterface::_slot_favorite_changed);
  disconnect(btnToggleFavorite, &QPushButton::clicked, this, &RadioInterface::_slot_handle_favorite_button);
}

void RadioInterface::closeEvent(QCloseEvent * event)
{
  int x = configWidget.closeDirect->isChecked() ? 1 : 0;
  dabSettings->setValue("closeDirect", x);
  if (x != 0)
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

#if 0
bool RadioInterface::eventFilter(QObject * obj, QEvent * event)
{
  if (!running.load())
  {
    return QWidget::eventFilter(obj, event);
  }

  if (my_dabProcessor == nullptr)
  {
    fprintf(stderr, "expert error 5\n");
    return true;
  }

  if (event->type() == QEvent::KeyPress)
  {
    QKeyEvent * ke = static_cast<QKeyEvent *>(event);
    if (ke->key() == Qt::Key_Return)
    {
      presetTimer.stop();
      channel.nextService.valid = false;
      QString serviceName = ensembleDisplay->currentIndex().data(Qt::DisplayRole).toString();
      if (channel.currentService.serviceName != serviceName)
      {
        fprintf(stderr, "currentservice = %s (%d)\n", channel.currentService.serviceName.toUtf8().data(), channel.currentService.valid);
        stopService(channel.currentService);
        channel.currentService.valid = false;
        _slot_select_service(ensembleDisplay->currentIndex());
      }
    }
  }
  else if ((obj == this->ensembleDisplay->viewport()) && (event->type() == QEvent::MouseButtonPress))
  {
    QMouseEvent * ev = static_cast<QMouseEvent *>(event);
    if (ev->buttons() & Qt::RightButton)
    {
      Audiodata ad;
      Packetdata pd;
      QString serviceName = this->ensembleDisplay->indexAt(ev->pos()).data().toString();
      serviceName = serviceName.right(16);
      //	      if (serviceName. at (1) == ' ')
      //	         return true;
      my_dabProcessor->dataforAudioService(serviceName, &ad);
      if (ad.defined && (channel.currentService.serviceName == serviceName))
      {
        return true;
      }

      if ((ad.defined) && (ad.ASCTy == 077))
      {
        for (uint16_t i = 0; i < channel.backgroundServices.size(); i++)
        {
          if (channel.backgroundServices.at(i).serviceName == serviceName)
          {
            my_dabProcessor->stop_service(ad.subchId, BACK_GROUND);
            if (channel.backgroundServices.at(i).fd != nullptr)
            {
              fclose(channel.backgroundServices.at(i).fd);
            }
            channel.backgroundServices.erase(channel.backgroundServices.begin() + i);
            colorServiceName(serviceName, Qt::black, fontSize, false);
            return true;
          }
        }
        FILE * f = filenameFinder.findFrameDump_fileName(serviceName, true);
        if (f == nullptr)
        {
          return true;
        }

        (void)my_dabProcessor->set_audioChannel(&ad, &audioBuffer, f, BACK_GROUND);

        DabService s;
        s.channel = ad.channel;
        s.serviceName = ad.serviceName;
        s.SId = ad.SId;
        s.SCIds = ad.SCIds;
        s.subChId = ad.subchId;
        s.fd = f;
        channel.backgroundServices.push_back(s);
        colorServiceName(s.serviceName, Qt::blue, fontSize + 2, true);
        return true;
      }
    }
  }

  return QWidget::eventFilter(obj, event);
}
#endif

//void RadioInterface::colorServiceName(const QString & serviceName, QColor color, int fS, bool italic)
//{
//  for (int j = 0; j < model.rowCount(); j++)
//  {
//    QString itemText = model.index(j, 0).data(Qt::DisplayRole).toString();
//    if (itemText == serviceName)
//    {
//      colorService(model.index(j, 0), color, fS, italic);
//      return;
//    }
//  }
//}

void RadioInterface::slot_start_announcement(const QString & name, int subChId)
{
  if (!running.load())
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
  if (!running.load())
  {
    return;
  }

  if (name == serviceLabel->text())
  {
    _set_status_info_status(mStatusInfo.Announce, false);
    fprintf(stdout, "Announcement for %s (%d) stops\n", name.toUtf8().data(), subChId);
  }
}

////////////////////////////////////////////////////////////////////////
//
//	selection, either direct, from presets,  from history or schedule
////////////////////////////////////////////////////////////////////////

//
//	Regular selection, straight for the service list
void RadioInterface::_slot_select_service(QModelIndex ind)
{
  QString currentProgram = ind.data(Qt::DisplayRole).toString();
  localSelect(channel.channelName, currentProgram);
}

void RadioInterface::_slot_service_changed(const QString & iChannel, const QString & iService)
{
  localSelect(iChannel, iService);
}

void RadioInterface::_slot_favorite_changed(const bool iIsFav)
{
  mCurFavoriteState = iIsFav;
  set_favorite_button_style();
}

//	selecting from a content description
void RadioInterface::slot_handle_content_selector(const QString & s)
{
  localSelect(s);
}

//
//	From a predefined schedule list, the service names most
//	likely are less than 16 characters
//

void RadioInterface::localSelect(const QString & s)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
  QStringList list = s.split(":", Qt::SkipEmptyParts);
#else
  QStringList list = s.split(":", QString::SkipEmptyParts);
#endif
  if (list.length() != 2)
  {
    return;
  }
  localSelect(list.at(0), list.at(1));
}

void RadioInterface::localSelect(const QString & theChannel, const QString & service)
{
  if (my_dabProcessor == nullptr)
  {  // should not happen
    return;
  }

  stopService(channel.currentService);

  QString serviceName = service;

  for (int i = service.size(); i < 16; i++)
  {
    serviceName.push_back(' ');
  }

  // Is it only a service change within the same channel?
  if (theChannel == channel.channelName)
  {
    channel.currentService.valid = false;
    DabService s;
    my_dabProcessor->getParameters(serviceName, &s.SId, &s.SCIds);
    if (s.SId == 0)
    {
      write_warning_message("Insufficient data for this program (1)");
      return;
    }
    s.serviceName = service; // TODO: service or serviceName?
    startService(s);
    return;
  }

  // The hard part is stopping the current service, quitting the current channel, selecting a new channel, waiting a while
  stopChannel();
  //      and start the new channel first
  int k = channelSelector->findText(theChannel);
  if (k != -1)
  {
    new_channelIndex(k);
  }
  else
  {
    QMessageBox::warning(this, tr("Warning"), tr("Incorrect preset\n"));
    return;
  }

  // Prepare the service, start the new channel and wait
  channel.nextService.valid = true;
  channel.nextService.channel = theChannel;
  channel.nextService.serviceName = serviceName;
  channel.nextService.SId = 0;
  channel.nextService.SCIds = 0;
  presetTimer.setSingleShot(true);
  const int32_t switchDelay = dabSettings->value("switchDelay", SWITCH_DELAY).toInt();
  presetTimer.setInterval(switchDelay * 1000);
  presetTimer.start(switchDelay * 1000);
  startChannel(channelSelector->currentText());
}

////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////

void RadioInterface::stopService(DabService & s)
{
  presetTimer.stop();
  channelTimer.stop();

  if (my_dabProcessor == nullptr)
  {
    fprintf(stderr, "Expert error 22\n");
    return;
  }

  //	stop "dumpers"
  if (channel.currentService.frameDumper != nullptr)
  {
    stopFramedumping();
    channel.currentService.frameDumper = nullptr;
  }

  if (audioDumper != nullptr)
  {
    stopAudiodumping();
  }

  //	and clean up the technical widget
  theTechWindow->cleanUp();
  //
  //	stop "secondary services" - if any - as well
  if (s.valid)
  {
    my_dabProcessor->stop_service(s.subChId, FORE_GROUND);
    if (s.is_audio)
    {
      soundOut->stop();
      for (int i = 0; i < 5; i++)
      {
        Packetdata pd;
        my_dabProcessor->dataforPacketService(s.serviceName, &pd, i);
        if (pd.defined)
        {
          my_dabProcessor->stop_service(pd.subchId, FORE_GROUND);
          break;
        }
      }
    }
    s.valid = false;
    //
    //	The name of the service - on stopping it - will be - normally -
    //	made back again in the service list. An exception is when another
    //	instance of the service runs as background process, then the
    //	name should be made green, italic
//    for (int i = 0; i < model.rowCount(); i++)
//    {
//      QString itemText = model.index(i, 0).data(Qt::DisplayRole).toString();
//      if (itemText != s.serviceName)
//      {
//        continue;
//      }
//      for (uint16_t j = 0; j < channel.backgroundServices.size(); j++)
//      {
//        if (channel.backgroundServices.at(j).serviceName == s.serviceName)
//        {
//          colorService(model.index(i, 0), Qt::blue, fontSize + 2, true);
//          cleanScreen();
//          return;
//        }
//      }  // ok, service is not background as well
//      colorService(model.index(i, 0), Qt::black, fontSize);
//      break;
//    }
  }
  show_pause_slide();
  cleanScreen();
}

void RadioInterface::startService(DabService & s)
{
  QString serviceName = s.serviceName;

  channel.currentService = s;
  channel.currentService.frameDumper = nullptr;
  channel.currentService.valid = false;
  LOG("start service ", serviceName.toUtf8().data());
  LOG("service has SNR ", QString::number(channel.snr));
  //
  //	mark the selected service in the service list
  //int rowCount = model.rowCount();
  //(void)rowCount;

  //colorServiceName(serviceName, Qt::red, 16, true);
  mpServiceListHandler->set_selector(channel.channelName, serviceName);

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
  my_dabProcessor->dataforAudioService(serviceName, &ad);

  if (ad.defined)
  {
    channel.currentService.valid = true;
    channel.currentService.is_audio = true;
    channel.currentService.subChId = ad.subchId;
    if (my_dabProcessor->has_timeTable(ad.SId))
    {
      theTechWindow->show_timetableButton(true);
    }

    startAudioservice(&ad);
    if (dabSettings->value("has-presetName", 0).toInt() == 1)
    {
      QString s = channel.channelName + ":" + serviceName;
      dabSettings->setValue("presetname", s);
    }
    else
    {
      dabSettings->setValue("presetname", "");
    }
#ifdef  HAVE_PLUTO_RXTX
                                                                                                                            if (streamerOut != nullptr)
	      streamerOut -> addRds (std::string (serviceName. toUtf8 (). data ()));
#endif
  }
  else if (my_dabProcessor->is_packetService(serviceName))
  {
    Packetdata pd;
    my_dabProcessor->dataforPacketService(serviceName, &pd, 0);
    channel.currentService.valid = true;
    channel.currentService.is_audio = false;
    channel.currentService.subChId = pd.subchId;
    startPacketservice(serviceName);
  }
  else
  {
    write_warning_message("Insufficient data for this program (2)");
    dabSettings->setValue("presetname", "");
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
  channel.currentService.valid = true;

  (void)my_dabProcessor->set_audioChannel(ad, &audioBuffer, nullptr, FORE_GROUND);
  for (int i = 1; i < 10; i++)
  {
    Packetdata pd;
    my_dabProcessor->dataforPacketService(ad->serviceName, &pd, i);
    if (pd.defined)
    {
      my_dabProcessor->set_dataChannel(&pd, &dataBuffer, FORE_GROUND);
      fprintf(stdout, "adding %s (%d) as subservice\n", pd.serviceName.toUtf8().data(), pd.subchId);
      break;
    }
  }
  //	activate sound
  soundOut->restart();
  programTypeLabel->setText(getProgramType(ad->programType));
  //	show service related data
  theTechWindow->show_serviceData(ad);
  _set_status_info_status(mStatusInfo.BitRate, (int32_t)(ad->bitRate));
}

void RadioInterface::startPacketservice(const QString & s)
{
  Packetdata pd;

  my_dabProcessor->dataforPacketService(s, &pd, 0);
  if ((!pd.defined) || (pd.DSCTy == 0) || (pd.bitRate == 0))
  {
    write_warning_message("Insufficient data for this program (3)");
    return;
  }

  if (!my_dabProcessor->set_dataChannel(&pd, &dataBuffer, FORE_GROUND))
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
    write_warning_message("MOT partially implemented");
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
void RadioInterface::cleanScreen()
{
  serviceLabel->setText("");
  lblDynLabel->setStyleSheet("color: white");
  lblDynLabel->setText("");
  theTechWindow->cleanUp();
  programTypeLabel->setText("");
  theTechWindow->cleanUp();
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

//	Previous and next services. trivial implementation
void RadioInterface::handle_serviceButton(direction d)
{
  if (!running.load())
  {
    return;
  }

  presetTimer.stop();
  stopScanning(false);
  channel.nextService.valid = false;
  if (!channel.currentService.valid)
  {
    return;
  }

  QString oldService = channel.currentService.serviceName;

  stopService(channel.currentService);
  channel.currentService.valid = false;

  if ((serviceList.size() != 0) && (oldService != ""))
  {
    for (int i = 0; i < (int)(serviceList.size()); i++)
    {
      if (serviceList.at(i).name == oldService)
      {
        //colorService(model.index(i, 0), Qt::black, fontSize);
        if (d == FORWARD)
        {
          i = (i + 1) % serviceList.size();
        }
        else
        {
          i = (i - 1 + serviceList.size()) % serviceList.size();
        }
        DabService s;
        my_dabProcessor->getParameters(serviceList.at(i).name, &s.SId, &s.SCIds);
        if (s.SId == 0)
        {
          write_warning_message("insufficient data for this program (4)");
          break;
        }
        s.serviceName = serviceList.at(i).name;
        startService(s);
        break;
      }
    }
  }
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
void RadioInterface::_slot_set_preset_service()
{
  if (!running.load())
  {
    return;
  }

  presetTimer.stop();
  stopScanning(false);
  if (!channel.nextService.valid)
  {
    return;
  }

  if (channel.nextService.channel != channel.channelName)
  {
    return;
  }

  if (channel.Eid == 0)
  {
    write_warning_message("Ensemble not yet recognized");
    return;
  }

  QString presetName = channel.nextService.serviceName;
  for (int i = presetName.length(); i < 16; i++)
  {
    presetName.push_back(' ');
  }

  DabService s;
  s.serviceName = presetName;
  my_dabProcessor->getParameters(presetName, &s.SId, &s.SCIds);
  if (s.SId == 0)
  {
    write_warning_message("Insufficient data for this program (5)");
    return;
  }

  channel.nextService.valid = false;
  startService(s);
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
void RadioInterface::startChannel(const QString & theChannel)
{
  const int32_t tunedFrequencyHz = theBand.get_frequency_Hz(theChannel);
  LOG("channel starts ", theChannel);
  my_spectrumViewer.show_nominal_frequency_MHz((float)tunedFrequencyHz / 1'000'000.0f);
  dabSettings->setValue("channel", theChannel);
  mpInputDevice->resetBuffer();
  serviceList.clear();
  //model.clear();
  //ensembleDisplay->setModel(&model);
  mpInputDevice->restartReader(tunedFrequencyHz);
  channel.clean_channel();
  channel.channelName = theChannel;
  //dabSettings->setValue("channel", theChannel);
  channel.nominalFreqHz = tunedFrequencyHz;

  if (transmitterTags_local && (mapHandler != nullptr))
  {
    mapHandler->putData(MAP_RESET, cmplx(0, 0), "", "", "", 0, 0, 0, 0);
  }
  else if (mapHandler != nullptr)
  {
    mapHandler->putData(MAP_FRAME, cmplx(-1, -1), "", "", "", 0, 0, 0, 0);
  }

  enable_ui_elements_for_safety(true);

  my_dabProcessor->start();

  if (!scanning.load())
  {
    const int32_t switchDelay = dabSettings->value("switchDelay", SWITCH_DELAY).toInt();
    epgTimer.start(switchDelay * 1000);
  }
}

//
//	apart from stopping the reader, a lot of administration
//	is to be done.
void RadioInterface::stopChannel()
{
  if (mpInputDevice == nullptr)
  {    // should not happen
    return;
  }
  stop_etiHandler();
  LOG("channel stops ", channel.channelName);
  //
  //	first, stop services in fore and background
  if (channel.currentService.valid)
  {
    stopService(channel.currentService);
  }

  for (uint16_t i = 0; i < channel.backgroundServices.size(); i++)
  {
    DabService s = channel.backgroundServices.at(i);
    my_dabProcessor->stop_service(s.subChId, BACK_GROUND);
    if (s.fd != nullptr)
    {
      fclose(s.fd);
    }
  }
  channel.backgroundServices.clear();

  stopSourcedumping();
  soundOut->stop();

  _show_epg_label(false);

  if (my_contentTable != nullptr)
  {
    my_contentTable->hide();
    delete my_contentTable;
    my_contentTable = nullptr;
  }
  //	note framedumping - if any - was already stopped
  //	ficDumping - if on - is stopped here
  if (ficDumpPointer != nullptr)
  {
    my_dabProcessor->stop_ficDump();
    ficDumpPointer = nullptr;
  }
  epgTimer.stop();
  mpInputDevice->stopReader();
  my_dabProcessor->stop();
  usleep(1000);
  theTechWindow->cleanUp();

  show_pause_slide();
  presetTimer.stop();
  channelTimer.stop();
  channel.clean_channel();
  channel.targetPos = cmplx(0, 0);
  if (transmitterTags_local && (mapHandler != nullptr))
  {
    mapHandler->putData(MAP_RESET, channel.targetPos, "", "", "", 0, 0, 0, 0);
  }
  transmitter_country->setText("");
  transmitter_coordinates->setText("");

  enable_ui_elements_for_safety(false);  // hide some buttons
  QCoreApplication::processEvents();

  //	no processing left at this time
  usleep(1000);    // may be handling pending signals?
  channel.currentService.valid = false;
  channel.nextService.valid = false;

  //	all stopped, now look at the GUI elements
  ficError_display->setValue(0);
  //	the visual elements related to service and channel
  slot_set_synced(false);
  ensembleId->setText("");
  transmitter_coordinates->setText(" ");

  serviceList.clear();
  //model.clear();
  //ensembleDisplay->setModel(&model);
  cleanScreen();
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
  if (!running.load())
  {
    return;
  }

  LOG("select channel ", channel.toUtf8().data());
  presetTimer.stop();
  stopScanning(false);
  stopChannel();
  startChannel(channel);
}

////////////////////////////////////////////////////////////////////////
//
//	scanning
/////////////////////////////////////////////////////////////////////////

void RadioInterface::_slot_handle_scan_button()
{
  if (!running.load())
  {
    return;
  }
  if (scanning.load())
  {
    stopScanning(false);
  }
  else
  {
    startScanning();
  }
}

void RadioInterface::startScanning()
{
  presetTimer.stop();
  channelTimer.stop();
  epgTimer.stop();
  connect(my_dabProcessor, &DabProcessor::signal_no_signal_found, this, &RadioInterface::slot_no_signal_found);
  stopChannel();
  const int cc = theBand.firstChannel();

  LOG("scanning starts with ", QString::number(cc));
  scanning.store(true);

//  if (my_scanTable == nullptr)
//  {
//    my_scanTable = new ContentTable(this, dabSettings, "scan", my_dabProcessor->scanWidth());
//  }
//  else
//  {
//    my_scanTable->clearTable();
//  }

  mpServiceListHandler->delete_table(false);
  mpServiceListHandler->create_new_table();

  QString topLine = QString("ensemble") + ";" + "channelName" + ";" + "frequency (KHz)" + ";" + "Eid" + ";" + "time" + ";" + "tii" + ";" + "SNR" + ";" + "nr services" + ";";
//  my_scanTable->addLine(topLine);
//  my_scanTable->addLine("\n");

  my_dabProcessor->set_scanMode(true);
  //  To avoid reaction of the system on setting a different value:
  new_channelIndex(cc);
  lblDynLabel->setText("Scanning channel " + channelSelector->currentText());
  btnScanning->start_animation();
  const int32_t switchDelay = dabSettings->value("switchDelay", SWITCH_DELAY).toInt();
  channelTimer.start(switchDelay * 1000);

  startChannel(channelSelector->currentText());
}

//
//	stop_scanning is called
//	1. when the scanbutton is touched during scanning
//	2. on user selection of a service or a channel select
//	3. on device selection
//	4. on handling a reset
void RadioInterface::stopScanning(bool dump)
{
  disconnect(my_dabProcessor, &DabProcessor::signal_no_signal_found, this, &RadioInterface::slot_no_signal_found);
  (void)dump;
  if (scanning.load())
  {
    btnScanning->stop_animation();
    LOG("scanning stops ", "");
    my_dabProcessor->set_scanMode(false);
    lblDynLabel->setText("Scan ended");
    channelTimer.stop();
    scanning.store(false);
    mpServiceListHandler->restore_favorites(); // try to restore former favorites from an backup table
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

  disconnect(my_dabProcessor, &DabProcessor::signal_no_signal_found, this, &RadioInterface::slot_no_signal_found);
  channelTimer.stop();
  disconnect(&channelTimer, &QTimer::timeout, this, &RadioInterface::_slot_channel_timeout);

  if (running.load() && scanning.load())
  {
    int cc = channelSelector->currentIndex();
    if (!serviceList.empty())
    {
      showServices();
    }
    stopChannel();
    cc = theBand.nextChannel(cc);
    fprintf(stdout, "going to channel %d\n", cc);
    if (cc >= channelSelector->count())
    {
      stopScanning(true);
    }
    else
    {  // we just continue
      if (cc >= channelSelector->count())
      {
        cc = theBand.firstChannel();
      }
      //	To avoid reaction of the system on setting a different value:
      new_channelIndex(cc);

      connect(my_dabProcessor, &DabProcessor::signal_no_signal_found, this, &RadioInterface::slot_no_signal_found);
      connect(&channelTimer, &QTimer::timeout, this, &RadioInterface::_slot_channel_timeout);

      lblDynLabel->setText("Scanning channel " + channelSelector->currentText());
      const int32_t switchDelay = dabSettings->value("switchDelay", SWITCH_DELAY).toInt();
      channelTimer.start(switchDelay * 1000);
      startChannel(channelSelector->currentText());
    }
  }
  else if (scanning.load())
  {
    stopScanning(false);
  }
}

////////////////////////////////////////////////////////////////////////////
//
// showServices
////////////////////////////////////////////////////////////////////////////

void RadioInterface::showServices()
{
//  int scanMode = configWidget.scanmodeSelector->currentIndex();
//  QString SNR = "SNR " + QString::number(channel.snr);
//
//  if (my_dabProcessor == nullptr)
//  {  // cannot happen
//    fprintf(stderr, "Expert error 26\n");
//    return;
//  }
//
//  QString utcTime = convertTime(UTC.year, UTC.month, UTC.day, UTC.hour, UTC.minute);
//  QString headLine = channel.ensembleName + ";" + channel.channelName + ";" + QString::number(channel.nominalFreqHz / 1000) + ";"
//                   + hextoString(channel.Eid) + " " + ";" + transmitter_coordinates->text() + " " + ";" + utcTime + ";" + SNR + ";"
//                   + QString::number(serviceList.size()) + ";" + lblStationLocation->text();
//  QStringList s = my_dabProcessor->basicPrint();
//  my_scanTable->addLine(headLine);
//  my_scanTable->addLine("\n;\n");
//  for (const auto & i : s)
//  {
//    my_scanTable->addLine(i);
//  }
//  my_scanTable->addLine("\n;\n;\n");
//  my_scanTable->show();
}

/////////////////////////////////////////////////////////////////////
//
std::vector<serviceId> RadioInterface::insert_sorted(const std::vector<serviceId> & iList, const serviceId & iEntry)
{
  std::vector<serviceId> oList;

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
  configWidget.dumpButton->setEnabled(iEnable);
  btnPrevService->setEnabled(iEnable);
  btnNextService->setEnabled(iEnable);
  configWidget.contentButton->setEnabled(iEnable);
}

void RadioInterface::_slot_handle_mute_button()
{
  mutingActive = !mutingActive;
  btnMuteAudio->setIcon(QIcon(mutingActive ? ":res/icons/muted24.png" : ":res/icons/unmuted24.png"));
  btnMuteAudio->setIconSize(QSize(24, 24));
  btnMuteAudio->setFixedSize(QSize(32, 32));
}

void RadioInterface::new_channelIndex(int index)
{
  if (channelSelector->currentIndex() == index)
  {
    return;
  }
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
    disconnect(channelSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_handle_channel_selector);
#else
    disconnect(channelSelector, qOverload<const QString &>(&QComboBox::activated), this, &RadioInterface::_slot_handle_channel_selector);
#endif
  channelSelector->blockSignals(true);
  emit signal_set_new_channel(index);
  while (channelSelector->currentIndex() != index)
  {
    usleep(2000);
  }
  channelSelector->blockSignals(false);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
  connect(channelSelector, &QComboBox::textActivated, this, &RadioInterface::_slot_handle_channel_selector);
#else
  connect(channelSelector, qOverload<const QString &>(&QComboBox::activated), this, &RadioInterface::_slot_handle_channel_selector);
#endif
}


/////////////////////////////////////////////////////////////////////////
//	External configuration items				//////

void RadioInterface::_slot_handle_switch_delay_setting(int newV)
{
  dabSettings->setValue("switchDelay", newV);
}

void RadioInterface::_slot_handle_save_service_selector(int d) // TODO: is that choice useful? Always active?
{
  (void)d;
  dabSettings->setValue("has-presetName", configWidget.saveServiceSelector->isChecked() ? 1 : 0);
}

//-------------------------------------------------------------------------
//------------------------------------------------------------------------
//
//	if configured, the interpreation of the EPG data starts automatically,
//	the servicenames of an SPI/EPG service may differ from one country
//	to another
void RadioInterface::slot_epg_timer_timeout()
{
  epgTimer.stop();

  if (dabSettings->value("epgFlag", 0).toInt() != 1)
  {
    return;
  }
  if (scanning.load())
  {
    return;
  }
  for (auto serv: serviceList)
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
      my_dabProcessor->dataforPacketService(serv.name, &pd, 0);

      if ((!pd.defined) || (pd.DSCTy == 0) || (pd.bitRate == 0))
      {
        continue;
      }

      if (pd.DSCTy == 60)
      {
        LOG("hidden service started ", serv.name);
        _show_epg_label(true);
        fprintf(stdout, "Starting hidden service %s\n", serv.name.toUtf8().data());
        my_dabProcessor->set_dataChannel(&pd, &dataBuffer, BACK_GROUND);
        DabService s;
        s.channel = pd.channel;
        s.serviceName = pd.serviceName;
        s.SId = pd.SId;
        s.SCIds = pd.SCIds;
        s.subChId = pd.subchId;
        s.fd = nullptr;
        channel.backgroundServices.push_back(s);

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

uint32_t RadioInterface::extract_epg(const QString& iName, const std::vector<serviceId> & iServiceList, uint32_t ensembleId)
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
  if (my_dabProcessor != nullptr)
  {
    my_dabProcessor->set_epgData(SId, theTime, theText, theDescr);
  }
}

void RadioInterface::_slot_handle_time_table()
{
  int epgWidth;
  if (!channel.currentService.valid || !channel.currentService.is_audio)
  {
    return;
  }

  my_timeTable->clear();
  epgWidth = dabSettings->value("epgWidth", 70).toInt();
  if (epgWidth < 50)
  {
    epgWidth = 50;
  }
  std::vector<EpgElement> res = my_dabProcessor->find_epgData(channel.currentService.SId);
  for (const auto & element: res)
  {
    my_timeTable->addElement(element.theTime, epgWidth, element.theText, element.theDescr);
  }
}

void RadioInterface::_slot_handle_skip_list_button()
{
  if (!theBand.isHidden())
  {
    theBand.hide();
  }
  else
  {
    theBand.show();
  }
}

void RadioInterface::_slot_handle_skip_file_button()
{
  const QString fileName = filenameFinder.findskipFile_fileName();
  theBand.saveSettings();
  theBand.setup_skipList(fileName);
}

void RadioInterface::_slot_handle_tii_detector_mode(bool iIsChecked)
{
  assert(my_dabProcessor != nullptr);
  my_dabProcessor->set_tiiDetectorMode(iIsChecked);
  channel.transmitters.clear();
  dabSettings->setValue("tii_detector", (iIsChecked) ? 1 : 0);
}

void RadioInterface::_slot_handle_dc_avoidance_algorithm(bool iIsChecked)
{
  assert(my_dabProcessor != nullptr);
  dabSettings->setValue("dcAvoidance", (iIsChecked ? 1 : 0)); // write settings before action as file operation can influence sample activities
  my_dabProcessor->set_dc_avoidance_algorithm(iIsChecked);
}

void RadioInterface::_slot_handle_dc_removal(bool iIsChecked)
{
  assert(my_dabProcessor != nullptr);
  dabSettings->setValue("dcRemoval", (iIsChecked ? 1 : 0)); // write settings before action as file operation can influence sample activities
  my_dabProcessor->set_dc_removal(iIsChecked);
}

void RadioInterface::_slot_handle_dl_text_button()
{
  if (dlTextFile != nullptr)
  {
    fclose(dlTextFile);
    dlTextFile = nullptr;
    configWidget.dlTextButton->setText("dlText");
    return;
  }

  QString fileName = filenameFinder.finddlText_fileName(true);
  dlTextFile = fopen(fileName.toUtf8().data(), "w+");
  if (dlTextFile == nullptr)
  {
    return;
  }
  configWidget.dlTextButton->setText("writing");
}


void RadioInterface::_slot_handle_config_button()
{
  if (!configDisplay.isHidden())
  {
    configDisplay.hide();
  }
  else
  {
    configDisplay.show();
  }
}

void RadioInterface::slot_nr_services(int n)
{
  channel.serviceCount = n; // only a minor channel transmit this information, so do not rely on this
}

void RadioInterface::LOG(const QString & a1, const QString & a2)
{
  if (logFile == nullptr)
  {
    return;
  }

  QString theTime;
  if (configWidget.utcSelector->isChecked())
  {
    theTime = convertTime(mUTC.year, mUTC.month, mUTC.day, mUTC.hour, mUTC.minute);
  }
  else
  {
    theTime = QDateTime::currentDateTime().toString();
  }

  fprintf(logFile, "at %s: %s %s\n", theTime.toUtf8().data(), a1.toUtf8().data(), a2.toUtf8().data());
}

void RadioInterface::_slot_handle_logger_button(int s)
{
  (void)s;
  if (configWidget.loggerButton->isChecked())
  {
    if (logFile != nullptr)
    {
      fprintf(stderr, "should not happen (logfile)\n");
      return;
    }
    logFile = filenameFinder.findLogFileName();
    if (logFile != nullptr)
    {
      LOG("Log started with ", mpInputDevice->deviceName());
    }
  }
  else if (logFile != nullptr)
  {
    fclose(logFile);
    logFile = nullptr;
  }
}

void RadioInterface::_slot_handle_set_coordinates_button()
{
  Coordinates theCoordinator(dabSettings);
  (void)theCoordinator.QDialog::exec();
  float local_lat = dabSettings->value("latitude", 0).toFloat();
  float local_lon = dabSettings->value("longitude", 0).toFloat();
  channel.localPos = cmplx(local_lat, local_lon);
}

void RadioInterface::_slot_load_table()
{
  QString tableFile = dabSettings->value("tiiFile", "").toString();

  if (tableFile == "")
  {
    tableFile = QDir::homePath() + "/.txdata.tii";
    dabSettings->setValue("tiiFile", tableFile);
  }
  tiiHandler.loadTable(tableFile);
  if (tiiHandler.is_valid())
  {
    QMessageBox::information(this, tr("success"), tr("Loading and installing database complete\n"));
    channel.tiiFile = tiiHandler.fill_cache_from_tii_file(tableFile);
  }
  else
  {
    QMessageBox::information(this, tr("fail"), tr("Loading database failed\n"));
    channel.tiiFile = false;
  }
}

//
//	ensure that we only get a handler if we have a start location
void RadioInterface::_slot_handle_http_button()
{
  if (real(channel.localPos) == 0)
  {
    QMessageBox::information(this, "Data missing", "Provide your coordinates first on the configuration window!");
    return;
  }

  if (mapHandler == nullptr)
  {
    QString browserAddress = dabSettings->value("browserAddress", "http://localhost").toString();
    QString mapPort = dabSettings->value("mapPort", 8080).toString();

    QString mapFile;
    if (dabSettings->value("saveLocations", 0).toInt() == 1)
    {
      mapFile = filenameFinder.findMaps_fileName();
    }
    else
    {
      mapFile = "";
    }
    mapHandler = new httpHandler(this,
                                 mapPort,
                                 browserAddress,
                                 channel.localPos,
                                 mapFile,
                                 dabSettings->value("autoBrowser", 1).toInt() == 1);
    maxDistance = -1;
    if (mapHandler != nullptr)
    {
      _set_http_server_button(true);
    }
  }
  else
  {
    locker.lock();
    delete mapHandler;
    mapHandler = nullptr;
    locker.unlock();
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

void RadioInterface::_slot_handle_auto_browser(int d)
{
  (void)d;
  dabSettings->setValue("autoBrowser", configWidget.autoBrowser->isChecked() ? 1 : 0);
}

void RadioInterface::_slot_handle_transmitter_tags(int d)
{
  (void)d;
  maxDistance = -1;
  transmitterTags_local = configWidget.transmitterTags->isChecked();
  dabSettings->setValue("transmitterTags", transmitterTags_local ? 1 : 0);
  channel.targetPos = cmplx(0, 0);
  if ((transmitterTags_local) && (mapHandler != nullptr))
  {
    mapHandler->putData(MAP_RESET, channel.targetPos, "", "", "", 0, 0, 0, 0);
  }
}

void RadioInterface::_slot_handle_on_top(int d)
{
  bool onTop = false;
  (void)d;
  if (configWidget.onTop->isChecked())
  {
    onTop = true;
  }
  dabSettings->setValue("onTop", onTop ? 1 : 0);
}

void RadioInterface::show_pause_slide()
{
  QPixmap p;
  if (p.load(":res/DABLogoInvSmall.png", "png"))
  {
    write_picture(p);
  }
}

void RadioInterface::_slot_handle_port_selector()
{
  mapPortHandler theHandler(dabSettings);
  (void)theHandler.QDialog::exec();
}

void RadioInterface::_slot_handle_transm_selector(int x)
{
  (void)x;
  dabSettings->setValue("saveLocations", configWidget.transmSelector->isChecked() ? 1 : 0);
}

void RadioInterface::_slot_handle_save_slides(int x)
{
  (void)x;
  dabSettings->setValue("saveSlides", configWidget.saveSlides->isChecked() ? 1 : 0);
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
  if (my_dabProcessor == nullptr)
  {  // should not happen
    return;
  }

  if (channel.etiActive)
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
  if (!channel.etiActive)
  {
    return;
  }

  my_dabProcessor->stop_etiGenerator();
  channel.etiActive = false;
  btnScanning->setText("ETI");
}

void RadioInterface::start_etiHandler()
{
  if (channel.etiActive)
  {
    return;
  }

  QString etiFile = filenameFinder.find_eti_fileName(channel.ensembleName, channel.channelName);
  if (etiFile == QString(""))
  {
    return;
  }
  LOG("etiHandler started", etiFile);
  channel.etiActive = my_dabProcessor->start_etiGenerator(etiFile);
  if (channel.etiActive)
  {
    btnScanning->setText("eti runs");
  }
}

void RadioInterface::_slot_handle_eti_active_selector(int k)
{
  (void)k;
  bool setting = configWidget.eti_activeSelector->isChecked();
  if (mpInputDevice == nullptr)
  {
    return;
  }

  if (setting)
  {
    stopScanning(false);
    disconnect(btnScanning, &QPushButton::clicked, this, &RadioInterface::_slot_handle_scan_button);
    connect(btnScanning, &QPushButton::clicked, this, &RadioInterface::_slot_handle_eti_handler);
    btnScanning->setText("ETI");
    if (mpInputDevice->isFileInput())
    {  // restore the button' visibility
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
  //sFreqOffHz = 2 * (iVal - 500);
  const int32_t newFreqOffHz = (iVal - 500);
  if (my_dabProcessor)
  {
    my_dabProcessor->add_bb_freq(newFreqOffHz);
  }
  //inputDevice->setVFOFrequency(sFreqOffHz + newFreqOffHz);
  //  sFreqOffHz = inputDevice->getVFOFrequency();

  QString s("Freq-Offs [Hz]: ");
  s += QString::number(newFreqOffHz);
  configWidget.sliderTest->setToolTip(s);
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
  mpServiceListHandler->set_favorite(mCurFavoriteState);
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
  font.setWeight(75);
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



