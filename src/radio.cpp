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
#include  <QCoreApplication>
#include  <QSettings>
#include  <QMessageBox>
#include  <QFileDialog>
#include  <QDateTime>
#include  <QStringList>
#include  <QStringListModel>
#include  <QMouseEvent>
#include  <QDir>
#include  <fstream>
#include  "dab-constants.h"
#include  "mot-content-types.h"
#include  <numeric>
#include  <unistd.h>
#include  <vector>
#include  "radio.h"
#include  "rawfiles.h"
#include  "wavfiles.h"
#include  "xml-filereader.h"
#include  "dab-tables.h"
#include  "ITU_Region_1.h"
#include  "coordinates.h"
#include  "mapport.h"
#include  "techdata.h"

#ifdef  TCP_STREAMER
#include	"tcp-streamer.h"
#elif  QT_AUDIO
#include	"Qt-audio.h"
#else

#include  "audiosink.h"

#endif
#ifdef  HAVE_RTLSDR

#include  "rtlsdr-handler.h"

#endif
#ifdef  HAVE_SDRPLAY_V2
#include	"sdrplay-handler-v2.h"
#endif
#ifdef  HAVE_SDRPLAY_V3

#include  "sdrplay-handler-v3.h"

#endif
#ifdef  __MINGW32__
                                                                                                                        #ifdef	HAVE_EXTIO
#include	"extio-handler.h"
#endif
#endif
#ifdef  HAVE_RTL_TCP
#include	"rtl_tcp_client.h"
#endif
#ifdef  HAVE_AIRSPY
#include	"airspy-handler.h"
#endif
#ifdef  HAVE_HACKRF
#include	"hackrf-handler.h"
#endif
#ifdef  HAVE_LIME
#include	"lime-handler.h"
#endif
#ifdef  HAVE_PLUTO_2
#include	"pluto-handler-2.h"
#elif  HAVE_PLUTO_RXTX
                                                                                                                        #include	"pluto-rxtx-handler.h"
#include	"dab-streamer.h"
#endif
#ifdef  HAVE_SOAPY
#include	"soapy-handler.h"
#endif
#ifdef  HAVE_ELAD
#include	"elad-handler.h"
#endif

#include  "history-handler.h"
#include  "time-table.h"

#include  "device-exceptions.h"
#include  "pause_slide.h"

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
  total_time = std::accumulate(cpu_times.begin(), cpu_times.end(), 0);
  return true;
}

#endif

#define  SINGLE_SCAN    0
#define  SCAN_TO_DATA    1
#define  SCAN_CONTINUOUSLY  2

const char * scanTextTable[3] = {
  "single scan", "scan to data", "scan continuously" };

static inline QString scanmodeText(int e)
{
  return QString(scanTextTable[e]);
}

static uint8_t convert(QString s)
{
  if (s == "Mode 1")
  {
    return 1;
  }
  if (s == "Mode 2")
  {
    return 2;
  }
  if (s == "Mode 3")
  {
    return 3;
  }
  if (s == "Mode 4")
  {
    return 4;
  }
  return 1;
}

