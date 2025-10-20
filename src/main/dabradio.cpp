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
#include "epgdec.h"
#include "epg-decoder.h"
#include "audioiodevice.h"
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
Q_LOGGING_CATEGORY(sLogRadioInterface, "RadioInterface", QtInfoMsg)

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

  Settings::Main::posAndSize.read_widget_geometry(this, (cShowSIdInServiceList ? 40 : 0) + 730, 540, true);

  mpServiceListHandler.reset(new ServiceListHandler(iFileNameDb, ui->tblServiceList));
  mpTechDataWidget.reset(new TechData(this, mpTechDataBuffer));

  _initialize_ui_buttons();
  _initialize_status_info();
  _initialize_dynamic_label();
  _initialize_thermo_peak_levels();
  _initialize_audio_output();
  _initialize_paths();
  _initialize_epg();
  _initialize_time_table();
  _initialize_tii_file();
  _initialize_version_and_copyright_info();
  _initialize_band_handler();
  _initialize_device_selector();

  _show_pause_slide();
  _set_clock_text();

  _get_local_position_from_config(mChannel.localPos);
  _get_last_service_from_config();

  mConfig.cmbSoftBitGen->addItems(_get_soft_bit_gen_names()); // fill soft-bit-type combobox with text elements

#ifdef DATA_STREAMER
  dataStreamer = new tcpServer(iDataPort);
#else
  (void)iDataPort;
#endif

  connect(this, &DabRadio::signal_set_new_channel, ui->cmbChannelSelector, &QComboBox::setCurrentIndex);
  connect(ui->btnHttpServer, &QPushButton::clicked, this,  &DabRadio::_slot_handle_http_button);
  connect(mpTechDataWidget.get(), &TechData::signal_handle_timeTable, this, &DabRadio::_slot_handle_time_table);
  connect(this, &DabRadio::signal_dab_processor_started, &mSpectrumViewer, &SpectrumViewer::slot_update_settings);
  connect(&mSpectrumViewer, &SpectrumViewer::signal_window_closed, this, &DabRadio::_slot_handle_spectrum_button);
  connect(mpTechDataWidget.get(), &TechData::signal_window_closed, this, &DabRadio::_slot_handle_tech_detail_button);
  connect(ui->btnEject, &QPushButton::clicked, this, [this](bool){ _slot_new_device(mDeviceSelector.get_device_name()); });
  connect(ui->configButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_config_button);

  _initialize_and_start_timers();

  _show_or_hide_windows_from_config();

  _check_coordinates(); // highlight coordinates button if no coordinates are given

  // If a device was selected, we just start, otherwise we wait until one is selected
  if (mpInputDevice != nullptr)
  {
    do_start();
  }
  else
  {
    mConfig.show(); // show configuration windows that the user selects a (valid) device
    connect(mConfig.deviceSelector, &QComboBox::textActivated, this, &DabRadio::_slot_do_start);
  }
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

  qCInfo(sLogRadioInterface) << "RadioInterface is deleted";
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
    _disconnect_dab_processor_signals();
    return;
  }

  _set_device_to_file_mode(!mChannel.realChannel);

  do_start();
}

// _slot_new_device is called from the UI when selecting a device with the selector
void DabRadio::_slot_new_device(const QString & deviceName)
{
  // Part I : stopping all activities
  mIsRunning.store(false);
  stop_scanning();
  stop_channel();
  _disconnect_dab_processor_signals();

  if (mpInputDevice != nullptr)
  {
    qCDebug(sLogRadioInterface()) << "Device is deleted";
    mpInputDevice.reset();
  }

  LOG("selecting ", deviceName);

  if (mDeviceSelector.reset_file_input_last_file(deviceName)) // this allows new file name selection for "file input"
  {
    _set_device_to_file_mode(true); // deletes service list after pressing the Eject button
  }

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

// when doStart is called, a device is available and selected
void DabRadio::do_start()
{
  _update_channel_selector(mChannel.channelName);
  // _update_channel_selector(mChannel.nextService.channel);

  // here the DAB processor is created (again)
  mpDabProcessor.reset(new DabProcessor(this, mpInputDevice.get(), &mProcessParams));

  mChannel.clean_channel();

  // Some hidden buttons can be made visible now
  _connect_dab_processor_signals();
  _connect_dab_processor(); // TODO: check whether some connects needs be shifted or combined

  mTiiListDisplay.hide();

  mpDabProcessor->set_sync_on_strongest_peak(Settings::Config::cbUseStrongestPeak.read().toBool());
  mpDabProcessor->set_dc_avoidance_algorithm(Settings::Config::cbUseDcAvoidance.read().toBool());
  mpDabProcessor->set_dc_removal(Settings::Config::cbUseDcRemoval.read().toBool());
  mpDabProcessor->set_tii_collisions(Settings::Config::cbTiiCollisions.read().toBool());
  mpDabProcessor->set_tii_processing(true);
  mpDabProcessor->set_tii_threshold(Settings::Config::sbTiiThreshold.read().toInt());
  mpDabProcessor->set_tii_sub_id(Settings::Config::sbTiiSubId.read().toInt());
  mpDabProcessor->slot_soft_bit_gen_type((ESoftBitType)mConfig.cmbSoftBitGen->currentIndex());

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

  emit signal_dab_processor_started(); // triggers the DAB processor rereading (new) settings

  // after the preset timer signals, the service will be started
  start_channel(ui->cmbChannelSelector->currentText(), mChannel.SId_next);

  mIsRunning.store(true);
}

///////////////////////////////////////////////////////////////////////////////
// a slot called by the DAB-processor
void DabRadio::slot_set_and_show_freq_corr_rf_Hz(i32 iFreqCorrRF)
{
  if (mpInputDevice != nullptr && mChannel.nominalFreqHz > 0)
  {
    mpInputDevice->setVFOFrequency(mChannel.nominalFreqHz + iFreqCorrRF);
  }

  mSpectrumViewer.show_freq_corr_rf_Hz(iFreqCorrRF);
}

void DabRadio::slot_show_freq_corr_bb_Hz(i32 iFreqCorrBB)
{
  mSpectrumViewer.show_freq_corr_bb_Hz(iFreqCorrBB);
}

// might be called when scanning only
void DabRadio::_slot_channel_timeout()
{
  qCDebug(sLogRadioInterface()) << Q_FUNC_INFO;
  slot_no_signal_found();
}

// The ensembleId is written as hexadecimal, however, the
// number display of Qt is only 7 segments ...
static QString hex_to_str(i32 v)
{
  QString res;
  for (i32 i = 0; i < 4; i++)
  {
    u8 t = (v & 0xF000) >> 12;
    QChar c = t <= 9 ? (char)('0' + t) : (char)('A' + t - 10);
    res.append(c);
    v <<= 4;
  }
  return res;
}

// a slot, called by the fib processor
void DabRadio::slot_name_of_ensemble(const i32 iEId, const QString & iEnsName, const QString & iEnsNameShort)
{
  qCDebug(sLogRadioInterface) << Q_FUNC_INFO << iEId << iEnsName << iEnsNameShort;

  if (!mIsRunning.load())
  {
    return;
  }

  QFont font = ui->ensembleId->font();
  font.setPointSize(14);
  ui->ensembleId->setFont(font);
  ui->ensembleId->setText(iEnsName + QString("(") + hex_to_str(iEId) + QString(")"));

  mChannel.ensembleName = iEnsName;
  mChannel.ensembleNameShort = iEnsNameShort;
  mChannel.Eid = iEId;
}

void DabRadio::_slot_handle_content_button()
{
  if (mpContentTable != nullptr)
  {
    const bool isShown = mpContentTable->is_visible();
    mpContentTable->hide();
    mpContentTable.reset();
    if (isShown) return; 
  }

  i32 numCols = 0;
  const QStringList s = mpDabProcessor->get_fib_decoder()->get_fib_content_str_list(numCols); // every list entry is one line
  mpContentTable.reset(new ContentTable(this, &Settings::Storage::instance(), mChannel.channelName, numCols));
  connect(mpContentTable.get(), &ContentTable::signal_go_service_id, this, &DabRadio::slot_handle_fib_content_selector);

  for (const auto & sl : s)
  {
    mpContentTable->add_line(sl);
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

void DabRadio::slot_handle_mot_object(const QByteArray & result, const QString & objectName, i32 contentType, bool dirElement)
{
  qCDebug(sLogRadioInterface()) << "ObjectName" << objectName << "ContentType" << contentType;

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

bool DabRadio::save_MOT_EPG_data(const QByteArray & result, const QString & objectName, i32 contentType)
{
  if (mEpgPath == "")
  {
    return false;
  }

  std::vector<u8> epgData(result.begin(), result.end());
  const u32 ensembleId = mpDabProcessor->get_fib_decoder()->get_ensembleId();
  const u32 currentSId = _extract_epg(objectName, mServiceList, ensembleId);
  const u32 julianDate = mpDabProcessor->get_fib_decoder()->get_mod_julian_date();
  const i32 subType = getContentSubType((MOTContentType)contentType);
  mpEpgProcessor->process_epg(epgData.data(), (i32)epgData.size(), currentSId, subType, julianDate);

  if (mConfig.cmbEpgObjectSaving->currentIndex() > 0)
  {
    QString temp = objectName;
    if (temp.isEmpty())
    {
      temp = "epg_file";
    }

    const bool saveToDir = mConfig.cmbEpgObjectSaving->currentIndex() == 2;
    const QString path = generate_file_path(mEpgPath, temp, saveToDir);
    mpEpgHandler->decode(epgData, QDir::toNativeSeparators(path));
  }

  return true;
}

void DabRadio::save_MOT_text(const QByteArray & result, i32 contentType, const QString & name)
{
  (void)contentType;
  if (mMotPath == "")
  {
    return;
  }

  // TODO: we do not know the real content type for sure, so we omit the file extension here (seems to be almost png images)
  const bool saveToDir = mConfig.cmbMotObjectSaving->currentIndex() == 2;
  QString path = generate_unique_file_path_from_hash(mMotPath, "", result, saveToDir);
  path = QDir::toNativeSeparators(path);

  if (!QFile::exists(path))
  {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
      qCCritical(sLogRadioInterface(), "save_MOT_text(): cannot write file %s", path.toUtf8().data());
    }
    else
    {
      qCDebug(sLogRadioInterface(), "save_MOT_text(): going to write MOT file %s", path.toUtf8().data());
      file.write(result);
    }
  }
  else
  {
    qCDebug(sLogRadioInterface(), "save_MOT_text(): file %s already exists", path.toUtf8().data());
  }
}

void DabRadio::save_MOT_object(const QByteArray & result, const QString & name)
{
  if (mMotPath == "")
  {
    return;
  }

  // create_directory(mMotPath, false) is called in save_MOT_text()

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

QString DabRadio::generate_unique_file_path_from_hash(const QString & iBasePath, const QString & iFileExt, const QByteArray & iData, const bool iStoreAsDir) const
{
  QString filename = QCryptographicHash::hash(iData, QCryptographicHash::Sha1).toHex().left(8);
  if (!iFileExt.isEmpty()) filename += "." + iFileExt;
  return generate_file_path(iBasePath, filename, iStoreAsDir);
}

QString DabRadio::generate_file_path(const QString & iBasePath, const QString & iFileName, const bool iStoreAsDir) const
{
  static const QRegularExpression regex(R"([<>:\"/\\|?*\s])"); // remove invalid file path characters
  const QString pathEnsemble = mChannel.channelName + "-" + mChannel.ensembleName.trimmed().replace(regex, "-");
  const QString pathService = mChannel.curPrimaryService.serviceLabel.trimmed().replace(regex, "-");
  const QString filename = pathEnsemble + "_" + pathService + "_" + iFileName;
  const QString filepath = pathEnsemble + "/" + pathService + "/";
  QString path = iBasePath;
  if (iStoreAsDir) path += filepath;
  path += filename;
  create_directory(path, true);
  return path;
}

// MOT slide, to show
void DabRadio::show_MOT_image(const QByteArray & data, const i32 contentType, const QString & pictureName, const i32 dirs)
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

  const bool saveMotObject = mConfig.cmbMotObjectSaving->currentIndex() > 0;

  if (saveMotObject && !mPicturesPath.isEmpty() && !mChannel.curPrimaryService.serviceLabel.isEmpty())
  {
    const bool saveToDir = mConfig.cmbMotObjectSaving->currentIndex() == 2;
    QString pict = generate_unique_file_path_from_hash(mPicturesPath, type, data, saveToDir);
    pict = QDir::toNativeSeparators(pict);
    // qInfo() << "filepath:" << pict << "(pictureName:" << pictureName << ")";

    if (!QFile::exists(pict))
    {
      QFile file(pict);
      if (!file.open(QIODevice::WriteOnly))
      {
        qCCritical(sLogRadioInterface()) << "Cannot write file" << pict;
      }
      else
      {
        qCInfo(sLogRadioInterface()) << "Write picture to" << pict;
        file.write(data);
      }
    }
    else if (pict != mMotPicPathLast) // show this message only once because some services transfers only one picture all day long
    {
      qCInfo(sLogRadioInterface()) << "File" << pict << "already exists";
    }
    mMotPicPathLast = pict;
  }

  // only show slides if it is a audio service
  //if (mChannel.currentService.is_audio)
  {
    QPixmap p;
    p.loadFromData(data, type);
    write_picture(p);
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

void DabRadio::write_picture(const QPixmap & iPixMap) const
{
  constexpr i32 w = 320;
  constexpr i32 h = 240;

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
// sendDatagram is triggered by the ip handler,
void DabRadio::slot_send_datagram(i32 length)
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
// tdcData is triggered by the backend.
void DabRadio::slot_handle_tdc_data(i32 frametype, i32 length)
{
  qCDebug(sLogRadioInterface) << Q_FUNC_INFO << frametype << length;
#ifdef DATA_STREAMER
  auto * const localBuffer = make_vla(u8, length + 8);
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

  // dump data to screen
  // mpDataBuffer->get_data_from_ring_buffer(localBuffer, length);
  // qCDebug(sLogRadioInterface) << "tdcData: " << QString::fromWCharArray(reinterpret_cast<wchar_t *>(localBuffer), length);

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
  qCWarning(sLogRadioInterface()) << "Configuration change is not supported yet";

  //   if (!mIsRunning.load() || mpDabProcessor == nullptr)
//   {
//     return;
//   }
//
//   SDabService s;
//
//   // if (mChannel.currentService.dataSetIsValid)
//   {
//     s = mChannel.currentService;
//   }
//
//   stop_service(s); // stops also dumps and make cleanups
//   stop_scanning();
//
//   // we stop all secondary services as well, but we maintain there description, file descriptors remain of course
//   for (const auto & backgroundService : mChannel.backgroundServices)
//   {
//     mpDabProcessor->stop_service(backgroundService.SubChId, EProcessFlag::Background);
//   }
//
//   qCInfo(sLogRadioInterface()) << "Configuration change will be done";
//
//   // We rebuild the services list from the fib and then we (try to) restart the service
//   mServiceList = mpDabProcessor->get_fib_decoder()->get_service_list();
//
//   if (mChannel.etiActive)
//   {
//     mpDabProcessor->reset_eti_generator();
//   }
//
//   // Of course, it may be disappeared
//   // if (s.dataSetIsValid)  // TODO: this should always be false?!
//   {
//     const QString sl = mpDabProcessor->get_fib_decoder()->get_service_label_from_SId_SCIdS(s.SId, s.SCIdS);
//
//     if (!sl.isEmpty())
//     {
//       start_primary_and_secondary_service(s);
//       return;
//     }
//
//     // The service is gone, it may be the subservice of another one
//     s.SCIdS = 0;
//     // s.serviceLabel = sl;
//
// //    if (!s.serviceLabel.isEmpty())
//     if (s.SId != 0)
//     {
//       start_primary_and_secondary_service(s);
//     }
//   }

  // we also have to restart all background services,
  // for (u16 i = 0; i < mChannel.backgroundServices.size(); ++i)
  // {
  //   if (const QString ss = mpDabProcessor->get_fib_decoder()->get_service_label_from_SId_SCIdS(s.SId, s.SCIdS);
  //       ss == "") // it is gone, close the file if any
  //   {
  //     // if (mChannel.backgroundServices.at(i).fd != nullptr)
  //     // {
  //     //   fclose(mChannel.backgroundServices.at(i).fd);
  //     // }
  //     mChannel.backgroundServices.erase(mChannel.backgroundServices.begin() + i);
  //   }
  //   else // (re)start the service
  //   {
  //     SAudioData ad;
  //     mpDabProcessor->get_fib_decoder()->get_data_for_audio_service(ss, &ad);
  //     // mpDabProcessor->get_fib_decoder()->get_data_for_audio_service(ss, &ad);
  //
  //     if (ad.isDefined) // TODO: is there a nicer way doing that?
  //     {
  //       // FILE * f = mChannel.backgroundServices.at(i).fd;
  //       mpDabProcessor->set_audio_channel(&ad, mpAudioBufferFromDecoder, EProcessFlag::Background);
  //       mChannel.backgroundServices.at(i).SubChId = ad.subchId;
  //     }
  //     else
  //     {
  //       std::vector<SPacketData> pdVec;
  //       mpDabProcessor->get_fib_decoder()->get_data_for_packet_service(ss, pdVec);
  //       if (!pdVec.empty())
  //       {
  //         mpDabProcessor->set_data_channel(&pdVec[0], mpDataBuffer, EProcessFlag::Background);
  //         mChannel.backgroundServices.at(i).SubChId = pdVec[0].subchId;
  //         // TODO: considering further data packages?
  //         if (pdVec.size() > 1)
  //         {
  //           qCWarning(sLogRadioInterface()) << "Warning: more than one data packet for service, but ignored (1)";
  //         }
  //       }
  //     }
  //   }
  // }
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
  mEpgTimer.stop();

  emit signal_stop_audio();

  if (mDlTextFile != nullptr)
  {
    fclose(mDlTextFile);
  }

  if (mpDabProcessor != nullptr)
  {
    mpDabProcessor->stop();
  }

  mpTechDataWidget->hide();

  if (mpContentTable != nullptr)
  {
    // mpContentTable->clearTable();
    mpContentTable->hide();
    mpContentTable.reset();
  }

  mBandHandler.saveSettings();
  stop_audio_frame_dumping();
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

void DabRadio::slot_handle_tii_threshold(i32 trs)
{
  if (mpDabProcessor == nullptr)
  {
    return;
  }

  mpDabProcessor->set_tii_threshold(trs);
}

void DabRadio::slot_handle_tii_subid(i32 subid)
{
  if (mpDabProcessor == nullptr)
  {
    return;
  }

  mpDabProcessor->set_tii_sub_id(subid);
}

void DabRadio::slot_fib_time(const IFibDecoder::SUtcTimeSet & iFibTimeInfo)
{
  // convert to local time and adapt date if needed
  const i32 daysOffs = _get_local_time(mLocalTime.hour, mLocalTime.minute, iFibTimeInfo.Hour, iFibTimeInfo.Minute, iFibTimeInfo.LTO_Minutes);

  mUTC.hour = iFibTimeInfo.Hour;
  mUTC.minute = iFibTimeInfo.Minute;
  mUTC.second = mLocalTime.second = iFibTimeInfo.Second;

  _get_YMD_from_mod_julian_date(mUTC.year, mUTC.month, mUTC.day, iFibTimeInfo.ModJulianDate);

  if (daysOffs != 0) // save a bit calculation time if local date is same than UTC date
  {
    _get_YMD_from_mod_julian_date(mLocalTime.year, mLocalTime.month, mLocalTime.day, iFibTimeInfo.ModJulianDate + daysOffs);
  }
  else
  {
    mLocalTime.year = mUTC.year;
    mLocalTime.month = mUTC.month;
    mLocalTime.day = mUTC.day;
  }

  if (Settings::Config::cbUseUtcTime.read().toBool())
  {
    _set_clock_text(_conv_to_time_str(mUTC.year, mUTC.month, mUTC.day, mUTC.hour, mUTC.minute, iFibTimeInfo.Second));
  }
  else
  {
    _set_clock_text(_conv_to_time_str(mLocalTime.year, mLocalTime.month, mLocalTime.day, mLocalTime.hour, mLocalTime.minute, iFibTimeInfo.Second));
  }
}

void DabRadio::_get_YMD_from_mod_julian_date(i32 & oYear, i32 & oMonth, i32 & oDay, const i32 iMJD) const
{
  // Convert Modified Julian Date (MJD) to Gregorian calendar date (year, month, day)
  // This algorithm follows the method described by Jean Meeus in "Astronomical Algorithms"
  const i32 J = iMJD + 2400001;  // Convert Modified Julian Date to Julian Date by adding offset
  const i32 j = J + 32044;       // Add constant to account for the difference between Julian and Gregorian calendars

  // Break down the date into cycles of different lengths
  const i32 g = j / 146097;   // Number of complete 400-year cycles (146097 days per 400-year cycle, a avr. year take 365.2422 days)
  const i32 dg = j % 146097;  // Remaining days after complete 400-year cycles

  // Handle century years (100-year cycles)
  const i32 c = ((dg / 36524) + 1) * 3 / 4;  // Number of complete 100-year cycles, adjusted for leap years
  const i32 dc = dg - c * 36524;             // Remaining days after complete 100-year cycles

  // Handle 4-year cycles
  const i32 b = dc / 1461;   // Number of complete 4-year cycles (1461 days per 4-year cycle)
  const i32 db = dc % 1461;  // Remaining days after complete 4-year cycles

  // Handle individual years within a 4-year cycle
  const i32 a = ((db / 365) + 1) * 3 / 4;  // Year within the 4-year cycle, adjusted for leap years
  const i32 da = db - a * 365;             // Day of the year (0-based)

  // Calculate the preliminary year
  const i32 y = g * 400 + c * 100 + b * 4 + a;  // Combine all cycle components to get the year

  // Calculate month and day
  // The formula shifts the calendar to start in March (m=0) and end in February (m=11)
  // This simplifies leap year calculations
  const i32 m = ((da * 5 + 308) / 153) - 2;      // Convert day of year to month (with shifted calendar)
  const i32 d = da - ((m + 4) * 153 / 5) + 122;  // Calculate day of month

  // Adjust year and month to standard calendar format (January = 1, December = 12)
  oYear = y - 4800 + ((m + 2) / 12);  // Final Gregorian year
  oMonth = ((m + 2) % 12) + 1;        // Final Gregorian month (1-12)
  oDay = d + 1;                       // Final Gregorian day of month (1-based)
}

i32 DabRadio::_get_local_time(i32 & oLocalHour, i32 & oLocalMinute, const i32 iUtcHour, const i32 iUtcMinute, const i32 iLtoMinutes) const
{
  oLocalHour = iUtcHour;
  oLocalMinute = iUtcMinute;

  oLocalMinute += iLtoMinutes; // consider local time offset, including daylight saving time

  while (oLocalMinute >= 60)
  {
    oLocalMinute -= 60;
    oLocalHour++;
  }

  while (oLocalMinute < 0)
  {
    oLocalMinute += 60;
    oLocalHour--;
  }

  if (oLocalHour > 23)
  {
    oLocalHour -= 24;
    return 1; // shift date to the next day
  }

  if (oLocalHour < 0)
  {
    oLocalHour += 24;
    return -1; // shift date to the previous day
  }

  return 0; // no day-shift needed
}

QString DabRadio::_conv_to_time_str(i32 year, i32 month, i32 day, i32 hours, i32 minutes, i32 seconds) const
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
// called from the MP4 decoder
void DabRadio::slot_show_frame_errors(i32 s)
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
// called from the MP4 decoder
void DabRadio::slot_show_rs_errors(i32 s)
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
// called from the aac decoder
void DabRadio::slot_show_aac_errors(i32 s)
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
// called from the ficHandler
void DabRadio::slot_show_fic_status(const i32 iSuccessPercent, const f32 iBER)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (ui->progBarFicError->value() != iSuccessPercent)
  {
    if (iSuccessPercent < 85)
    {
      ui->progBarFicError->setStyleSheet("QProgressBar::chunk { background-color: red; }");
    }
    else
    {
      ui->progBarFicError->setStyleSheet("QProgressBar::chunk { background-color: #E6E600; }");
    }

    ui->progBarFicError->setValue(iSuccessPercent);
  }

  if (!mSpectrumViewer.is_hidden())
  {
    mSpectrumViewer.show_fic_ber(iBER);
  }
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

// called from the PAD handler

void DabRadio::slot_show_label(const QString & s)
{
  if (mIsRunning.load())
  {
    ui->lblDynLabel->setStyleSheet("color: white");
    ui->lblDynLabel->setText(_convert_links_to_clickable(s));
  }

  // if we dtText is ON, some work is still to be done
  if ((mDlTextFile == nullptr) || (mDynLabelCache.add_if_new(s)))
  {
    return;
  }

  QString currentChannel = mChannel.channelName;
  QDateTime theDateTime = QDateTime::currentDateTime();
  fprintf(mDlTextFile,
          "%s.%s %4d-%02d-%02d %02d:%02d:%02d  %s\n",
          currentChannel.toUtf8().data(),
          mChannel.curPrimaryService.serviceLabel.toUtf8().data(),
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

static QString tiiNumber(i32 n)
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

  QString country;

  if (!mChannel.ecc_checked)
  {
    mChannel.ecc_checked = true;
    mChannel.ecc_byte = mpDabProcessor->get_fib_decoder()->get_ecc();
    country = find_ITU_code(mChannel.ecc_byte, (mChannel.Eid >> 12) & 0xF);
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
  STiiDataEntry tr_fb; // fallback data storage, if no database entry was found

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

    const STiiDataEntry * pTr = mTiiHandler.get_transmitter_name((mChannel.realChannel ? mChannel.channelName : "any"), mChannel.Eid, tii.mainId, tii.subId);
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

    if (!isDropDownVisible)
    {
      if (dataValid)
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
      else
      {
        ui->cmbTiiList->addItem(QString("%1/%2: No database entry found for TII %3-%4")
                                .arg(index + 1)
                                .arg(mTransmitterIds.size())
                                .arg(tiiNumber(tii.mainId))
                                .arg(tiiNumber(tii.subId)));

      }
    }

    // see if we have a map
    if (mpHttpHandler && dataValid)
    {
      const QDateTime theTime = (Settings::Config::cbUseUtcTime.read().toBool() ? QDateTime::currentDateTimeUtc() : QDateTime::currentDateTime());
      mpHttpHandler->putData(MAP_NORM_TRANS, pTr, theTime.toString(Qt::TextDate),
                             bd.strength_dB, (i32)bd.distance_km, (i32)bd.corner_deg, bd.isNonEtsiPhase);
    }
  }

  // iterate over combobox entries, if activated
  if (mConfig.cbAutoIterTiiEntries->isChecked() && ui->cmbTiiList->count() > 0 && !isDropDownVisible)
  {
    ui->cmbTiiList->setCurrentIndex((i32)mTiiIndex % ui->cmbTiiList->count());

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

void DabRadio::slot_show_iq(i32 iAmount, f32 iAvg)
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

// called from the MP4 decoder
void DabRadio::slot_show_rs_corrections(i32 c, i32 ec)
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
// called from the DAB processor
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
// called from the phasesynchronizer
void DabRadio::slot_show_correlation(f32 threshold, const QVector<i32> & v)
{
  if (!mIsRunning.load())
  {
    return;
  }

  mSpectrumViewer.show_correlation(threshold, v, mTransmitterIds);
  mChannel.nrTransmitters = v.size();
}

////////////////////////////////////////////////////////////////////////////

void DabRadio::slot_set_stream_selector(i32 k)
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
    //mConfig.dumpButton->show();
    ui->btnScanning->show();
    ui->cmbChannelSelector->show();
    ui->btnToggleFavorite->show();
    ui->btnEject->hide();
  }
  else
  {
    //mConfig.dumpButton->hide();
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
// dump handling
//
/////////////////////////////////////////////////////////////////////////

static void emphasize_pushbutton(QPushButton * const ipPB, const bool iEmphasize)
{
  ipPB->setStyleSheet(iEmphasize ? "background-color: #AE2B05; color: #EEEE00; font-weight: bold;" : "");
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
  emphasize_pushbutton(mConfig.dumpButton, false);
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
  emphasize_pushbutton(mConfig.dumpButton, true);
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
  emphasize_pushbutton(mpTechDataWidget->audiodumpButton, false);
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
  emphasize_pushbutton(mpTechDataWidget->audiodumpButton, true);
  mAudioDumpState = EAudioDumpState::WaitForInit;
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

void DabRadio::stop_audio_frame_dumping()
{
  if (mpAudioFrameDumper == nullptr)
  {
    return;
  }

  fclose(mpAudioFrameDumper);
  emphasize_pushbutton(mpTechDataWidget->framedumpButton, false);
  mpAudioFrameDumper = nullptr;
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

  emphasize_pushbutton(mpTechDataWidget->framedumpButton, true);
}

// called from the mp4 handler, using a signal
void DabRadio::slot_new_frame()
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

void DabRadio::_connect_dab_processor()
{
  // we avoided till now connecting the channel selector to the slot since that function does a lot more, things we do not want here
  connect(ui->cmbChannelSelector, &QComboBox::textActivated, this, &DabRadio::_slot_handle_channel_selector);
  connect(&mSpectrumViewer, &SpectrumViewer::signal_cb_nom_carrier_changed, mpDabProcessor.get(), &DabProcessor::slot_show_nominal_carrier);
  connect(&mSpectrumViewer, &SpectrumViewer::signal_cmb_carrier_changed, mpDabProcessor.get(), &DabProcessor::slot_select_carrier_plot_type);
  connect(&mSpectrumViewer, &SpectrumViewer::signal_cmb_iqscope_changed, mpDabProcessor.get(), &DabProcessor::slot_select_iq_plot_type);
  connect(mConfig.cmbSoftBitGen, qOverload<i32>(&QComboBox::currentIndexChanged), mpDabProcessor.get(), [this](i32 idx) { mpDabProcessor->slot_soft_bit_gen_type((ESoftBitType)idx); });

  // Just to be sure we disconnect here. It would have been helpful to have a function testing whether or not a connection exists, we need a kind of "reset"
  disconnect(mConfig.deviceSelector, &QComboBox::textActivated, this, &DabRadio::_slot_do_start);
  disconnect(mConfig.deviceSelector, &QComboBox::textActivated, this, &DabRadio::_slot_new_device);
  connect(mConfig.deviceSelector, &QComboBox::textActivated, this, &DabRadio::_slot_new_device);
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
    connect(ui->btnFib, &QPushButton::clicked, this, &DabRadio::_slot_handle_content_button),
    connect(ui->btnTechDetails, &QPushButton::clicked, this, &DabRadio::_slot_handle_tech_detail_button),
    connect(mConfig.resetButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_reset_button),
    connect(ui->btnScanning, &QPushButton::clicked, this, &DabRadio::_slot_handle_scan_button),
    connect(ui->btnSpectrumScope, &QPushButton::clicked, this, &DabRadio::_slot_handle_spectrum_button),
    connect(ui->btnDeviceWidget, &QPushButton::clicked, this, &DabRadio::_slot_handle_device_widget_button),
    connect(mConfig.dumpButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_source_dump_button),
    connect(mConfig.etiButton, &QPushButton::clicked, this, &DabRadio::_slot_handle_eti_button),
    connect(ui->btnPrevService, &QPushButton::clicked, this, &DabRadio::_slot_handle_prev_service_button),
    connect(ui->btnNextService, &QPushButton::clicked, this, &DabRadio::_slot_handle_next_service_button),
    connect(ui->btnTargetService, &QPushButton::clicked, this, &DabRadio::_slot_handle_target_service_button),
    connect(mpTechDataWidget.get(), &TechData::signal_handle_audioDumping, this, &DabRadio::_slot_handle_audio_dump_button),
    connect(mpTechDataWidget.get(), &TechData::signal_handle_frameDumping, this, &DabRadio::_slot_handle_frame_dump_button),
    connect(ui->btnMuteAudio, &QPushButton::clicked, this, &DabRadio::_slot_handle_mute_button),
    connect(mConfig.skipList_button, &QPushButton::clicked, this, &DabRadio::_slot_handle_skip_list_button),
    connect(mConfig.skipFile_button, &QPushButton::clicked, this, &DabRadio::_slot_handle_skip_file_button),
    connect(mpServiceListHandler.get(), &ServiceListHandler::signal_selection_changed, this, &DabRadio::_slot_service_changed),
    connect(mpServiceListHandler.get(), &ServiceListHandler::signal_favorite_status, this, &DabRadio::_slot_favorite_changed),
    connect(ui->btnToggleFavorite, &QPushButton::clicked, this, &DabRadio::_slot_handle_favorite_button),
    connect(ui->btnTii, &QPushButton::clicked, this, &DabRadio::_slot_handle_tii_button),
    connect(ui->btnCir, &QPushButton::clicked, this, &DabRadio::_slot_handle_cir_button),
    connect(mpDabProcessor->get_fib_decoder(), &IFibDecoder::signal_fib_loaded_state, this, &DabRadio::_slot_fib_loaded_state, Qt::QueuedConnection),
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

void DabRadio::slot_start_announcement(const QString & name, i32 subChId)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (name == ui->serviceLabel->text())
  {
    _set_status_info_status(mStatusInfo.Announce, true);
    qCInfo(sLogRadioInterface()) << "Announcement starts for service " << ui->serviceLabel->text() << "subchannel " << subChId;
  }
}

void DabRadio::slot_stop_announcement(const QString & name, i32 subChId)
{
  if (!mIsRunning.load())
  {
    return;
  }

  if (name == ui->serviceLabel->text())
  {
    _set_status_info_status(mStatusInfo.Announce, false);
    qCInfo(sLogRadioInterface()) << "Announcement stops for service " << ui->serviceLabel->text() << "subchannel " << subChId;
  }
}

// called from the service list handler when when the favorite state has changed
void DabRadio::_slot_favorite_changed(const bool iIsFav)
{
  mCurFavoriteState = iIsFav;
  set_favorite_button_style();
}

// called from the service list handler when channel and service has changed
void DabRadio::_slot_service_changed(const QString & iChannel, const QString & /*iService*/, const u32 iSId)
{
  if (!mIsScanning)
  {
    local_select(iChannel, iSId);
  }
}

// Selecting from a FIB content list
void DabRadio::slot_handle_fib_content_selector(const u32 iSId)
{
  local_select(mChannel.channelName, iSId);
}

void DabRadio::local_select(const QString & iChannel, const u32 iSId)
{
  if (mpDabProcessor == nullptr)  // should not happen
  {
    return;
  }

  if (iChannel == mChannel.channelName)  // Is it only a service change within the same channel?
  {
    if (iSId == 0)
    {
      write_warning_message("Insufficient data for this program (1)");
      return;
    }

    stop_services(false); // keep "global" services running

    start_primary_and_secondary_service(iSId, false);
  }
  else  // channel (and service) has changed
  {
    stop_channel();

    // TODO: make nicer
    i32 k = ui->cmbChannelSelector->findText(iChannel);
    if (k != -1)
    {
      _update_channel_selector(k);
    }
    else
    {
      QMessageBox::warning(this, tr("Warning"), tr("Incorrect preset\n"));
      return;
    }

    mChannel.SId_next = iSId;

    // Calling start_channel() builds up a new FIB tree and triggers finally calls to _slot_fib_loaded_state()
    start_channel(ui->cmbChannelSelector->currentText(), iSId);
  }
}

void DabRadio::stop_services(const bool iStopAlsoGlobServices)
{
  mChannelTimer.stop();

  if (mpDabProcessor == nullptr)
  {
    return;
  }

  stop_audio_frame_dumping();
  stop_audio_dumping();

  mpTechDataWidget->cleanUp(); // clean up the technical widget
  clean_up(); // clean up this main widget

  if (mChannel.curPrimaryService.isAudio)
  {
    emit signal_stop_audio();
  }

  if (iStopAlsoGlobServices)
  {
    mpDabProcessor->stop_all_services();
  }
  else
  {
    // TODO: keep "global" services like EPGs running, but currently clear all services in the MSC handler
    mpDabProcessor->stop_all_services();
  }

  mChannel.curSecondaryServiceVec.clear();
  mAudioFrameType = EAudioFrameType::None;

  _show_pause_slide();
  clean_screen();
}

void DabRadio::_display_service_label(const QString & iServiceLabel)
{
  QFont font = ui->serviceLabel->font();
  font.setPointSize(16);
  font.setBold(true);
  ui->serviceLabel->setStyleSheet("QLabel { color: lightblue; }");
  ui->serviceLabel->setFont(font);
  ui->serviceLabel->setText(iServiceLabel);
}

bool DabRadio::start_primary_and_secondary_service(const u32 iSId, const bool iStartPrimaryAudioOnly)
{
  /*
   * This method can be called twice after all services are terminated and a new service is selected:
   * For a faster audio startup where only FIG 0/1 and FIG 0/2 are needed, iStartPrimaryAudioOnly == true.
   * In this case the packet data are not retrieved. This happens afterwards with a second call to this method
   * with iStartPrimaryAudioOnly == false where all necessary FIB data are given.
   * Calling this with iStartPrimaryAudioOnly == true is not mandatory.
   */

  assert(mIsScanning == false); // while scan, no service handler should be started

  mpServiceListHandler->set_selector(mChannel.channelName, iSId);

  ui->lblDynLabel->setText("");

  // First try to setup a audio channel with this SId
  // TODO: Maybe we can skip this because SIds for primary packet data are always bigger than 0xFFFF?
  SAudioData ad;
  mpDabProcessor->get_fib_decoder()->get_data_for_audio_service(iSId, ad);

  if (ad.isDefined) // belongs SId to a audio service?
  {
    // qDebug() << "curPrimaryService.ServiceStarted" << mChannel.curPrimaryService.ServiceStarted;
    mChannel.curPrimaryService.SId = iSId;
    mChannel.curPrimaryService.isAudio = true;
    mChannel.curPrimaryService.SubChId = ad.SubChId;
    mChannel.curPrimaryService.serviceLabel = ad.serviceLabel; // may not be initialized (empty) with iStartPrimaryAudioOnly == true

    // TODO: how handle EPG?
    // if (mpDabProcessor->get_fib_decoder()->has_time_table(ad.SId))
    // {
    //   mpTechDataWidget->slot_show_timetableButton(true);
    // }

    mpTechDataWidget->show_service_data(&ad); // may not show all data with iStartPrimaryAudioOnly == true
    _set_status_info_status(mStatusInfo.InpBitRate, (i32)ad.bitRate); // the (i32) is important for the template deduction

    if (!_create_primary_backend_audio_service(ad) && iStartPrimaryAudioOnly)
    {
      qCCritical(sLogRadioInterface()) << "Could not create primary audio service";
      return false;
    }

    // Now try to find and create the secondary service(s)
    if (!iStartPrimaryAudioOnly) // only if FIB data complete loaded
    {
      std::vector<SPacketData> pdVec;
      mpDabProcessor->get_fib_decoder()->get_data_for_packet_service(iSId, pdVec);

      if (!pdVec.empty())
      {
        // TODO: considering further data packages?
        if (pdVec.size() > 1)
        {
          qCWarning(sLogRadioInterface()) << "More than one data packet for service, but ignored (1)";
        }

        const SPacketData & pd = pdVec[0];

        SDabService ds;
        ds.SId = pd.SId;
        ds.SubChId = pd.SubChId;
        ds.SCIdS = pd.SCIdS;
        ds.isAudio = false;
        // ds.ServiceStarted = true;

        if (!_create_secondary_backend_packet_service(pd))
        {
          qCCritical(sLogRadioInterface()) << "Could not create a secondary packet service";
          return false;
        }

        mChannel.curSecondaryServiceVec.emplace_back(ds);
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
      const SPacketData & pd = pdVec[0];
      mChannel.curPrimaryService.isAudio = false;
      mChannel.curPrimaryService.SubChId = pd.SubChId;
      mChannel.curPrimaryService.serviceLabel = pd.serviceLabel;

      // TODO: considering further data packages?
      if (pdVec.size() > 1)
      {
        qCWarning(sLogRadioInterface()) << "More than one data packet for service, but ignored (2)";
      }

      if (!_create_primary_backend_packet_service(pd))
      {
        qCCritical(sLogRadioInterface()) << "Could not create primary packet service";
        return false;
      }
    }
  }

  // Store primary channel to settings file
  Settings::Main::varPresetChannel.write(mChannel.channelName);
  Settings::Main::varPresetSId.write(mChannel.curPrimaryService.SId);

  _display_service_label(mChannel.curPrimaryService.serviceLabel);
  _update_audio_data_addon(mChannel.curPrimaryService.SId);
  return true;
}

bool DabRadio::_create_primary_backend_audio_service(const SAudioData & iAD)
{
  if (!iAD.isDefined || iAD.bitRate == 0)
  {
    write_warning_message("Insufficient data for this program (2)");
    return false;
  }

  if (!mpDabProcessor->is_service_running(iAD, EProcessFlag::Primary))
  {
    mpDabProcessor->set_audio_channel(iAD, mpAudioBufferFromDecoder, EProcessFlag::Primary);
    mpCurAudioFifo = nullptr; // trigger possible new sample rate setting
    mAudioFrameType = (iAD.ASCTy == 0x3F ? EAudioFrameType::AAC : EAudioFrameType::MP2);
  }
  else
  {
    qDebug() << "Primary audio service already started";
  }

  return true;
}

bool DabRadio::_create_secondary_backend_packet_service(const SPacketData & iPD)
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

bool DabRadio::_create_primary_backend_packet_service(const SPacketData & iPD)
{
  if (!iPD.isDefined || iPD.DSCTy == 0 || iPD.bitRate == 0)
  {
    write_warning_message("Insufficient data for this program (3)");
    return false;
  }

  if (!mpDabProcessor->is_service_running(iPD, EProcessFlag::Primary))
  {
    mpDabProcessor->set_data_channel(iPD, mpDataBuffer, EProcessFlag::Primary);

    // Unfortunately, we can not support primary data services yet, but the work will go on...
    switch (iPD.DSCTy)
    {
    case 5:
      qCDebug(sLogRadioInterface()) << "Selected AppType" << iPD.appTypeVec[0];
      write_warning_message("Primary 'Transparent Data Channel' (TCD) not supported");
      break;
    case 24:
      write_warning_message("Primary 'MPEG-2 Transport Stream' not supported");
      break;
    case 60:
      write_warning_message("Primary 'Multimedia Object Transfer' (MOT) not supported");
      break;
    case 44:
      write_warning_message("Primary Journaline");
      break;
    default:
      write_warning_message("Primary unimplemented protocol");
      break;
    }
  }
  else
  {
    qDebug() << "Primary packet service already started";
  }

  return true;
}

void DabRadio::clean_screen()
{
  ui->serviceLabel->setText("");
  ui->lblDynLabel->setText("");
  mpTechDataWidget->cleanUp();
  ui->programTypeLabel->setText("");
  mpTechDataWidget->cleanUp();
  _reset_status_info();
}

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

void DabRadio::_update_audio_data_addon(const u32 iSId) const
{
  SAudioDataAddOns adao;
  mpDabProcessor->get_fib_decoder()->get_data_for_audio_service_addon(iSId, adao);
  mpTechDataWidget->show_service_data_addon(&adao);
  ui->programTypeLabel->setText(adao.programType.has_value() ? getProgramType(adao.programType.value()) : "(no info received)");
}

void DabRadio::_update_scan_statistics(const SServiceId & sl)
{
  if (sl.isAudioChannel)
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

void DabRadio::_slot_fib_loaded_state(const IFibDecoder::EFibLoadingState iFibLoadingState)
{
  qDebug() << "FIB data for _slot_fib_data_loaded() loaded to state" << (i32)iFibLoadingState;

  assert(iFibLoadingState >= IFibDecoder::EFibLoadingState::S1_FastAudioDataLoaded &&
         iFibLoadingState <= IFibDecoder::EFibLoadingState::S5_DeferredDataLoaded);

  if (!mIsRunning.load())
  {
    return;
  }

  // while scanning, only the audio data with labels are important
  if (mIsScanning && iFibLoadingState != IFibDecoder::EFibLoadingState::S3_FullyAudioDataLoaded)
  {
    return;
  }

  if (iFibLoadingState == IFibDecoder::EFibLoadingState::S1_FastAudioDataLoaded)
  {
    if (mChannel.SId_next == 0)
    {
      qCCritical(sLogRadioInterface()) << "Fast audio select triggered, but no SId_next set";
      return;
    }

    qDebug() << "Fast audio select triggered -> necessary data for audio start available (except service label)";

    start_primary_and_secondary_service(mChannel.SId_next, true);
    return;
  }

  mServiceList = mpDabProcessor->get_fib_decoder()->get_service_list();

  ServiceListHandler::TSIdList SIdList;

  // Fill service list with wanted (via filter) content
  for (const auto & sl : mServiceList)
  {
    if (sl.isAudioChannel)
    {
      SIdList << sl.SId;
      mpServiceListHandler->add_entry(mChannel.channelName, sl.serviceLabel, sl.SId);
    }

    if (mIsScanning)
    {
      _update_scan_statistics(sl); // collect statistics while scanning
    }
  }

  if (!mIsScanning) // mChannel.nextService is invalid while scanning
  {
    // Crosscheck service list if a not more existing service is stored there (while scan the list was already cleaned up before)
    mpServiceListHandler->delete_not_existing_SId_at_channel(mChannel.channelName, SIdList);

    if (iFibLoadingState >= IFibDecoder::EFibLoadingState::S4_FullyPacketDataLoaded && mChannel.SId_next > 0)
    {
      start_primary_and_secondary_service(mChannel.SId_next, false);
      mChannel.SId_next = 0; // used, so make it invalid
    }
  }
  else
  {
    ui->lblDynLabel->setText(_get_scan_message(false));
  }
}

void DabRadio::write_warning_message(const QString & iMsg) const
{
  ui->lblDynLabel->setStyleSheet("color: #ff9100");
  ui->lblDynLabel->setText(iMsg);
}

///////////////////////////////////////////////////////////////////////////
//
// Channel basics
///////////////////////////////////////////////////////////////////////////
// Precondition: no channel should be active
//
void DabRadio::start_channel(const QString & iChannel, const u32 iFastSelectSId /*= 0*/)
{
  const i32 tunedFrequencyHz = mChannel.realChannel ? mBandHandler.get_frequency_Hz(iChannel) : mpInputDevice->getVFOFrequency();
  LOG("channel starts ", iChannel);
  mSpectrumViewer.show_nominal_frequency_MHz((f32)tunedFrequencyHz / 1'000'000.0f);
  mpInputDevice->resetBuffer();
  mServiceList.clear();
  mpInputDevice->restartReader(tunedFrequencyHz);
  mChannel.clean_channel();
  mChannel.channelName = (mChannel.realChannel ? iChannel : "File");
  mChannel.nominalFreqHz = tunedFrequencyHz;

  mpServiceListHandler->set_selector_channel_only(mChannel.channelName);

  STiiDataEntry theTransmitter;
  if (mpHttpHandler != nullptr)
  {
    theTransmitter.latitude = 0;
    theTransmitter.longitude = 0;
    mpHttpHandler->putData(MAP_RESET, &theTransmitter, "", 0, 0, 0, false);
  }
  mTransmitterIds.clear();

  usleep(250'000); // wait for the reader to start as some LOs on some devices need some more time to swing-in (TODO: better reset FIB decoder later?)

  enable_ui_elements_for_safety(!mIsScanning);

  mpDabProcessor->start(); // resets also the FIB decoder, next command must follow then

  if (iFastSelectSId > 0)
  {
    mpDabProcessor->get_fib_decoder()->set_SId_for_fast_audio_selection(iFastSelectSId);
  }

  if (!mIsScanning)
  {
    Settings::Main::varChannel.write(iChannel);
    mEpgTimer.start(cEpgTimeoutMs);
  }
}

// Apart from stopping the reader, a lot of administration is to be done.
void DabRadio::stop_channel()
{
  if (mpInputDevice == nullptr) // should not happen
  {
    return;
  }

  stop_ETI_handler();

  LOG("channel stops ", mChannel.channelName);
  stop_services(true);

  stop_source_dumping();

  emit signal_stop_audio();

  _show_epg_label(false);

  if (mpContentTable != nullptr)
  {
    mpContentTable->hide();
    mpContentTable.reset();
  }

  // note framedumping - if any - was already stopped
  // ficDumping - if on - is stopped here
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

  _show_pause_slide();

  mChannelTimer.stop();
  mChannel.clean_channel();
  mChannel.targetPos = cf32(0, 0);
  mChannel.SId_next = 0;

  if (mpHttpHandler != nullptr)
  {
    STiiDataEntry theTransmitter;
    theTransmitter.latitude = 0;
    theTransmitter.longitude = 0;
    mpHttpHandler->putData(MAP_RESET, &theTransmitter, "", 0, 0, 0, false);
  }

  ui->transmitter_country->setText("");
  ui->transmitter_coordinates->setText("");

  enable_ui_elements_for_safety(false);  // hide some buttons
  QCoreApplication::processEvents();

  // no processing left at this time
  usleep(1000);    // may be handling pending signals?
  // mChannel.curPrimaryService.ServiceStarted = false;
  // mChannel.nextService.dataSetIsValid = false;

  // all stopped, now look at the GUI elements
  ui->progBarFicError->setValue(0);
  // the visual elements related to service and channel
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
// next- and previous channel buttons
/////////////////////////////////////////////////////////////////////////

void DabRadio::_slot_handle_channel_selector(const QString & channel)
{
  if (!mIsRunning.load())
  {
    return;
  }

  LOG("select channel ", channel.toUtf8().data());
  stop_scanning();
  stop_channel();
  start_channel(channel);
}

////////////////////////////////////////////////////////////////////////
//
// scanning
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
  mChannelTimer.stop();
  mEpgTimer.stop();
  connect(mpDabProcessor.get(), &DabProcessor::signal_no_signal_found, this, &DabRadio::slot_no_signal_found);
  stop_channel();

  mScanResult = {};

  const i32 cc = mBandHandler.firstChannel();
  LOG("scanning starts with ", QString::number(cc));
  mIsScanning.store(true);

  mpServiceListHandler->set_data_mode(ServiceListHandler::EDataMode::Permanent);
  mpServiceListHandler->delete_table(false);
  mpServiceListHandler->create_new_table();

  mpDabProcessor->set_scan_mode(true); // avoid MSC activities
  //  To avoid reaction of the system on setting a different value:
  _update_channel_selector(cc);
  ui->lblDynLabel->setText(_get_scan_message(false));
  ui->btnScanning->start_animation();
  mChannelTimer.start(cChannelTimeoutMs);

  start_channel(ui->cmbChannelSelector->currentText());
}

// stop_scanning is called
// 1. when the scanbutton is touched during scanning
// 2. on user selection of a service or a channel select
// 3. on device selection
// 4. on handling a reset
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

// If the ofdm processor has waited - without success - for a period of N frames to get a start of a synchronization,
// it sends a signal to the GUI handler If "scanning" is "on" we hop to the next frequency on the list.
// Also called as a result of timeout on channelTimer.
void DabRadio::slot_no_signal_found()
{
  mChannelTimer.stop();

  if (mIsRunning.load() && mIsScanning.load())
  {
    i32 cc = ui->cmbChannelSelector->currentIndex();

    stop_channel();

    cc = mBandHandler.nextChannel(cc);

    // qInfo() << "going to channel" << cc;

    if (cc >= ui->cmbChannelSelector->count())
    {
      stop_scanning();
    }
    else
    {
      // To avoid reaction of the system on setting a different value:
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

// In those case we are sure not to have an operating dabProcessor, we hide some buttons
void DabRadio::enable_ui_elements_for_safety(const bool iEnable)
{
  mConfig.dumpButton->setEnabled(iEnable);
  mConfig.etiButton->setEnabled(iEnable);
  ui->btnToggleFavorite->setEnabled(iEnable);
  ui->btnPrevService->setEnabled(iEnable);
  ui->btnNextService->setEnabled(iEnable);
  ui->cmbChannelSelector->setEnabled(iEnable);
  ui->btnFib->setEnabled(iEnable);
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

void DabRadio::_update_channel_selector(const QString & iChannel) const
{
  if (iChannel != "")
  {
    i32 k = ui->cmbChannelSelector->findText(iChannel);
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

void DabRadio::_update_channel_selector(i32 index)
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
// External configuration items				//////

//-------------------------------------------------------------------------
//------------------------------------------------------------------------
//
// if configured, the interpretation of the EPG data starts automatically,
// the servicenames of an SPI/EPG service may differ from one country
// to another
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

  // TODO: make this EPG true!
  for (const auto & serv: mServiceList)
  {
    // if (serv.name.contains("-EPG ", Qt::CaseInsensitive) ||
    //     serv.name.contains(" EPG   ", Qt::CaseInsensitive) ||
    //     serv.name.contains("Spored", Qt::CaseInsensitive) ||
    //     serv.name.contains("NivaaEPG", Qt::CaseInsensitive) ||
    //     serv.name.contains("SPI", Qt::CaseSensitive) ||
    //     serv.name.contains("BBC Guide", Qt::CaseInsensitive) ||
    //     serv.name.contains("EPG_", Qt::CaseInsensitive) ||
    //     serv.name.startsWith("EPG ", Qt::CaseInsensitive))
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
        LOG("hidden service started ", serv.serviceLabel);
        _show_epg_label(true);
        qCDebug(sLogRadioInterface()) << "Starting hidden service " << serv.serviceLabel;
        mpDabProcessor->set_data_channel(pd, mpDataBuffer, EProcessFlag::Primary);  // which ProcessFlag?
        SDabService s;
        // s.channel = pd.channel;
        // s.serviceLabel = pd.serviceName;
        s.SId = pd.SId;
        s.SCIdS = pd.SCIdS;
        s.SubChId = pd.SubChId;
        // s.fd = nullptr;
        mChannel.curSecondaryServiceVec.emplace_back(s);
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

u32 DabRadio::_extract_epg(const QString& iName, const std::vector<SServiceId> & iServiceList, u32 ensembleId)
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

void DabRadio::slot_set_epg_data(i32 SId, i32 theTime, const QString & theText, const QString & theDescr)
{
  if (mpDabProcessor != nullptr)
  {
    // TODO: check EPG
    // mpDabProcessor->get_fib_decoder()->set_epg_data(SId, theTime, theText, theDescr);
  }
}

void DabRadio::_slot_handle_time_table()
{
  i32 epgWidth = 70; // comes only from ini file formerly
  if (/*!mChannel.curPrimaryService.ServiceStarted ||*/ !mChannel.curPrimaryService.isAudio)
  {
    return;
  }

  mpTimeTable->clear();

  if (epgWidth < 50)
  {
    epgWidth = 50;
  }
  // TODO: check EPG
  // std::vector<SEpgElement> res = mpDabProcessor->get_fib_decoder()->find_epg_data(mChannel.currentService.SId);
  // for (const auto & element: res)
  // {
  //   mpTimeTable->addElement(element.theTime, epgWidth, element.theText, element.theDescr);
  // }
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

void DabRadio::LOG(const QString & a1, const QString & a2)
{
  if (mpLogFile == nullptr)
  {
    return;
  }

  QString theTime;
  if (Settings::Config::cbUseUtcTime.read().toBool())
  {
    theTime = _conv_to_time_str(mUTC.year, mUTC.month, mUTC.day, mUTC.hour, mUTC.minute);
  }
  else
  {
    theTime = QDateTime::currentDateTime().toString();
  }

  fprintf(mpLogFile, "at %s: %s %s\n", theTime.toUtf8().data(), a1.toUtf8().data(), a2.toUtf8().data());
}

void DabRadio::slot_handle_logger_button(i32)
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
  _check_coordinates();
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
// ensure that we only get a handler if we have a start location
void DabRadio::_slot_handle_http_button()
{
  if (real(mChannel.localPos) == 0 || imag(mChannel.localPos) == 0)
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
    mpHttpHandler.reset(new HttpHandler(this, mapPort, browserAddress, mChannel.localPos, mapFile, Settings::Config::cbManualBrowserStart.read().toBool()));
    mMaxDistance = -1;

    if (mpHttpHandler != nullptr)
    {
      _set_http_server_button(true);
    }
  }
  else
  {
    slot_http_terminate();
  }
}

void DabRadio::slot_http_terminate()
{
  mpHttpHandler.reset();
  _set_http_server_button(false);
}

void DabRadio::_show_pause_slide()
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
// Experimental: handling eti
// writing an eti file and scanning seems incompatible to me, so
// that is why I use the button, originally named "btnScanning"
// for eti when eti is prepared.
// Preparing eti is with a checkbox on the configuration widget
//
/////////////////////////////////////////////////////////////////////////

void DabRadio::_slot_handle_eti_button()
{
  if (!mIsRunning.load() || mIsScanning.load())
  {
    return;
  }
  if (mpDabProcessor == nullptr)
  {  // should not happen
    return;
  }

  if (mChannel.etiActive)
  {
    stop_ETI_handler();
  }
  else
  {
    start_etiHandler();
  }
}

void DabRadio::stop_ETI_handler()
{
  if (!mChannel.etiActive)
  {
    return;
  }

  mpDabProcessor->stop_eti_generator();
  mChannel.etiActive = false;

  LOG("etiHandler stopped", "");
  emphasize_pushbutton(mConfig.etiButton, false);

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
    emphasize_pushbutton(mConfig.etiButton, true);
  }
}

void DabRadio::slot_test_slider(i32 iVal) // iVal 0..1000
{
  emit signal_test_slider_changed(iVal);
}

QStringList DabRadio::_get_soft_bit_gen_names() const
{
  QStringList sl;

  // ATTENTION: use same sequence as in ESoftBitType
  sl << "Soft decision 1"; // ESoftBitType::SOFTDEC1
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
  ui->btnFib->setIconSize(QSize(24, 24));
  ui->btnFib->setFixedSize(QSize(32, 32));
  ui->btnTii->setIconSize(QSize(24, 24));
  ui->btnTii->setFixedSize(QSize(32, 32));
  ui->btnCir->setIconSize(QSize(24, 24));
  ui->btnCir->setFixedSize(QSize(32, 32));
  ui->btnScanning->setIconSize(QSize(24, 24));
  ui->btnScanning->setFixedSize(QSize(32, 32));
  ui->btnScanning->init(":res/icons/scan24.png", 3, 1);
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

void DabRadio::_check_coordinates() const
{
  const f32 local_lat = Settings::Config::varLatitude.read().toFloat();
  const f32 local_lon = Settings::Config::varLongitude.read().toFloat();
  emphasize_pushbutton(mConfig.set_coordinatesButton, local_lat == 0 || local_lon == 0); // it is very unlikely that an exact zero is a valid coordinate
}

void DabRadio::_get_last_service_from_config()
{
  mChannel.channelName = Settings::Main::varPresetChannel.read().toString();
  mChannel.SId_next    = Settings::Main::varPresetSId.read().toUInt();

  // if (channel.isEmpty())
  // {
  //   return;
  // }
  //
  // mChannel.channelName
  //
  // ioDabService.channel = channel;
  // // ioDabService.serviceLabel.clear();
  // ioDabService.SId = SId;
  // ioDabService.SCIdS = 0;
  // ioDabService.dataSetIsValid = true;

  // if (QString presetName = Settings::Main::varPresetName.read().toString();
  //   !presetName.isEmpty())
  // {
  //   const QStringList ss = presetName.split(":");
  //
  //   if (ss.size() == 2)
  //   {
  //     ioDabService.channel = ss.at(0);
  //     ioDabService.serviceLabel = ss.at(1);
  //   }
  //   else
  //   {
  //     ioDabService.channel = "";
  //     ioDabService.serviceLabel = presetName;
  //   }
  //
  //   ioDabService.SId = 0;
  //   ioDabService.SCIdS = 0;
  //   ioDabService.dataSetIsValid = true;
  // }
}

void DabRadio::_get_local_position_from_config(cf32 & oLocalPos) const
{
  const f32 local_lat = Settings::Config::varLatitude.read().toFloat();
  const f32 local_lon = Settings::Config::varLongitude.read().toFloat();
  oLocalPos = cf32(local_lat, local_lon);
}

void DabRadio::_initialize_ui_buttons()
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
  ui->btnFib->setStyleSheet(get_bg_style_sheet({ 220, 100, 152 }));
  ui->btnTii->setStyleSheet(get_bg_style_sheet({ 255, 100, 0 }));
  ui->btnCir->setStyleSheet(get_bg_style_sheet({ 220, 37, 192 }));

  _set_http_server_button(false);

  // only the queued call will consider the button size
  QMetaObject::invokeMethod(this, "_slot_update_mute_state", Qt::QueuedConnection, Q_ARG(bool, false));
  QMetaObject::invokeMethod(this, "_slot_favorite_changed", Qt::QueuedConnection, Q_ARG(bool, false));
  QMetaObject::invokeMethod(this, &DabRadio::_slot_set_static_button_style, Qt::QueuedConnection);
}

void DabRadio::_initialize_status_info()
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

void DabRadio::_initialize_dynamic_label()
{
  ui->lblDynLabel->setTextFormat(Qt::RichText);
  ui->lblDynLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  ui->lblDynLabel->setOpenExternalLinks(true);
}

void DabRadio::_initialize_thermo_peak_levels()
{
  ui->thermoPeakLevelLeft->setValue(-40.0);
  ui->thermoPeakLevelRight->setValue(-40.0);
  ui->thermoPeakLevelLeft->setFillBrush(QColor(0x6F70EF));
  ui->thermoPeakLevelRight->setFillBrush(QColor(0x6F70EF));
  ui->thermoPeakLevelLeft->setBorderWidth(0);
  ui->thermoPeakLevelRight->setBorderWidth(0);
}

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

void DabRadio::_initialize_paths()
{
  mPicturesPath = Settings::Config::varPicturesPath.read().toString();
  // mPicturesPath = check_and_create_dir(mPicturesPath);

  mMotPath = Settings::Config::varMotPath.read().toString();
  // mMotPath = check_and_create_dir(mMotPath);

  mEpgPath = Settings::Config::varEpgPath.read().toString();
  // mEpgPath = check_and_create_dir(mEpgPath);
}

void DabRadio::_initialize_epg()
{
  mpEpgHandler.reset(new CEPGDecoder);
  mpEpgProcessor.reset(new EpgDecoder);

  connect(mpEpgProcessor.get(), &EpgDecoder::signal_set_epg_data, this, &DabRadio::slot_set_epg_data);

  // timer for autostart epg service
  mEpgTimer.setSingleShot(true);
  connect(&mEpgTimer, &QTimer::timeout, this, &DabRadio::slot_epg_timer_timeout);

  _show_epg_label(false);
}

void DabRadio::_initialize_time_table()
{
  mpTimeTable.reset(new TimeTableHandler(this));
  mpTimeTable->hide();
}

void DabRadio::_initialize_tii_file()
{
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
}

void DabRadio::_initialize_version_and_copyright_info()
{
  ui->lblVersion->setText(QString("V" + mVersionStr));
  ui->lblCopyrightIcon->setToolTip(get_copyright_text());
  ui->lblCopyrightIcon->setTextInteractionFlags(Qt::TextBrowserInteraction);
  ui->lblCopyrightIcon->setOpenExternalLinks(true);
}

void DabRadio::_initialize_band_handler()
{
  mBandHandler.setupChannels(ui->cmbChannelSelector, BAND_III);
  const QString skipFileName = Settings::Config::varSkipFile.read().toString();
  mBandHandler.setup_skipList(skipFileName);
}

void DabRadio::_initialize_and_start_timers()
{
  // The displaytimer is there to show the number of seconds running and handle - if available - the tii data
  mDisplayTimer.setInterval(cDisplayTimeoutMs);
  connect(&mDisplayTimer, &QTimer::timeout, this, &DabRadio::_slot_update_time_display);
  mDisplayTimer.start(cDisplayTimeoutMs);

  // timer for scanning
  mChannelTimer.setSingleShot(true);
  mChannelTimer.setInterval(cChannelTimeoutMs);
  connect(&mChannelTimer, &QTimer::timeout, this, &DabRadio::_slot_channel_timeout);

  mClockResetTimer.setSingleShot(true);
  connect(&mClockResetTimer, &QTimer::timeout, [this](){ _set_clock_text(); });

  mTiiIndexCntTimer.setSingleShot(true);
  mTiiIndexCntTimer.setInterval(cTiiIndexCntTimeoutMs);
}

void DabRadio::_initialize_device_selector()
{
  mConfig.deviceSelector->addItems(mDeviceSelector.get_device_name_list());

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

void DabRadio::_show_or_hide_windows_from_config()
{
  if (Settings::SpectrumViewer::varUiVisible.read().toBool())
  {
    mSpectrumViewer.show();
  }

  if (Settings::TechDataViewer::varUiVisible.read().toBool())
  {
    mpTechDataWidget->show();
  }

  mShowTiiListWindow = Settings::TiiViewer::varUiVisible.read().toBool();
}