RadioInterface::RadioInterface(QSettings * Si, const QString & presetFile, const QString & freqExtension, bool error_report, int32_t dataPort, int32_t clockPort, int fmFrequency, QWidget * parent)
  : QWidget(parent),
    spectrumBuffer(2 * 32768),
    iqBuffer(2 * 1536),
    carrBuffer(2 * 1536),
    tiiBuffer(32768),
    //nullBuffer(32768),
    snrBuffer(512),
    responseBuffer(32768),
    frameBuffer(2 * 32768),
    dataBuffer(32768),
    audioBuffer(8 * 32768),
    my_spectrumViewer(this, Si, &spectrumBuffer, &iqBuffer, &carrBuffer, &responseBuffer),
    //my_correlationViewer(this, Si, &responseBuffer),
    my_tiiViewer(this, Si, &tiiBuffer),
    my_snrViewer(this, Si),
    my_presetHandler(this),
    theBand(freqExtension, Si),
    configDisplay(nullptr),
    the_dlCache(10),
    tiiHandler(),
    filenameFinder(Si),
    theTechData(16 * 32768)
{
  int16_t latency;
  int16_t k;
  QString h;
  uint8_t dabBand;

  dabSettings = Si;
  this->error_report = error_report;
  this->fmFrequency = fmFrequency;
  this->dlTextFile = nullptr;
  this->ficDumpPointer = nullptr;
  running.store(false);
  scanning.store(false);
  my_dabProcessor = nullptr;
  stereoSetting = false;
  maxDistance = -1;
  my_contentTable = nullptr;
  my_scanTable = nullptr;
  mapHandler = nullptr;
  //	"globals" is introduced to reduce the number of parameters
  //	for the dabProcessor
  globals.spectrumBuffer = &spectrumBuffer;
  globals.iqBuffer = &iqBuffer;
  globals.carrBuffer = &carrBuffer;
  globals.responseBuffer = &responseBuffer;
  globals.tiiBuffer = &tiiBuffer;
  //globals.nullBuffer = &nullBuffer;
  globals.snrBuffer = &snrBuffer;
  globals.frameBuffer = &frameBuffer;

  latency = dabSettings->value("latency", 5).toInt();

  QString dabMode = dabSettings->value("dabMode", "Mode 1").toString();
  globals.dabMode = convert(dabMode);
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

#ifdef  _SEND_DATAGRAM_
                                                                                                                          ipAddress		= dabSettings -> value ("ipAddress", "127.0.0.1"). toString();
	port			= dabSettings -> value ("port", 8888). toInt();
#endif
  //
  //	set on top or not? checked at start up
  if (dabSettings->value("onTop", 0).toInt() == 1)
  {
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
  }

  //	The settings are done, now creation of the GUI parts
  setupUi(this);

  setWindowIcon(QIcon(":res/DABplusLogoWB.png"));

  //setWindowTitle(QString(PRJ_NAME) + QString(" (V" PRJ_VERS ")"));
  setWindowTitle(PRJ_NAME);
  int x = dabSettings->value("mainWidget-x", 100).toInt();
  int y = dabSettings->value("mainWidget-y", 100).toInt();
  int wi = dabSettings->value("main-widget-w", 300).toInt();
  int he = dabSettings->value("main-widget-h", 200).toInt();
  this->resize(QSize(wi, he));
  this->move(QPoint(x, y));

  configWidget.setupUi(&configDisplay);
  x = dabSettings->value("configWidget-x", 200).toInt();
  y = dabSettings->value("configWidget-y", 200).toInt();
  wi = dabSettings->value("configWidget-w", 200).toInt();
  he = dabSettings->value("configWidget-h", 150).toInt();
  configDisplay.resize(QSize(wi, he));
  configDisplay.move(QPoint(x, y));

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

  configWidget.EPGLabel->hide();
  configWidget.EPGLabel->setStyleSheet("QLabel {background-color : yellow; color : black}");

  x = dabSettings->value("switchDelay", 8).toInt();
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

  connect(configWidget.loadTableButton, SIGNAL (clicked()), this, SLOT (_slot_load_table()));
  connect(configWidget.onTop, SIGNAL (stateChanged(int)), this, SLOT (_slot_handle_on_top(int)));
  connect(configWidget.transmSelector, SIGNAL (stateChanged(int)), this, SLOT (_slot_handle_transm_selector(int)));
  connect(configWidget.saveSlides, SIGNAL (stateChanged(int)), this, SLOT (_slot_handle_save_slides(int)));

  logFile = nullptr;
  int scanMode = dabSettings->value("scanMode", SINGLE_SCAN).toInt();
  configWidget.scanmodeSelector->setCurrentIndex(scanMode);

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

  x = dabSettings->value("serviceOrder", ALPHA_BASED).toInt();
  if (x == ALPHA_BASED)
  {
    configWidget.orderAlfabetical->setChecked(true);
  }
  else if (x == ID_BASED)
  {
    configWidget.orderServiceIds->setChecked(true);
  }
  else
  {
    configWidget.ordersubChannelIds->setChecked(true);
  }

  if (dabSettings->value("autoBrowser", 1).toInt() == 1)
  {
    configWidget.autoBrowser->setChecked(true);
  }

  if (dabSettings->value("transmitterTags", 1).toInt() == 1)
  {
    configWidget.transmitterTags->setChecked(true);
  }
  connect(configWidget.autoBrowser, SIGNAL (stateChanged(int)), this, SLOT (_slot_handle_auto_browser(int)));
  connect(configWidget.transmitterTags, SIGNAL (stateChanged(int)), this, SLOT (_slot_handle_transmitter_tags(int)));

  transmitterTags_local = configWidget.transmitterTags->isChecked();
  theTechWindow->hide();  // until shown otherwise
  serviceList.clear();
  model.clear();
  ensembleDisplay->setModel(&model);
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
#elif QT_AUDIO
  soundOut = new Qt_Audio();
  theTechWindow->hide();
#else
  //	just sound out
  soundOut = new AudioSink(latency);
  ((AudioSink *)soundOut)->setupChannels(configWidget.streamoutSelector);
  configWidget.streamoutSelector->show();
  bool err;
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
  picturesPath += PRJ_NAME "-files/";
  picturesPath = dabSettings->value("picturesPath", picturesPath).toString();
  picturesPath = checkDir(picturesPath);
  filePath = dabSettings->value("filePath", picturesPath).toString();
  if (filePath != "")
  {
    filePath = checkDir(filePath);
  }
  //
#ifndef  __MINGW32__
  epgPath = checkDir(QDir::tempPath());
#else
  epgPath		= checkDir (QDir::homePath ());
#endif
  epgPath += PRJ_NAME "-files/";
  epgPath = dabSettings->value("epgPath", epgPath).toString();
  epgPath = checkDir(epgPath);
  connect(&epgProcessor, &epgDecoder::signal_set_epg_data, this, &RadioInterface::slot_set_epg_data);
  //	timer for autostart epg service
  epgTimer.setSingleShot(true);
  connect(&epgTimer, SIGNAL (timeout()), this, SLOT (slot_epg_timer_timeout()));

  QString historyFile = QDir::toNativeSeparators(QDir::homePath() + "/.config/" APP_NAME "/stationlist.xml");
  historyFile = dabSettings->value("history", historyFile).toString();
  historyFile = QDir::toNativeSeparators(historyFile);
  my_history = new historyHandler(this, historyFile);
  my_timeTable = new timeTableHandler(this);
  my_timeTable->hide();

  connect(my_history, &historyHandler::handle_historySelect, this, &RadioInterface::_slot_handle_history_select);
  connect(this, SIGNAL (signal_set_new_channel(int)), channelSelector, SLOT (setCurrentIndex(int)));
  connect(this, SIGNAL (signal_set_new_preset_index(int)), presetSelector, SLOT (setCurrentIndex(int)));
  connect(configWidget.dlTextButton, SIGNAL (clicked()), this, SLOT (_slot_handle_dl_text_button()));
  connect(configWidget.loggerButton, SIGNAL (stateChanged(int)), this, SLOT (_slot_handle_logger_button(int)));
  connect(httpButton, SIGNAL (clicked()), this, SLOT (_slot_handle_http_button()));

  //	restore some settings from previous incarnations
  QString t = dabSettings->value("dabBand", "VHF Band III").toString();
  dabBand = t == "VHF Band III" ? BAND_III : L_BAND;

  theBand.setupChannels(channelSelector, dabBand);
  QString skipfileName = dabSettings->value("skipFile", "").toString();
  theBand.setup_skipList(skipfileName);

  QPalette p = configWidget.ficError_display->palette();
  p.setColor(QPalette::Highlight, Qt::red);
  configWidget.ficError_display->setPalette(p);
  p.setColor(QPalette::Highlight, Qt::green);
  //
  audioDumper = nullptr;
  rawDumper = nullptr;
  ficBlocks = 0;
  ficSuccess = 0;
  total_ficError = 0;
  total_fics = 0;

  connect(configWidget.streamoutSelector, SIGNAL (activated(int)), this, SLOT (slot_set_stream_selector(int)));
  connect(theTechWindow, &TechData::handle_timeTable, this, &RadioInterface::_slot_handle_time_table);

  my_presetHandler.loadPresets(presetFile, presetSelector);

  //	display the version
  copyrightLabel->setToolTip(footText());
  presetSelector->setToolTip(presetText());

  connect(configWidget.portSelector, SIGNAL (clicked()), this, SLOT (_slot_handle_port_selector()));
  connect(configWidget.set_coordinatesButton, SIGNAL (clicked()), this, SLOT (_slot_handle_set_coordinates_button()));
  QString tiiFileName = dabSettings->value("tiiFile", "").toString();
  channel.tiiFile = false;
  if (tiiFileName != "")
  {
    channel.tiiFile = tiiHandler.fill_cache_from_tii_file(tiiFileName);
    if (!channel.tiiFile)
    {
      httpButton->hide();
    }
  }

  connect(configWidget.eti_activeSelector, SIGNAL (stateChanged(int)), this, SLOT (_slot_handle_eti_active_selector(int)));

  channel.etiActive = false;
  show_pause_slide();

  //	and start the timer(s)
  //	The displaytimer is there to show the number of
  //	seconds running and handle - if available - the tii data
  displayTimer.setInterval(1000);
  connect(&displayTimer, SIGNAL (timeout()), this, SLOT (_slot_update_time_display()));
  displayTimer.start(1000);
  numberofSeconds = 0;

  //	timer for scanning
  channelTimer.setSingleShot(true);
  channelTimer.setInterval(10000);
  connect(&channelTimer, SIGNAL (timeout()), this, SLOT (_slot_channel_timeout()));
  //
  //	presetTimer
  presetTimer.setSingleShot(true);
  connect(&presetTimer, SIGNAL (timeout()), this, SLOT (_slot_set_preset_service()));

  QPalette lcdPalette;
#ifndef __MAC__
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
  lcdPalette.setColor(QPalette::Window, Qt::white);
#else
  lcdPalette. setColor (QPalette::Background, Qt::white);
#endif
  lcdPalette.setColor(QPalette::Base, Qt::black);
#endif
  configWidget.frequencyDisplay->setPalette(lcdPalette);
  configWidget.frequencyDisplay->setAutoFillBackground(true);
  configWidget.cpuMonitor->setPalette(lcdPalette);
  configWidget.cpuMonitor->setAutoFillBackground(true);

  localTimeDisplay->setStyleSheet("QLabel {background-color : gray; color: white}");
  runtimeDisplay->setStyleSheet("QLabel {background-color : gray; color: white}");

  //	add devices to the list
  configWidget.deviceSelector->addItem("select input");
  configWidget.deviceSelector->addItem("file input(.raw)");
  configWidget.deviceSelector->addItem("file input(.iq)");
  configWidget.deviceSelector->addItem("file input(.sdr)");
  configWidget.deviceSelector->addItem("xml files");
#ifdef  HAVE_SDRPLAY_V3
  configWidget.deviceSelector->addItem("sdrplay");
#endif
#ifdef  HAVE_SDRPLAY_V2
  configWidget. deviceSelector	-> addItem ("sdrplay-v2");
#endif
#ifdef  HAVE_RTLSDR
  configWidget.deviceSelector->addItem("dabstick");
#endif
#ifdef  HAVE_AIRSPY
  configWidget. deviceSelector	-> addItem ("airspy");
#endif
#ifdef  HAVE_HACKRF
  configWidget. deviceSelector	-> addItem ("hackrf");
#endif
#ifdef  HAVE_LIME
  configWidget. deviceSelector	-> addItem ("limeSDR");
#endif
#ifdef  HAVE_PLUTO_RXTX
                                                                                                                          configWidget. deviceSelector	-> addItem ("pluto-rxtx");
	streamerOut	= nullptr;
#elif  HAVE_PLUTO_2
  configWidget. deviceSelector	-> addItem ("pluto");
#endif
#ifdef  HAVE_RTL_TCP
  configWidget. deviceSelector	-> addItem ("rtl_tcp");
#endif
#ifdef  HAVE_SOAPY
  configWidget. deviceSelector	-> addItem ("soapy");
#endif
#ifdef  HAVE_EXTIO
  configWidget. deviceSelector	-> addItem ("extio");
#endif
#ifdef  HAVE_ELAD
  configWidget. deviceSelector	-> addItem ("elad-s1");
#endif
  inputDevice = nullptr;
  h = dabSettings->value("device", "no device").toString();
  k = configWidget.deviceSelector->findText(h);
  //	fprintf (stdout, "%d %s\n", k, h. toUtf8(). data());
  if (k != -1)
  {
    configWidget.deviceSelector->setCurrentIndex(k);
    inputDevice = setDevice(configWidget.deviceSelector->currentText());
    if (inputDevice != nullptr)
    {
      dabSettings->setValue("device", configWidget.deviceSelector->currentText());
    }
  }

  bool hidden = dabSettings->value("hidden", 0).toInt() != 0;
  if (hidden)
  {
    configButton->setText("Show Controls");
    configDisplay.hide();
  }
  else
  {
    configButton->setText("Hide Controls");
    configDisplay.show();
  }

  connect(configButton, SIGNAL (clicked()), this, SLOT (_slot_handle_config_button()));

  if (inputDevice != nullptr)
  {
    if (dabSettings->value("deviceVisible", 1).toInt() != 0)
    {
      inputDevice->show();
    }
    else
    {
      inputDevice->hide();
    }
  }

  if (dabSettings->value("spectrumVisible", 0).toInt() == 1)
  {
    my_spectrumViewer.show();
  }
  if (dabSettings->value("tiiVisible", 0).toInt() == 1)
  {
    my_tiiViewer.show();
  }
//  if (dabSettings->value("correlationVisible", 0).toInt() == 1)
//  {
//    my_correlationViewer.show();
//  }
  if (dabSettings->value("snrVisible", 0).toInt() == 1)
  {
    my_snrViewer.show();
  }
  if (dabSettings->value("techDataVisible", 0).toInt() == 1)
  {
    theTechWindow->show();
  }

  //	if a device was selected, we just start, otherwise
  //	we wait until one is selected

  if (inputDevice != nullptr)
  {
    if (doStart())
    {
      qApp->installEventFilter(this);
      return;
    }
    else
    {
      delete inputDevice;
      inputDevice = nullptr;
    }
  }
  if (hidden)
  {  // make it visible
    configButton->setText("Hide Controls");
    dabSettings->setValue("hidden", 0);
  }
  configDisplay.show();
  connect(configWidget.deviceSelector, &smallComboBox::textActivated, this, &RadioInterface::_slot_do_start);
  qApp->installEventFilter(this);
}

QString RadioInterface::presetText()
{
  return QString("Click with the right mouse button on the text of the item and the item will be removed from the presetList");
}

QString RadioInterface::footText()
{
  version = QString(PRJ_VERS);
  QString versionText = "<html><head/><body><p>";
  versionText =  "<h3>" + QString(PRJ_NAME) + " V" + version + " (Qt " QT_VERSION_STR ")</h3>";
  versionText += "<p><b>Built on " + QString(__TIMESTAMP__) + QString("<br/>Commit ") + QString(GITHASH) + ".</b></p>";
  versionText += "<p>Forked and partly extensive changed and extended by Thomas Neder<br/>"
                 "(<a href=\"https://github.com/tomneda/DABstar\">https://github.com/tomneda/DABstar</a>) from Qt-DAB<br/>"
                 "(<a href=\"https://github.com/JvanKatwijk/qt-dab\">https://github.com/JvanKatwijk/qt-dab</a>) by Jan van Katwijk<br/>"
                 "(<a href=\"mailto:J.vanKatwijk@gmail.com\">J.vanKatwijk@gmail.com</a>).</p>";
  versionText += "<p>Rights of Qt, portaudio, libfaad, libsamplerate and libsndfile gratefully acknowledged.<br/>"
                 "Rights of developers of RTLSDR library, SDRplay libraries, AIRspy library and others gratefully acknowledged.<br/>"
                 "Copyright of DevSec Studio for the skin, made available under MIT license, is gratefully acknowledged.<br/>"
                 "Rights of other contributors gratefully acknowledged.</p>";
  versionText += "</p></body></html>";
  return versionText;
}

// _slot_do_start(QString) is called when - on startup - NO device was registered to be used, and the user presses the selectDevice comboBox
void RadioInterface::_slot_do_start(const QString & dev)
{
  (void)dev;
  inputDevice = setDevice(dev);
  //	Some buttons should not be touched before we have a device
  if (inputDevice == nullptr)
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
  my_dabProcessor = new DabProcessor(this, inputDevice, &globals);
  channel.cleanChannel();

  //	Some hidden buttons can be made visible now
  connectGUI();

  if (dabSettings->value("showDeviceWidget", 0).toInt() != 0)
  {
    inputDevice->show();
  }

  //	we avoided till now connecting the channel selector
  //	to the slot since that function does a lot more, things we
  //	do not want here
  connect(presetSelector, &presetComboBox::textActivated, this, &RadioInterface::slot_handle_preset_selector);
  connect(channelSelector, &smallComboBox::textActivated, this, &RadioInterface::_slot_handle_channel_selector);
  connect(&my_spectrumViewer, &SpectrumViewer::signal_cb_nom_carrier_changed, my_dabProcessor, &DabProcessor::slot_show_nominal_carrier);
  connect(&my_spectrumViewer, &SpectrumViewer::signal_cmb_carrier_changed, my_dabProcessor, &DabProcessor::slot_select_carrier_plot_type);

  //
  //	Just to be sure we disconnect here.
  //	It would have been helpful to have a function
  //	testing whether or not a connection exists, we need a kind
  //	of "reset"
  disconnect(configWidget.deviceSelector, &smallComboBox::textActivated, this, &RadioInterface::doStart);
  disconnect(configWidget.deviceSelector, SIGNAL (activated(const QString &)), this, SLOT (_slot_new_device(const QString &)));
  connect(configWidget.deviceSelector, SIGNAL (activated(const QString &)), this, SLOT (_slot_new_device(const QString &)));
  //
  if (channel.nextService.valid)
  {
    int switchDelay = dabSettings->value("switchDelay", 8).toInt();
    presetTimer.setSingleShot(true);
    presetTimer.setInterval(switchDelay * 1000);
    presetTimer.start(switchDelay * 1000);
  }

  bool dm = dabSettings->value("tii_detector", 0).toInt() == 1;
  if (dm)
  {
    configWidget.tii_detectorMode->setChecked(true);
  }
  my_dabProcessor->set_tiiDetectorMode(dm);
  connect(configWidget.tii_detectorMode, SIGNAL (stateChanged(int)), this, SLOT (_slot_handle_tii_detector_mode(int)));
  //
  //	after the preset timer signals, the service will be started
  startChannel(channelSelector->currentText());
  running.store(true);
  return true;
}

RadioInterface::~RadioInterface()
{
  fprintf(stdout, "RadioInterface is deleted\n");
}

//
///////////////////////////////////////////////////////////////////////////////
//
//	a slot called by the ofdmprocessor
void RadioInterface::slot_set_corrector_display(int v)
{
  if (!my_spectrumViewer.isHidden())
  {
    my_spectrumViewer.show_correction(v);
  }
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
  int serviceOrder;
  if (!running.load())
  {
    return;
  }

  if (my_dabProcessor->getSubChId(serviceName, SId) < 0)
  {
    return;
  }
  (void)SId;
  serviceId ed;
  ed.name = serviceName;
  ed.SId = SId;
  if (isMember(serviceList, ed))
  {
    return;
  }

  ed.subChId = my_dabProcessor->getSubChId(serviceName, SId);
  serviceOrder = dabSettings->value("serviceOrder", ALPHA_BASED).toInt();

  serviceList = insert(serviceList, ed, serviceOrder);
  my_history->addElement(channel.channelName, serviceName);
  model.clear();
  for (auto serv: serviceList)
  {
    model.appendRow(new QStandardItem(serv.name));
  }
#ifdef  __MINGW32__
                                                                                                                          for (int i = model. rowCount (); i < 12; i ++)
	   model. appendRow (new QStandardItem ("      "));
#endif
  for (int i = 0; i < model.rowCount(); i++)
  {
    model.setData(model.index(i, 0), QFont(theFont, fontSize), Qt::FontRole);
  }

  ensembleDisplay->setModel(&model);
  if (channel.serviceCount == model.rowCount() && !scanning)
  {
    presetTimer.stop();
    _slot_set_preset_service();
  }
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
  if (configWidget.scanmodeSelector->currentIndex() == SCAN_TO_DATA)
  {
    stopScanning(false);
  }
}
//
///////////////////////////////////////////////////////////////////////////

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
    theTime = convertTime(UTC.year, UTC.month, UTC.day, UTC.hour, UTC.minute);
  }
  else
  {
    theTime = convertTime(localTime.year, localTime.month, localTime.day, localTime.hour, localTime.minute);
  }

  QString header = channel.ensembleName + ";" + channel.channelName + ";" + QString::number(channel.frequency) + ";" + hextoString(channel.Eid) + " " + ";" + transmitter_coordinates->text() + " " + ";" + theTime + ";" + SNR + ";" + QString::number(
    serviceList.size()) + ";" + distanceLabel->text() + "\n";

  my_contentTable = new contentTable(this, dabSettings, channel.channelName, my_dabProcessor->scanWidth());
  connect(my_contentTable, &contentTable::goService, this, &RadioInterface::slot_handle_content_selector);

  my_contentTable->addLine(header);
  //	my_contentTable		-> addLine ("\n");
  for (int i = 0; i < s.size(); i++)
  {
    my_contentTable->addLine(s.at(i));
  }
  my_contentTable->show();
}

QString RadioInterface::checkDir(const QString s)
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

  case MOTBaseTypeText: save_MOTtext(result, contentType, objectName);
    break;

  case MOTBaseTypeImage: show_MOTlabel(result, contentType, objectName, dirElement);
    break;

  case MOTBaseTypeAudio: break;

  case MOTBaseTypeVideo: break;

  case MOTBaseTypeTransport: save_MOTObject(result, objectName);
    break;

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
      epgProcessor.process_epg(epgData.data(), epgData.size(), currentSId, subType, julianDate);
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
    fprintf(stdout, "going to write file %s\n", textName.toUtf8().data());
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
    static int counter = 0;
    name = "motObject_" + QString::number(counter);
    counter++;
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
      fprintf(stdout, "going to write file %s\n", pict.toUtf8().data());
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
  //constexpr int w = 400;
  //constexpr int h = 2 * w / 3.5;
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
  //pictureLabel->setFixedWidth(w+20);
  //pictureLabel->setFixedHeight(h+20);
  pictureLabel->setAlignment(Qt::AlignCenter);
  pictureLabel->show();
}

//
//	sendDatagram is triggered by the ip handler,
void RadioInterface::slot_send_datagram(int length)
{
  uint8_t localBuffer[length];

  if (dataBuffer.GetRingBufferReadAvailable() < length)
  {
    fprintf(stderr, "Something went wrong\n");
    return;
  }

  dataBuffer.getDataFromBuffer(localBuffer, length);
#ifdef  _SEND_DATAGRAM_
                                                                                                                          if (running. load()) {
	   dataOut_socket. writeDatagram ((const char *)localBuffer, length,
	                                   QHostAddress (ipAddress),
	                                   port);
	}

#endif
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
  if (dataBuffer.GetRingBufferReadAvailable() < length)
  {
    fprintf(stderr, "Something went wrong\n");
    return;
  }
#ifdef  DATA_STREAMER
                                                                                                                          dataBuffer. getDataFromBuffer (&localBuffer [8], length);
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
  int serviceOrder;
  if (!running.load() || my_dabProcessor == nullptr)
  {
    return;
  }
  dabService s;
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
  for (uint16_t i = 0; i < channel.backgroundServices.size(); i++)
  {
    my_dabProcessor->stop_service(channel.backgroundServices.at(i).subChId, BACK_GROUND);
  }

  fprintf(stdout, "change will be effected\n");
  serviceOrder = dabSettings->value("serviceOrder", ALPHA_BASED).toInt();


  //	we rebuild the services list from the fib and
  //	then we (try to) restart the service
  serviceList = my_dabProcessor->getServices(serviceOrder);
  model.clear();
  for (auto serv: serviceList)
  {
    model.appendRow(new QStandardItem(serv.name));
  }
  int row = model.rowCount();
  for (int i = 0; i < row; i++)
  {
    model.setData(model.index(i, 0), QFont(theFont, fontSize), Qt::FontRole);
  }
  ensembleDisplay->setModel(&model);
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

      for (int j = 0; j < model.rowCount(); j++)
      {
        QString itemText = model.index(j, 0).data(Qt::DisplayRole).toString();
        if (itemText == ss)
        {
          colorService(model.index(j, 0), Qt::blue, fontSize + 2, true);
        }
      }
    }
  }
}

//
//	In order to not overload with an enormous amount of
//	signals, we trigger this function at most 10 times a second
//
void RadioInterface::slot_new_audio(int amount, int rate)
{
  if (!running.load())
  {
    return;
  }

  static int teller = 0;
  if (!theTechWindow->isHidden())
  {
    teller++;
    if (teller > 10)
    {
      teller = 0;
      theTechWindow->showRate(rate);
    }
  }
  int16_t vec[amount];
  while (audioBuffer.GetRingBufferReadAvailable() > amount)
  {
    audioBuffer.getDataFromBuffer(vec, amount);
    if (!mutingActive)
    {
      soundOut->audioOut(vec, amount, rate);
    }
#ifdef HAVE_PLUTO_RXTX
    if (streamerOut != nullptr)
      {
        streamerOut->audioOut(vec, amount, rate);
      }
#endif
    if (!theTechWindow->isHidden())
    {
      theTechData.putDataIntoBuffer(vec, amount);
      theTechWindow->audioDataAvailable(amount, rate);
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

  if (my_scanTable != nullptr)
  {
    my_scanTable->clearTable();
    my_scanTable->hide();
    delete my_scanTable;
  }

  my_presetHandler.savePresets(presetSelector);
  theBand.saveSettings();
  stopFramedumping();
  stopSourcedumping();
  stopAudiodumping();
  //	theTable. hide		();
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
  //my_correlationViewer.hide();
  my_tiiViewer.hide();
  my_snrViewer.hide();
  if (my_dabProcessor != nullptr)
  {
    delete my_dabProcessor;
  }
  if (inputDevice != nullptr)
  {
    delete inputDevice;
  }
  delete soundOut;
  delete my_history;
  delete my_timeTable;
  //	close();
  fprintf(stdout, ".. end the radio silences\n");
}

//
static size_t previous_idle_time = 0;
static size_t previous_total_time = 0;

void RadioInterface::_slot_update_time_display()
{
  if (!running.load())
  {
    return;
  }

  numberofSeconds++;

  int16_t numberHours = numberofSeconds / 3600;
  int16_t numberMinutes = (numberofSeconds / 60) % 60;
  QString text = QString("Runtime: ") + QString::number(numberHours).rightJustified(2, '0') + ":" + QString::number(numberMinutes).rightJustified(2, '0');
  runtimeDisplay->setText(text);

  if ((numberofSeconds % 2) == 0)
  {
    size_t idle_time, total_time;
    get_cpu_times(idle_time, total_time);
    const float idle_time_delta = idle_time - previous_idle_time;
    const float total_time_delta = total_time - previous_total_time;
    const float utilization = 100.0 * (1.0 - idle_time_delta / total_time_delta);
    configWidget.cpuMonitor->display(utilization);
    previous_idle_time = idle_time;
    previous_total_time = total_time;
  }
  //
  //	The timer runs autonomously, so it might happen
  //	that it rings when there is no processor running
  if (my_dabProcessor == nullptr)
  {
    return;
  }
  if (soundOut->hasMissed())
  {
    int xxx = ((AudioSink *)soundOut)->missed();
    if (!theTechWindow->isHidden())
    {
      theTechWindow->showMissed(xxx);
    }
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
//	precondition: everything is quiet
deviceHandler * RadioInterface::setDevice(const QString & s)
{
  QString file;
  deviceHandler * inputDevice = nullptr;
  //	OK, everything quiet, now let us see what to do

  channel.realChannel = true;    // until proven otherwise
#ifdef  HAVE_SDRPLAY_V2
                                                                                                                          if (s == "sdrplay-v2") {
#ifdef	__MINGW32__
	   QMessageBox::warning (this, tr ("Warning"), tr ("If SDRuno is installed with drivers 3.10,\nV2.13 drivers will not work anymore, choose \"sdrplay\" instead\n"));
	   return nullptr;
#endif
	   try {
	      inputDevice	= new sdrplayHandler (dabSettings, version);
	      showButtons();
	   }
	   catch (const std::exception &e) {
	      QMessageBox::warning (this, tr ("Warning"), tr (e. what ()));
	      return nullptr;
	   }
	   catch (...) {
	      QMessageBox::warning (this, tr ("Warning"), tr ("sdrplay-v2 fails"));
	      return nullptr;
	   }
	}
	else
#endif
#ifdef  HAVE_SDRPLAY_V3
  if (s == "sdrplay")
  {
    try
    {
      inputDevice = new sdrplayHandler_v3(dabSettings, version);
      showButtons();
    }
    catch (const std::exception & e)
    {
      QMessageBox::warning(this, tr("Warning"), tr(e.what()));
      return nullptr;
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("sdrplay v3 fails"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_RTLSDR
  if (s == "dabstick")
  {
    try
    {
      inputDevice = new rtlsdrHandler(dabSettings, version);
      showButtons();
    }
    catch (const std::exception & ex)
    {
      QMessageBox::warning(this, tr("Warning"), tr(ex.what()));
      return nullptr;
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("rtlsdr device fails"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_AIRSPY
  if (s == "airspy") {
	   try {
	      inputDevice	= new airspyHandler (dabSettings, version);
	      showButtons();
	   }
	   catch (const std::exception &e) {
	      QMessageBox::warning (this, tr ("Warning"), tr (e. what ()));
	      return nullptr;
	   }
	   catch (...) {
	      QMessageBox::warning (this, tr ("Warning"), tr ("airspy device fails"));
	      return nullptr;
	   }
	}
	else
#endif
#ifdef  HAVE_HACKRF
  if (s == "hackrf")
  {
    try
    {
      inputDevice = new hackrfHandler(dabSettings, version);
      showButtons();
    }
    catch (const std::exception & e)
    {
      QMessageBox::warning(this, tr("Warning"), tr(e.what()));
      return nullptr;
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("hackrf device fails"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_LIME
     if (s == "limeSDR") {
	   try {
	      inputDevice = new limeHandler (dabSettings, version);
	      showButtons();
	   }
	   catch (const std::exception &e) {
	      QMessageBox::warning (this, tr ("Warning"), tr (e. what ()));
	      return nullptr;
	   }
	   catch (...) {
	      QMessageBox::warning (this, tr ("Warning"), tr ("lime device fails"));
	      return nullptr;
	   }
	}
	else
#endif
#ifdef  HAVE_PLUTO_2
   if (s == "pluto") {
	   try {
	      inputDevice = new plutoHandler (dabSettings, version);
	      showButtons();
	   }
	   catch (const std::exception &e) {
	      QMessageBox::warning (this, tr ("Warning"), tr (e. what ()));
	      return nullptr;
	   }
	   catch (...) {
	      QMessageBox::warning (this, tr ("Warning"), tr ("pluto device fails"));
	      return nullptr;
	   }
	}
	else
#endif
#ifdef  HAVE_PLUTO_RXTX
   if (s == "pluto-rxtx") {
	   try {
	      inputDevice = new plutoHandler (dabSettings,
	                                      version, fmFrequency);
	      showButtons();
	      streamerOut = new dabStreamer (48000, 192000,
	                                       (plutoHandler *)inputDevice);
	      ((plutoHandler *)inputDevice)	-> startTransmitter (
	                                               fmFrequency);
	   }
	   catch (const std::exception &e) {
	      QMessageBox::warning (this, tr ("Warning"), tr (e. what ()));
	      return nullptr;
	   }
	   catch (...) {
	      QMessageBox::warning (this, tr ("Warning"), tr ("pluto device fails"));
	      return nullptr;
	   }
	}
	else
#endif
#ifdef HAVE_RTL_TCP
   //	RTL_TCP might be working.
	if (s == "rtl_tcp") {
	   try {
	      inputDevice = new rtl_tcp_client (dabSettings);
	      showButtons();
	   }
	   catch (...) {
	      QMessageBox::warning (this, tr ("Warning"), tr ("rtl_tcp: no luck\n") );
	      return nullptr;
	   }
	}
	else
#endif
#ifdef  HAVE_ELAD
    if (s == "elad-s1") {
	   try {
	      inputDevice = new eladHandler (dabSettings);
	      showButtons();
	   }
	   catch (...) {
	      QMessageBox::warning (this, tr ("Warning"), tr ("no elad device found\n"));
	      return nullptr;
	   }
	}
	else
#endif
#ifdef  HAVE_SOAPY
    if (s == "soapy") {
	   try {
	      inputDevice	= new soapyHandler (dabSettings);
	      showButtons();
	   }
	   catch (...) {
	      QMessageBox::warning (this, tr ("Warning"), tr ("no soapy device found\n"));
	      return nullptr;
	   }
	}
	else
#endif
#ifdef HAVE_EXTIO
    //	extio is - in its current settings - for Windows, it is a
//	wrap around the dll
	if (s == "extio") {
	   try {
	      inputDevice = new extioHandler (dabSettings);
	      showButtons();
	   }
	   catch (...) {
	      QMessageBox::warning (this, tr ("Warning"), tr ("extio: no luck\n") );
	      return nullptr;
	   }
	}
	else
#endif
  if (s == "xml files")
  {
    file = QFileDialog::getOpenFileName(this, tr("Open file ..."), QDir::homePath(), tr("xml data (*.*)"));
    if (file == QString(""))
    {
      return nullptr;
    }
    file = QDir::toNativeSeparators(file);
    try
    {
      inputDevice = new xml_fileReader(file);
      channel.realChannel = false;
      hideButtons();
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("file not found"));
      return nullptr;
    }
  }
  else if ((s == "file input(.iq)") || (s == "file input(.raw)"))
  {
    const char * p;
    if (s == "file input(.iq)")
    {
      p = "iq data (*iq)";
    }
    else
    {
      p = "raw data (*raw)";
    }

    file = QFileDialog::getOpenFileName(this, tr("Open file ..."), QDir::homePath(), tr(p));
    if (file == QString(""))
    {
      return nullptr;
    }

    file = QDir::toNativeSeparators(file);
    try
    {
      inputDevice = new rawFiles(file);
      hideButtons();
      channel.realChannel = false;
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("file not found"));
      return nullptr;
    }
  }
  else if (s == "file input(.sdr)")
  {
    file = QFileDialog::getOpenFileName(this, tr("Open file ..."), QDir::homePath(), tr("raw data (*.sdr)"));
    if (file == QString(""))
    {
      return nullptr;
    }

    file = QDir::toNativeSeparators(file);
    try
    {
      inputDevice = new wavFiles(file);
      channel.realChannel = false;
      hideButtons();
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("file not found"));
      return nullptr;
    }
  }
  else
  {
    fprintf(stderr, "unknown device, failing\n");
    return nullptr;
  }

  //
  //	It took some code, but it seems we have a device
  my_spectrumViewer.setBitDepth(inputDevice->bitDepth());

  dabSettings->setValue("device", s);
  //	do we want to see the widget for device control?
  if (dabSettings->value("deviceVisible", 0).toInt())
  {
    inputDevice->show();
  }
  else
  {
    inputDevice->hide();
  }

  return inputDevice;
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
  if (inputDevice != nullptr)
  {
    delete inputDevice;
    fprintf(stderr, "device is deleted\n");
    inputDevice = nullptr;
  }

  LOG("selecting ", deviceName);
  inputDevice = setDevice(deviceName);
  if (inputDevice == nullptr)
  {
    inputDevice = new deviceHandler();
    return;    // nothing will happen
  }

  if (dabSettings->value("deviceVisible", 1).toInt() != 0)
  {
    inputDevice->show();
  }
  else
  {
    inputDevice->hide();
  }
  doStart();    // will set running
}

void RadioInterface::_slot_handle_device_widget_button()
{
  if (inputDevice == nullptr)
  {
    return;
  }

  if (inputDevice->isHidden())
  {
    inputDevice->show();
  }
  else
  {
    inputDevice->hide();
  }

  dabSettings->setValue("deviceVisible", inputDevice->isHidden() ? 0 : 1);
}

///////////////////////////////////////////////////////////////////////////
//	signals, received from ofdm_decoder for which that data is
//	to be displayed
///////////////////////////////////////////////////////////////////////////

//	called from the fibDecoder
void RadioInterface::slot_clock_time(int year, int month, int day, int hours, int minutes, int d2, int h2, int m2, int seconds)
{
  this->localTime.year = year;
  this->localTime.month = month;
  this->localTime.day = day;
  this->localTime.hour = hours;
  this->localTime.minute = minutes;
  this->localTime.second = seconds;

#ifdef  CLOCK_STREAMER
                                                                                                                          uint8_t	localBuffer [10];
	localBuffer [0] = 0xFF;
	localBuffer [1] = 0x00;
	localBuffer [2] = 0xFF;
	localBuffer [3] = 0x00;
	localBuffer [4] = (year & 0xFF00) >> 8;
	localBuffer [5] = year & 0xFF;
	localBuffer [6] = month;
	localBuffer [7] = day;
	localBuffer [8] = minutes;
	localBuffer [9] = seconds;
	if (running. load())
	   clockStreamer -> sendData (localBuffer, 10);
#endif
  this->UTC.year = year;
  this->UTC.month = month;
  this->UTC.day = d2;
  this->UTC.hour = h2;
  this->UTC.minute = m2;
  QString result;
  if (configWidget.utcSelector->isChecked())
  {
    result = convertTime(year, month, day, h2, m2);
  }
  else
  {
    result = convertTime(year, month, day, hours, minutes);
  }
  localTimeDisplay->setText(result);
}

QString RadioInterface::convertTime(int year, int month, int day, int hours, int minutes)
{
  return QString::number(year).rightJustified(4, '0') + "-" +
         QString::number(month).rightJustified(2, '0') + "-" +
         QString::number(day).rightJustified(2, '0') + "  " +
         QString::number(hours).rightJustified(2, '0') + ":" +
         QString::number(minutes).rightJustified(2, '0');
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
    QPalette p = configWidget.ficError_display->palette();
    if (ficSuccess < 85)
    {
      p.setColor(QPalette::Highlight, Qt::red);
    }
    else
    {
      p.setColor(QPalette::Highlight, Qt::green);
    }

    configWidget.ficError_display->setPalette(p);
    configWidget.ficError_display->setValue(ficSuccess);
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

//	called from the dabProcessor
void RadioInterface::slot_show_snr(int s)
{
  if (!running.load())
  {
    return;
  }

  channel.snr = s;
  if (!my_spectrumViewer.isHidden())
  {
    my_spectrumViewer.show_snr(s);
  }

  if (my_snrViewer.isHidden())
  {
    snrBuffer.FlushRingBuffer();
    return;
  }

  int amount = snrBuffer.GetRingBufferReadAvailable();
  if (amount <= 0)
  {
    return;
  }

  float ss[amount];
  snrBuffer.getDataFromBuffer(ss, amount);
  for (int i = 0; i < amount; i++)
  {
    my_snrViewer.add_snr(ss[i]);
  }
  my_snrViewer.show_snr();
}

//	just switch a color, called from the dabprocessor
void RadioInterface::slot_set_synced(bool b)
{
  (void)b;
}
//
//	called from the PAD handler

void RadioInterface::slot_show_label(const QString & s)
{
#ifdef  HAVE_PLUTO_RXTX
                                                                                                                          if (streamerOut != nullptr)
	   streamerOut -> addRds (std::string (s. toUtf8 (). data ()));
#endif
  if (running.load())
  {
    dynamicLabel->setWordWrap(true);
    dynamicLabel->setText(s);
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
          localTime.year,
          localTime.month,
          localTime.day,
          localTime.hour,
          localTime.minute,
          localTime.second,
          s.toUtf8().data());
}

void RadioInterface::slot_set_stereo(bool b)
{
  if (!running.load())
  {
    return;
  }
  if (stereoSetting == b)
  {
    return;
  }
  stereoLabel->setStyleSheet(b ? "color:#FF4444" : "color:#AAAAAA");
  stereoLabel->setText(b ? "<b>STEREO</b>" : "<b>MONO</b>");
  stereoSetting = b;
}

static QString tiiNumber(int n)
{
  if (n >= 10)
  {
    return QString::number(n);
  }
  return QString("0") + QString::number(n);
}

void RadioInterface::slot_show_tii_spectrum()
{
  my_tiiViewer.showSpectrum(1);
}

void RadioInterface::slot_show_tii(int mainId, int subId)
{
  QString a = "Est: ";
  bool found = false;
  QString country = "";
  bool tiiChange = false;

  if (mainId == 0xFF)
  {
    return;
  }

  for (int i = 0; i < (int)(channel.transmitters.size()); i += 2)
  {
    if ((channel.transmitters.at(i) == (mainId & 0x7F)) && (channel.transmitters.at(i + 1) == subId))
    {
      found = true;
      break;
    }
  }

  if (!found)
  {
    channel.transmitters.append(mainId & 0x7F);
    channel.transmitters.append(subId);
  }

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

  a = a + " " + tiiNumber(mainId) + " " + tiiNumber(subId);
  transmitter_coordinates->setAlignment(Qt::AlignRight);
  transmitter_coordinates->setText(a);
  my_tiiViewer.showTransmitters(channel.transmitters);

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

  QString theName = tiiHandler.get_transmitter_name(channel.realChannel ? channel.channelName : "any",
    //	                                            channel. countryName,
                                                     channel.Eid, mainId, subId);
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
  distanceLabel->setText(labelText);

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
  //
  //	to be certain, we check
  if (channel.targetPos == cmplx(0, 0) || (distance == 0) || (corner == 0))
  {
    return;
  }

  QDateTime theTime = configWidget.utcSelector->isChecked() ? QDateTime::currentDateTimeUtc() : QDateTime::currentDateTime();

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

  my_spectrumViewer.showSpectrum(amount, inputDevice->getVFOFrequency());
}

void RadioInterface::slot_show_iq(int iAmount, float iAvg)
{
  if (!running.load())
  {
    return;
  }

  my_spectrumViewer.showIQ(iAmount, iAvg);
}

void RadioInterface::slot_show_mod_quality_data(const OfdmDecoder::SQualityData * pQD)
{
  if (!running.load())
  {
    return;
  }

  if (!my_spectrumViewer.isHidden())
  {
    my_spectrumViewer.showQuality(pQD->CurOfdmSymbolNo, pQD->StdDeviation, pQD->TimeOffset, pQD->FreqOffset, pQD->PhaseCorr);
  }
}

//
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
void RadioInterface::slot_show_clock_error(int e)
{
  if (!running.load())
  {
    return;
  }
  if (!my_spectrumViewer.isHidden())
  {
    my_spectrumViewer.show_clockErr(e);
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

  my_spectrumViewer.showCorrelation(amount, marker, v);
  channel.nrTransmitters = v.size();
}

////////////////////////////////////////////////////////////////////////////

void RadioInterface::slot_set_stream_selector(int k)
{
  if (!running.load())
  {
    return;
  }
#if  not defined (TCP_STREAMER) && not defined (QT_AUDIO)
  ((AudioSink *)(soundOut))->selectDevice(k);
  dabSettings->setValue("soundchannel", configWidget.streamoutSelector->currentText());
#else
  (void)k;
#endif
}

void RadioInterface::slot_switch_visibility(QWidget * w)
{
  if (w->isHidden())
  {
    w->show();
  }
  else
  {
    w->hide();
  }
}

void RadioInterface::_slot_handle_detail_button()
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

//
//	Whenever the input device is a file, some functions,
//	e.g. selecting a channel, setting an alarm, are not
//	meaningful
void RadioInterface::showButtons()
{
  configWidget.dumpButton->show();
  configWidget.frequencyDisplay->show();
  scanButton->show();
  channelSelector->show();
  nextChannelButton->show();
  prevChannelButton->show();
  presetSelector->show();
}

void RadioInterface::hideButtons()
{
  configWidget.dumpButton->hide();
  configWidget.frequencyDisplay->hide();
  scanButton->hide();
  channelSelector->hide();
  nextChannelButton->hide();
  prevChannelButton->hide();
  presetSelector->hide();
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
//
////////////////////////////////////////////////////////////////////////
//
//	dump handling
//
/////////////////////////////////////////////////////////////////////////

void setButtonFont(QPushButton * b, QString text, int size)
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
  QString deviceName = inputDevice->deviceName();
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
  theTechWindow->audiodumpButton_text("audio dump", 10);
}

void RadioInterface::startAudiodumping()
{
  audioDumper = filenameFinder.findAudioDump_fileName(channel.currentService.serviceName, true);
  if (audioDumper == nullptr)
  {
    return;
  }

  LOG("audiodump starts ", serviceLabel->text());
  theTechWindow->audiodumpButton_text("writing", 12);
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
  theTechWindow->framedumpButton_text("frame dump", 10);
  channel.currentService.frameDumper = nullptr;
}

void RadioInterface::startFramedumping()
{
  channel.currentService.frameDumper = filenameFinder.findFrameDump_fileName(channel.currentService.serviceName, true);
  if (channel.currentService.frameDumper == nullptr)
  {
    return;
  }
  theTechWindow->framedumpButton_text("recording", 12);
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
    frameBuffer.FlushRingBuffer();
  }
  else
  {
    while (frameBuffer.GetRingBufferReadAvailable() >= amount)
    {
      frameBuffer.getDataFromBuffer(buffer, amount);
      if (channel.currentService.frameDumper != nullptr)
      {
        fwrite(buffer, amount, 1, channel.currentService.frameDumper);
      }
    }
  }
}

void RadioInterface::_slot_handle_tii_button()
{
  if (!running.load())
  {
    return;
  }

  if (my_tiiViewer.isHidden())
  {
    my_tiiViewer.show();
  }
  else
  {
    my_tiiViewer.hide();
  }
  dabSettings->setValue("tiiVisible", my_tiiViewer.isHidden() ? 0 : 1);
}

void RadioInterface::_slot_handle_spectrum_button()
{
  if (!running.load())
  {
    return;
  }

  if (my_spectrumViewer.isHidden())
  {
    my_spectrumViewer.show();
  }
  else
  {
    my_spectrumViewer.hide();
  }
  dabSettings->setValue("spectrumVisible", my_spectrumViewer.isHidden() ? 0 : 1);
}

void RadioInterface::_slot_handle_snr_button()
{
  if (!running.load())
  {
    return;
  }

  if (my_snrViewer.isHidden())
  {
    my_snrViewer.show();
  }
  else
  {
    my_snrViewer.hide();
  }
  dabSettings->setValue("snrVisible", my_snrViewer.isHidden() ? 0 : 1);
}

void RadioInterface::_slot_handle_history_button()
{
  if (!running.load())
  {
    return;
  }

  if (my_history->isHidden())
  {
    my_history->show();
  }
  else
  {
    my_history->hide();
  }
}

//
//	When changing (or setting) a device, we do not want anybody
//	to have the buttons on the GUI touched, so
//	we just disconnet them and (re)connect them as soon as
//	a device is operational
void RadioInterface::connectGUI()
{
  connect(configWidget.contentButton, SIGNAL (clicked()), this, SLOT (_slot_handle_content_button()));
  connect(detailButton, SIGNAL (clicked()), this, SLOT (_slot_handle_detail_button()));
  connect(configWidget.resetButton, SIGNAL (clicked()), this, SLOT (_slot_handle_reset_button()));
  connect(scanButton, SIGNAL (clicked()), this, SLOT (_slot_handle_scan_button()));
  connect(configWidget.show_tiiButton, SIGNAL (clicked()), this, SLOT (_slot_handle_tii_button()));
  connect(configWidget.show_spectrumButton, SIGNAL (clicked()), this, SLOT (_slot_handle_spectrum_button()));
  connect(configWidget.snrButton, SIGNAL (clicked()), this, SLOT (_slot_handle_snr_button()));
  connect(configWidget.devicewidgetButton, SIGNAL (clicked()), this, SLOT (_slot_handle_device_widget_button()));
  connect(historyButton, SIGNAL (clicked()), this, SLOT (_slot_handle_history_button()));
  connect(configWidget.dumpButton, SIGNAL (clicked()), this, SLOT (_slot_handle_source_dump_button()));
  connect(nextChannelButton, SIGNAL (clicked()), this, SLOT (_slot_handle_next_channel_button()));
  connect(prevChannelButton, SIGNAL (clicked()), this, SLOT (_slot_handle_prev_channel_button()));
  connect(prevServiceButton, SIGNAL (clicked()), this, SLOT (_slot_handle_prev_service_button()));
  connect(nextServiceButton, SIGNAL (clicked()), this, SLOT (_slot_handle_next_service_button()));
  connect(theTechWindow, SIGNAL (handle_audioDumping()), this, SLOT (_slot_handle_audio_dump_button()));
  connect(theTechWindow, SIGNAL (handle_frameDumping()), this, SLOT (_slot_handle_frame_dump_button()));
  connect(muteButton, SIGNAL (clicked()), this, SLOT (_slot_handle_mute_button()));
  connect(ensembleDisplay, SIGNAL (clicked(QModelIndex)), this, SLOT (_slot_select_service(QModelIndex)));
  connect(configWidget.switchDelaySetting, SIGNAL (valueChanged(int)), this, SLOT (_slot_handle_switch_delay_setting(int)));
  connect(configWidget.orderAlfabetical, SIGNAL (clicked()), this, SLOT (_slot_handle_order_alfabetical()));
  connect(configWidget.orderServiceIds, SIGNAL (clicked()), this, SLOT (_slot_handle_order_service_ids()));
  connect(configWidget.ordersubChannelIds, SIGNAL (clicked()), this, SLOT (_slot_handle_order_sub_channel_ids()));
  connect(configWidget.scanmodeSelector, SIGNAL (currentIndexChanged(int)), this, SLOT (_slot_handle_scan_mode_selector(int)));
  connect(configWidget.saveServiceSelector, SIGNAL (stateChanged(int)), this, SLOT (_slot_handle_save_service_selector(int)));
  connect(configWidget.skipList_button, SIGNAL (clicked()), this, SLOT (_slot_handle_skip_list_button()));
  connect(configWidget.skipFile_button, SIGNAL (clicked()), this, SLOT (_slot_handle_skip_file_button()));
}

void RadioInterface::disconnectGUI()
{
  disconnect(configWidget.contentButton, SIGNAL (clicked()), this, SLOT (_slot_handle_content_button()));
  disconnect(detailButton, SIGNAL (clicked()), this, SLOT (_slot_handle_detail_button()));
  disconnect(configWidget.resetButton, SIGNAL (clicked()), this, SLOT (_slot_handle_reset_button()));
  disconnect(scanButton, SIGNAL (clicked()), this, SLOT (_slot_handle_scan_button()));
  disconnect(configWidget.show_tiiButton, SIGNAL (clicked()), this, SLOT (_slot_handle_tii_button()));
  disconnect(configWidget.show_spectrumButton, SIGNAL (clicked()), this, SLOT (_slot_handle_spectrum_button()));
  disconnect(configWidget.snrButton, SIGNAL (clicked()), this, SLOT (_slot_handle_snr_button()));
  disconnect(configWidget.devicewidgetButton, SIGNAL (clicked()), this, SLOT (_slot_handle_device_widget_button()));
  disconnect(historyButton, SIGNAL (clicked()), this, SLOT (_slot_handle_history_button()));
  disconnect(configWidget.dumpButton, SIGNAL (clicked()), this, SLOT (_slot_handle_source_dump_button()));
  disconnect(nextChannelButton, SIGNAL (clicked()), this, SLOT (_slot_handle_next_channel_button()));
  disconnect(prevChannelButton, SIGNAL (clicked()), this, SLOT (_slot_handle_prev_channel_button()));
  disconnect(prevServiceButton, SIGNAL (clicked()), this, SLOT (_slot_handle_prev_service_button()));
  disconnect(nextServiceButton, SIGNAL (clicked()), this, SLOT (_slot_handle_next_service_button()));
  disconnect(theTechWindow, SIGNAL (handle_audioDumping()), this, SLOT (_slot_handle_audio_dump_button()));
  disconnect(theTechWindow, SIGNAL (handle_frameDumping()), this, SLOT (_slot_handle_frame_dump_button()));
  disconnect(muteButton, SIGNAL (clicked()), this, SLOT (_slot_handle_mute_button()));
  disconnect(ensembleDisplay, SIGNAL (clicked(QModelIndex)), this, SLOT (_slot_select_service(QModelIndex)));
  disconnect(configWidget.switchDelaySetting, SIGNAL (valueChanged(int)), this, SLOT (_slot_handle_switch_delay_setting(int)));
  disconnect(configWidget.orderAlfabetical, SIGNAL (clicked()), this, SLOT (_slot_handle_order_alfabetical()));
  disconnect(configWidget.orderServiceIds, SIGNAL (clicked()), this, SLOT (_slot_handle_order_service_ids()));
  disconnect(configWidget.ordersubChannelIds, SIGNAL (clicked()), this, SLOT (_slot_handle_order_sub_channel_ids()));
  disconnect(configWidget.scanmodeSelector, SIGNAL (currentIndexChanged(int)), this, SLOT (_slot_handle_scan_mode_selector(int)));
  disconnect(configWidget.saveServiceSelector, SIGNAL (stateChanged(int)), this, SLOT (_slot_handle_save_service_selector(int)));
  disconnect(configWidget.skipList_button, SIGNAL (clicked()), this, SLOT (_slot_handle_skip_list_button()));
  disconnect(configWidget.skipFile_button, SIGNAL (clicked()), this, SLOT (_slot_handle_skip_file_button()));
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
    QKeyEvent * ke = static_cast <QKeyEvent *> (event);
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
  else if ((obj == this->my_history->viewport()) && (event->type() == QEvent::MouseButtonPress))
  {
    QMouseEvent * ev = static_cast<QMouseEvent *>(event);
    if (ev->buttons() & Qt::RightButton)
    {
      my_history->clearHistory();
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
        presetData pd;
        pd.serviceName = serviceName;
        pd.channel = channelSelector->currentText();
        QString itemText = pd.channel + ":" + pd.serviceName;
        for (int i = 0; i < presetSelector->count(); i++)
        {
          if (presetSelector->itemText(i) == itemText)
          {
            return true;
          }
        }
        presetSelector->addItem(itemText);
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

        dabService s;
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

void RadioInterface::colorServiceName(const QString & serviceName, QColor color, int fS, bool italic)
{
  for (int j = 0; j < model.rowCount(); j++)
  {
    QString itemText = model.index(j, 0).data(Qt::DisplayRole).toString();
    if (itemText == serviceName)
    {
      colorService(model.index(j, 0), color, fS, italic);
      return;
    }
  }
}

void RadioInterface::slot_start_announcement(const QString & name, int subChId)
{
  if (!running.load())
  {
    return;
  }

  if (name == serviceLabel->text())
  {
    serviceLabel->setStyleSheet("QLabel {color : red}");
    fprintf(stdout, "announcement for %s (%d) starts\n", name.toUtf8().data(), subChId);
  }
}

void RadioInterface::slot_stop_announcement(const QString & name, int subChId)
{
  (void)subChId;
  if (!running.load())
  {
    return;
  }

  if (name == serviceLabel->text())
  {
    serviceLabel->setStyleSheet("QLabel {color : blue}");
    fprintf(stdout, "end for announcement service %s\n", name.toUtf8().data());
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

//
//	selecting from the preset list
void RadioInterface::slot_handle_preset_selector(const QString & s)
{
  if ((s == "Presets") || (presetSelector->currentIndex() == 0))
  {
    return;
  }
  localSelect(s);
}

//
//	selecting from the history list, which is essential
//	the same as handling form the preset list
void RadioInterface::_slot_handle_history_select(const QString & s)
{
  localSelect(s);
}

//
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
  QStringList list = s.split (":", QString::SkipEmptyParts);
#endif
  if (list.length() != 2)
  {
    return;
  }
  localSelect(list.at(0), list.at(1));
}

void RadioInterface::localSelect(const QString & theChannel, const QString & service)
{
  int switchDelay;
  QString serviceName = service;

  if (my_dabProcessor == nullptr)
  {  // should not happen
    return;
  }

  stopService(channel.currentService);

  for (int i = service.size(); i < 16; i++)
  {
    serviceName.push_back(' ');
  }

  if (theChannel == channel.channelName)
  {
    channel.currentService.valid = false;
    dabService s;
    my_dabProcessor->getParameters(serviceName, &s.SId, &s.SCIds);
    if (s.SId == 0)
    {
      write_warning_message("insufficient data for this program (1)");
      return;
    }
    s.serviceName = service;
    startService(s);
    return;
  }
  //
  //	The hard part is stopping the current service,
  //	quitting the current channel,
  //      selecting a new channel,
  //      waiting a while
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
  //
  //	prepare the service, start the new channel and wait
  channel.nextService.valid = true;
  channel.nextService.channel = theChannel;
  channel.nextService.serviceName = serviceName;
  channel.nextService.SId = 0;
  channel.nextService.SCIds = 0;
  presetTimer.setSingleShot(true);
  switchDelay = dabSettings->value("switchDelay", 8).toInt();
  presetTimer.setInterval(switchDelay * 1000);
  presetTimer.start(switchDelay * 1000);
  startChannel(channelSelector->currentText());
}

////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////

void RadioInterface::stopService(dabService & s)
{
  presetTimer.stop();
  channelTimer.stop();

  // reset muting if active
  if (mutingActive)
  {
    _slot_handle_mute_button();
  }

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
    for (int i = 0; i < model.rowCount(); i++)
    {
      QString itemText = model.index(i, 0).data(Qt::DisplayRole).toString();
      if (itemText != s.serviceName)
      {
        continue;
      }
      for (uint16_t j = 0; j < channel.backgroundServices.size(); j++)
      {
        if (channel.backgroundServices.at(j).serviceName == s.serviceName)
        {
          colorService(model.index(i, 0), Qt::blue, fontSize + 2, true);
          cleanScreen();
          return;
        }
      }  // ok, service is not background as well
      colorService(model.index(i, 0), Qt::black, fontSize);
      break;
    }
  }
  show_pause_slide();
  cleanScreen();
}

//
//
void RadioInterface::startService(dabService & s)
{
  QString serviceName = s.serviceName;

  channel.currentService = s;
  channel.currentService.frameDumper = nullptr;
  channel.currentService.valid = false;
  LOG("start service ", serviceName.toUtf8().data());
  LOG("service has SNR ", QString::number(channel.snr));
  //
  //	mark the selected service in the service list
  int rowCount = model.rowCount();
  (void)rowCount;
  colorServiceName(serviceName, Qt::red, 16, true);
  //
  //	and display the servicename on the serviceLabel
  QFont font = serviceLabel->font();
  font.setPointSize(20);
  font.setBold(true);
  serviceLabel->setStyleSheet("color: lightblue");
  serviceLabel->setFont(font);
  serviceLabel->setText(serviceName);
  dynamicLabel->setText("");

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
    write_warning_message("insufficient data for this program (2)");
    dabSettings->setValue("presetname", "");
  }
}

void RadioInterface::colorService(QModelIndex ind, QColor c, int pt, bool italic)
{
  QMap<int, QVariant> vMap = model.itemData(ind);
  vMap.insert(Qt::ForegroundRole, QVariant(QBrush(c)));
  model.setItemData(ind, vMap);
  model.setData(ind, QFont(theFont, pt, -1, italic), Qt::FontRole);
}

//
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
}

void RadioInterface::startPacketservice(const QString & s)
{
  Packetdata pd;

  my_dabProcessor->dataforPacketService(s, &pd, 0);
  if ((!pd.defined) || (pd.DSCTy == 0) || (pd.bitRate == 0))
  {
    write_warning_message("insufficient data for this program (3)");
    return;
  }

  if (!my_dabProcessor->set_dataChannel(&pd, &dataBuffer, FORE_GROUND))
  {
    QMessageBox::warning(this, tr("sdr"), tr("could not start this service\n"));
    return;
  }

  switch (pd.DSCTy)
  {
  default: slot_show_label(QString("unimplemented Data"));
    break;
  case 5: fprintf(stdout, "selected apptype %d\n", pd.appType);
    slot_show_label(QString("Transp. Channel not implemented"));
    break;
  case 60: slot_show_label(QString("MOT partially implemented"));
    break;
  case 59:
  {
#ifdef  _SEND_DATAGRAM_
                                                                                                                            QString text = QString ("Embedded IP: UDP data to ");
	      text. append (ipAddress);
	      text. append (" ");
	      QString n = QString::number (port);
	      text. append (n);
	      showLabel (text);
#else
    slot_show_label("Embedded IP not supported ");
#endif
  }
    break;
  case 44: slot_show_label(QString("Journaline"));
    break;
  }
}

//	This function is only used in the Gui to clear
//	the details of a selected service
void RadioInterface::cleanScreen()
{
  serviceLabel->setText("");
  dynamicLabel->setStyleSheet("color: white");
  dynamicLabel->setText("");
  theTechWindow->cleanUp();
  stereoLabel->setText("");
  programTypeLabel->setText("");

  new_presetIndex(0);
  stereoSetting = false;
  theTechWindow->cleanUp();
  slot_set_stereo(false);
}

////////////////////////////////////////////////////////////////////////////
//
//	next and previous service selection
////////////////////////////////////////////////////////////////////////////

void RadioInterface::_slot_handle_prev_service_button()
{
  disconnect(prevServiceButton, SIGNAL (clicked()), this, SLOT (_slot_handle_prev_service_button()));
  handle_serviceButton(BACKWARDS);
  connect(prevServiceButton, SIGNAL (clicked()), this, SLOT (_slot_handle_prev_service_button()));
}

void RadioInterface::_slot_handle_next_service_button()
{
  disconnect(nextServiceButton, SIGNAL (clicked()), this, SLOT (_slot_handle_next_service_button()));
  handle_serviceButton(FORWARD);
  connect(nextServiceButton, SIGNAL (clicked()), this, SLOT (_slot_handle_next_service_button()));
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
        colorService(model.index(i, 0), Qt::black, fontSize);
        if (d == FORWARD)
        {
          i = (i + 1) % serviceList.size();
        }
        else
        {
          i = (i - 1 + serviceList.size()) % serviceList.size();
        }
        dabService s;
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
    write_warning_message("ensemble not yet recognized");
    return;
  }

  QString presetName = channel.nextService.serviceName;
  for (int i = presetName.length(); i < 16; i++)
  {
    presetName.push_back(' ');
  }

  dabService s;
  s.serviceName = presetName;
  my_dabProcessor->getParameters(presetName, &s.SId, &s.SCIds);
  if (s.SId == 0)
  {
    write_warning_message("insufficient data for this program (5)");
    return;
  }

  channel.nextService.valid = false;
  startService(s);
}

void RadioInterface::write_warning_message(const QString & iMsg)
{
  //QMessageBox::warning(this, "Warning", iMsg);
  dynamicLabel->setStyleSheet("color: red");
  dynamicLabel->setText(iMsg);
}

///////////////////////////////////////////////////////////////////////////
//
//	Channel basics
///////////////////////////////////////////////////////////////////////////
//	Precondition: no channel should be active
//
void RadioInterface::startChannel(const QString & theChannel)
{
  int tunedFrequency = theBand.Frequency(theChannel);
  LOG("channel starts ", theChannel);
  configWidget.frequencyDisplay->display(tunedFrequency / 1000000.0);
  my_spectrumViewer.showFrequency(tunedFrequency / 1000000.0);
  dabSettings->setValue("channel", theChannel);
  inputDevice->resetBuffer();
  serviceList.clear();
  model.clear();
  ensembleDisplay->setModel(&model);
  inputDevice->restartReader(tunedFrequency);
  channel.cleanChannel();
  channel.channelName = theChannel;
  dabSettings->setValue("channel", theChannel);
  channel.frequency = tunedFrequency / 1000;
  my_tiiViewer.clear();
  if (transmitterTags_local && (mapHandler != nullptr))
  {
    mapHandler->putData(MAP_RESET, cmplx(0, 0), "", "", "", 0, 0, 0, 0);
  }
  else if (mapHandler != nullptr)
  {
    mapHandler->putData(MAP_FRAME, cmplx(-1, -1), "", "", "", 0, 0, 0, 0);
  }
  show_for_safety();
  my_dabProcessor->start();
  int switchDelay = dabSettings->value("switchDelay", 8).toInt();
  if (!scanning.load())
  {
    epgTimer.start(switchDelay * 1000);
  }
}

//
//	apart from stopping the reader, a lot of administration
//	is to be done.
void RadioInterface::stopChannel()
{
  if (inputDevice == nullptr)
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
    dabService s = channel.backgroundServices.at(i);
    my_dabProcessor->stop_service(s.subChId, BACK_GROUND);
    if (s.fd != nullptr)
    {
      fclose(s.fd);
    }
  }
  channel.backgroundServices.clear();

  stopSourcedumping();
  soundOut->stop();
  //
  configWidget.EPGLabel->hide();
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
  inputDevice->stopReader();
  my_dabProcessor->stop();
  usleep(1000);
  theTechWindow->cleanUp();

  show_pause_slide();
  presetTimer.stop();
  channelTimer.stop();
  channel.cleanChannel();
  channel.targetPos = cmplx(0, 0);
  if (transmitterTags_local && (mapHandler != nullptr))
  {
    mapHandler->putData(MAP_RESET, channel.targetPos, "", "", "", 0, 0, 0, 0);
  }
  transmitter_country->setText("");
  transmitter_coordinates->setText("");
  //
  hide_for_safety();  // hide some buttons
  my_tiiViewer.clear();
  QCoreApplication::processEvents();
  //
  //	no processing left at this time
  usleep(1000);    // may be handling pending signals?
  channel.currentService.valid = false;
  channel.nextService.valid = false;

  //	all stopped, now look at the GUI elements
  configWidget.ficError_display->setValue(0);
  //	the visual elements related to service and channel
  slot_set_synced(false);
  ensembleId->setText("");
  transmitter_coordinates->setText(" ");

  serviceList.clear();
  model.clear();
  ensembleDisplay->setModel(&model);
  cleanScreen();
  configWidget.EPGLabel->hide();
  distanceLabel->setText("");
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
  presetSelector->setCurrentIndex(0);
  stopScanning(false);
  stopChannel();
  startChannel(channel);
}

void RadioInterface::_slot_handle_next_channel_button()
{
  int nrChannels = channelSelector->count();
  int newChannel = channelSelector->currentIndex() + 1;
  set_channelButton(newChannel % nrChannels);
}

void RadioInterface::_slot_handle_prev_channel_button()
{
  int nrChannels = channelSelector->count();
  if (channelSelector->currentIndex() == 0)
  {
    set_channelButton(nrChannels - 1);
  }
  else
  {
    set_channelButton(channelSelector->currentIndex() - 1);
  }
}

void RadioInterface::set_channelButton(int currentChannel)
{
  if (!running.load())
  {
    return;
  }

  presetTimer.stop();
  if (my_dabProcessor == nullptr)
  {
    fprintf(stderr, "Expert error 23\n");
    abort();
  }

  stopScanning(false);
  stopChannel();
  new_channelIndex(currentChannel);
  startChannel(channelSelector->currentText());
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
  int switchDelay;
  int scanMode = configWidget.scanmodeSelector->currentIndex();
  presetTimer.stop();
  channelTimer.stop();
  epgTimer.stop();
  connect(my_dabProcessor, SIGNAL (slot_no_signal_found()), this, SLOT (slot_no_signal_found()));
  new_presetIndex(0);
  stopChannel();
  int cc = channelSelector->currentIndex();
  if (scanMode == SCAN_TO_DATA)
  {
    cc++;
    if (cc >= channelSelector->count())
    {
      cc = 0;
    }
  }
  else
  {
    cc = theBand.firstChannel();
  }

  LOG("scanning starts with ", QString::number(cc));
  scanning.store(true);
  if ((scanMode == SINGLE_SCAN) || (scanMode == SCAN_CONTINUOUSLY))
  {
    if (my_scanTable == nullptr)
    {
      my_scanTable = new contentTable(this, dabSettings, "scan", my_dabProcessor->scanWidth());
    }
    else
    {
      my_scanTable->clearTable();
    }
    QString topLine = QString("ensemble") + ";" + "channelName" + ";" + "frequency (KHz)" + ";" + "Eid" + ";" + "time" + ";" + "tii" + ";" + "SNR" + ";" + "nr services" + ";";
    my_scanTable->addLine(topLine);
    my_scanTable->addLine("\n");
  }

  if (scanMode == SINGLE_SCAN)
  {
    scanDumpFile = filenameFinder.findScanDump_fileName();
  }
  else if (scanMode == SCAN_CONTINUOUSLY)
  {
    scanDumpFile = filenameFinder.findSummary_fileName();
  }
  else
  {
    scanDumpFile = nullptr;
  }

  my_dabProcessor->set_scanMode(true);
  //      To avoid reaction of the system on setting a different value:
  new_channelIndex(cc);
  dynamicLabel->setText("scan mode \"" + scanmodeText(scanMode) + "\" scanning channel " + channelSelector->currentText());
  scanButton->setText("scanning");
  switchDelay = dabSettings->value("switchDelay", 8).toInt();
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
  disconnect(my_dabProcessor, &DabProcessor::No_Signal_Found, this, &RadioInterface::slot_no_signal_found);
  (void)dump;
  if (scanning.load())
  {
    scanButton->setText("scan");
    LOG("scanning stops ", "");
    my_dabProcessor->set_scanMode(false);
    dynamicLabel->setText("Scan ended");
    channelTimer.stop();
    scanning.store(false);
    if (scanDumpFile != nullptr)
    {
      if (my_scanTable != nullptr)
      {
        my_scanTable->dump(scanDumpFile);
      }
      fclose(scanDumpFile);
      scanDumpFile = nullptr;
    }
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
  int switchDelay;
  int scanMode = configWidget.scanmodeSelector->currentIndex();

  disconnect(my_dabProcessor, SIGNAL (slot_no_signal_found()), this, SLOT (slot_no_signal_found()));
  channelTimer.stop();
  disconnect(&channelTimer, SIGNAL (timeout()), this, SLOT (_slot_channel_timeout()));

  if (running.load() && scanning.load())
  {
    int cc = channelSelector->currentIndex();
    if ((scanMode != SCAN_TO_DATA) && (serviceList.size() > 0))
    {
      showServices();
    }
    stopChannel();
    cc = theBand.nextChannel(cc);
    fprintf(stdout, "going to channel %d\n", cc);
    if ((cc >= channelSelector->count()) && (scanMode == SINGLE_SCAN))
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

      connect(my_dabProcessor, SIGNAL (slot_no_signal_found()), this, SLOT (slot_no_signal_found()));
      connect(&channelTimer, SIGNAL (timeout()), this, SLOT (_slot_channel_timeout()));

      dynamicLabel->setText("scan mode \"" + scanmodeText(scanMode) + "\" scanning channel " + channelSelector->currentText());
      switchDelay = dabSettings->value("switchDelay", 8).toInt();
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
  int scanMode = configWidget.scanmodeSelector->currentIndex();
  QString SNR = "SNR " + QString::number(channel.snr);

  if (my_dabProcessor == nullptr)
  {  // cannot happen
    fprintf(stderr, "Expert error 26\n");
    return;
  }

  QString utcTime = convertTime(UTC.year, UTC.month, UTC.day, UTC.hour, UTC.minute);
  if (scanMode == SINGLE_SCAN)
  {
    QString headLine = channel.ensembleName + ";" + channel.channelName + ";" + QString::number(channel.frequency) + ";" + hextoString(
      channel.Eid) + " " + ";" + transmitter_coordinates->text() + " " + ";" + utcTime + ";" + SNR + ";" + QString::number(serviceList.size()) + ";" + distanceLabel->text();
    QStringList s = my_dabProcessor->basicPrint();
    my_scanTable->addLine(headLine);
    my_scanTable->addLine("\n;\n");
    for (int i = 0; i < s.size(); i++)
    {
      my_scanTable->addLine(s.at(i));
    }
    my_scanTable->addLine("\n;\n;\n");
    my_scanTable->show();
  }
  else if (scanMode == SCAN_CONTINUOUSLY)
  {
    QString headLine = channel.ensembleName + ";" + channel.channelName + ";" + QString::number(channel.frequency) + ";" + hextoString(
      channel.Eid) + ";" + utcTime + ";" + transmitter_coordinates->text() + ";" + SNR + ";" + QString::number(serviceList.size()) + ";" + distanceLabel->text();

    //	   my_scanTable -> addLine ("\n");
    my_scanTable->addLine(headLine);
    my_scanTable->show();
  }
}

/////////////////////////////////////////////////////////////////////
//
bool RadioInterface::isMember(const std::vector<serviceId> & a, serviceId b)
{
  for (auto serv: a)
  {
    if (serv.name == b.name)
    {
      return true;
    }
  }
  return false;
}

std::vector<serviceId> RadioInterface::insert(const std::vector<serviceId> & l, serviceId n, int order)
{
  std::vector<serviceId> k;

  if (l.size() == 0)
  {
    k.push_back(n);
    return k;
  }
  uint32_t baseN = 0;
  uint16_t baseSubCh = 0;
  QString baseS = "";

  bool inserted = false;
  for (auto serv: l)
  {
    if (!inserted)
    {
      if (order == ID_BASED)
      {
        if ((baseN <= n.SId) && (n.SId <= serv.SId))
        {
          k.push_back(n);
          inserted = true;
        }
      }
      else if (order == SUBCH_BASED)
      {
        if ((baseSubCh <= n.subChId) && (n.subChId <= serv.subChId))
        {
          k.push_back(n);
          inserted = true;
        }
      }
      else
      {
        if ((baseS < n.name) && (n.name < serv.name))
        {
          k.push_back(n);
          inserted = true;
        }
      }
    }
    baseS = serv.name;
    baseN = serv.SId;
    baseSubCh = serv.subChId;
    k.push_back(serv);
  }

  if (!inserted)
  {
    k.push_back(n);
  }
  return k;
}

//
//	In those case we are sure not to have an operating
//	dabProcessor, we hide some buttons
void RadioInterface::hide_for_safety()
{
  configWidget.dumpButton->hide();
  prevServiceButton->hide();
  nextServiceButton->hide();
  configWidget.contentButton->hide();
}

void RadioInterface::show_for_safety()
{
  configWidget.dumpButton->show();
  prevServiceButton->show();
  nextServiceButton->show();
  configWidget.contentButton->show();
}

void RadioInterface::_slot_handle_mute_button()
{
  mutingActive = !mutingActive;
  setButtonFont(muteButton, (mutingActive ? "Muting" : "Mute"), 10);
}

void RadioInterface::new_presetIndex(int index)
{
  if (presetSelector->currentIndex() == index)
  {
    return;
  }
  presetSelector->blockSignals(true);
  emit signal_set_new_preset_index(index);
  while (presetSelector->currentIndex() != 0)
  {
    usleep(200);
  }
  presetSelector->blockSignals(false);
}

void RadioInterface::new_channelIndex(int index)
{
  if (channelSelector->currentIndex() == index)
  {
    return;
  }
  disconnect(channelSelector, SIGNAL (activated(const QString &)), this, SLOT (_slot_handle_channel_selector(const QString &)));
  channelSelector->blockSignals(true);
  emit signal_set_new_channel(index);
  while (channelSelector->currentIndex() != index)
  {
    usleep(2000);
  }
  channelSelector->blockSignals(false);
  connect(channelSelector, SIGNAL (activated(const QString &)), this, SLOT (_slot_handle_channel_selector(const QString &)));
}


/////////////////////////////////////////////////////////////////////////
//	External configuration items				//////

void RadioInterface::_slot_handle_switch_delay_setting(int newV)
{
  dabSettings->setValue("switchDelay", newV);
}

void RadioInterface::_slot_handle_scan_mode_selector(int d)
{
  dabSettings->setValue("scanMode", d);
}

void RadioInterface::_slot_handle_save_service_selector(int d)
{
  (void)d;
  dabSettings->setValue("has-presetName", configWidget.saveServiceSelector->isChecked() ? 1 : 0);
}

void RadioInterface::_slot_handle_order_alfabetical()
{
  dabSettings->setValue("serviceOrder", ALPHA_BASED);
}

void RadioInterface::_slot_handle_order_service_ids()
{
  dabSettings->setValue("serviceOrder", ID_BASED);
}

void RadioInterface::_slot_handle_order_sub_channel_ids()
{
  dabSettings->setValue("serviceOrder", SUBCH_BASED);
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
        configWidget.EPGLabel->show();
        fprintf(stdout, "Starting hidden service %s\n", serv.name.toUtf8().data());
        my_dabProcessor->set_dataChannel(&pd, &dataBuffer, BACK_GROUND);
        dabService s;
        s.channel = pd.channel;
        s.serviceName = pd.serviceName;
        s.SId = pd.SId;
        s.SCIds = pd.SCIds;
        s.subChId = pd.subchId;
        s.fd = nullptr;
        channel.backgroundServices.push_back(s);

        for (int j = 0; j < model.rowCount(); j++)
        {
          QString itemText = model.index(j, 0).data(Qt::DisplayRole).toString();
          if (itemText == pd.serviceName)
          {
            colorService(model.index(j, 0), Qt::blue, fontSize + 2, true);
            break;
          }
        }
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
        configWidget.EPGLabel->show();
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

uint32_t RadioInterface::extract_epg(QString name, std::vector<serviceId> & serviceList, uint32_t ensembleId)
{
  (void)ensembleId;
  for (auto serv: serviceList)
  {
    if (name.contains(QString::number(serv.SId, 16), Qt::CaseInsensitive))
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

void RadioInterface::_slot_handle_tii_detector_mode(int d)
{
  bool b = configWidget.tii_detectorMode->isChecked();
  (void)d;
  my_dabProcessor->set_tiiDetectorMode(b);
  dabSettings->setValue("tii_detector", b ? 1 : 0);
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
    configButton->setText("Show Controls");
    configDisplay.hide();
    dabSettings->setValue("hidden", 1);
  }
  else
  {
    configButton->setText("Hide Controls");
    configDisplay.show();
    dabSettings->setValue("hidden", 0);
  }
}

void RadioInterface::slot_nr_services(int n)
{
  channel.serviceCount = n;
}

void RadioInterface::LOG(const QString & a1, const QString & a2)
{
  QString theTime;
  if (logFile == nullptr)
  {
    return;
  }
  if (configWidget.utcSelector->isChecked())
  {
    theTime = convertTime(UTC.year, UTC.month, UTC.day, UTC.hour, UTC.minute);
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
      LOG("Log started with ", inputDevice->deviceName());
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
  coordinates theCoordinator(dabSettings);
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
      httpButton->setText("http-on");
    }
  }
  else
  {
    locker.lock();
    delete mapHandler;
    mapHandler = nullptr;
    locker.unlock();
    httpButton->setText("http");
  }
}

void RadioInterface::slot_http_terminate()
{
  locker.lock();
  if (mapHandler != nullptr)
  {
    delete mapHandler;
    mapHandler = nullptr;
  }
  locker.unlock();
  httpButton->setText("http");
}

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
  p.loadFromData(QByteArray::fromRawData(reinterpret_cast<const char *>(PAUSESLIDE), PAUSESLIDE_SIZE), "png");  // ignore obsolete last zero in data
  write_picture(p);
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
//	that is why I use the button, originally named "scanButton"
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
  scanButton->setText("ETI");
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
    scanButton->setText("eti runs");
  }
}

void RadioInterface::_slot_handle_eti_active_selector(int k)
{
  (void)k;
  bool setting = configWidget.eti_activeSelector->isChecked();
  if (inputDevice == nullptr)
  {
    return;
  }

  if (setting)
  {
    stopScanning(false);
    disconnect(scanButton, SIGNAL (clicked()), this, SLOT (_slot_handle_scan_button()));
    connect(scanButton, SIGNAL (clicked()), this, SLOT (_slot_handle_eti_handler()));
    scanButton->setText("ETI");
    if (inputDevice->isFileInput())
    {  // restore the button' visibility
      scanButton->show();
    }
    return;
  }
  //	otherwise, disconnect the eti handling and reconnect scan
  //	be careful, an ETI session may be going on
  stop_etiHandler();    // just in case
  disconnect(scanButton, SIGNAL (clicked()), this, SLOT (_slot_handle_eti_handler()));
  connect(scanButton, SIGNAL (clicked()), this, SLOT (_slot_handle_scan_button()));
  scanButton->setText("scan");
  if (inputDevice->isFileInput())
  {  // hide the button now
    scanButton->hide();
  }
}

