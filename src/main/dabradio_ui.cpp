//
// Created by tomneda on 2025-10-24.
//
#include "dabradio.h"
#include "ui_dabradio.h"
#include "service-list-handler.h"
#include "setting-helper.h"
#include "techdata.h"
#include "coordinates.h"
#include "compass_direction.h"
#include "ITU_Region_1.h"
#include "copyright_info.h"
#include "map-http-server.h"

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

template void DabRadio::_set_status_info_status<bool>(StatusInfoElem<bool> &, bool);
template void DabRadio::_set_status_info_status<u32>(StatusInfoElem<u32> &, u32);
template void DabRadio::_set_status_info_status<i32>(StatusInfoElem<i32> &, i32);

void DabRadio::_emphasize_pushbutton(QPushButton * const ipPB, const bool iEmphasize) const
{
  ipPB->setStyleSheet(iEmphasize ? "background-color: #AE2B05; color: #EEEE00; font-weight: bold;" : "");
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
  ui->btnOpenPicFolder->setStyleSheet(get_bg_style_sheet({ 220, 180, 45 }));

  _set_http_server_button(EHttpButtonState::Off);
  slot_handle_mot_saving_selector(mConfig.cmbMotObjectSaving->currentIndex());

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
  auto setup_thermo_peak_level_widget = [](QwtThermo * ipThermo)
  {
    ipThermo->setValue(-40.0);
    ipThermo->setBorderWidth(0);
    ipThermo->setStyleSheet("background-color: black;");

    QwtLinearColorMap * pColorMap = new QwtLinearColorMap();
    pColorMap->setColorInterval(QColor(0, 100, 200), QColor(255, 0, 0));
    pColorMap->addColorStop(0.72727273, QColor(200, 200, 0)); // -6dBFS
    pColorMap->addColorStop(0.45454545, QColor(0, 200, 0));   // -12dBFS
    ipThermo->setColorMap(pColorMap);
  };

  setup_thermo_peak_level_widget(ui->thermoPeakLevelLeft);
  setup_thermo_peak_level_widget(ui->thermoPeakLevelRight);
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

void DabRadio::_initialize_version_and_copyright_info()
{
  const QString crt = get_copyright_text();
  ui->lblVersion->setText(QString("V" + mVersionStr));
  ui->lblVersion->setToolTip(crt);
  ui->lblVersion->setCursor(Qt::WaitCursor);
  ui->lblCopyrightIcon->setToolTip(crt);
  ui->lblCopyrightIcon->setTextInteractionFlags(Qt::TextBrowserInteraction);
  ui->lblCopyrightIcon->setOpenExternalLinks(true);
  ui->lblCopyrightIcon->setCursor(Qt::WaitCursor);
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

void DabRadio::set_favorite_button_style()
{
  ui->btnToggleFavorite->setIcon(QIcon(mCurFavoriteState ? ":res/icons/starfilled24.png" : ":res/icons/starempty24.png"));
  ui->btnToggleFavorite->setIconSize(QSize(24, 24));
  ui->btnToggleFavorite->setFixedSize(QSize(32, 32));
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

void DabRadio::cleanup_ui()
{
  ui->progBarFicError->setValue(0);
  ui->progBarAudioBuffer->setValue(0);
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

void DabRadio::slot_handle_journaline_viewer_closed(i32 iSubChannel)
{
  if (mpDabProcessor)
  {
    mpDabProcessor->stop_service(iSubChannel, EProcessFlag::Primary);
    mpDabProcessor->stop_service(iSubChannel, EProcessFlag::Secondary);
  }
}

void DabRadio::slot_handle_tii_viewer_closed()
{
  if (mShowTiiListWindow)  // should be always true in this situation
  {
    _slot_handle_tii_button();
  }
}

void DabRadio::_slot_handle_favorite_button(bool /*iClicked*/)
{
  mCurFavoriteState = !mCurFavoriteState;
  set_favorite_button_style();
  mpServiceListHandler->set_favorite_state(mCurFavoriteState);
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
  ui->btnOpenPicFolder->setIconSize(QSize(24, 24));
  ui->btnOpenPicFolder->setFixedSize(QSize(32, 32));
  ui->btnScanning->setIconSize(QSize(24, 24));
  ui->btnScanning->setFixedSize(QSize(32, 32));
  ui->btnScanning->init(":res/icons/scan24.png", 3, 1);
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

void DabRadio::clean_screen()
{
  if (!mIsScanning) ui->lblDynLabel->setText("");
  ui->serviceLabel->setText("");
  ui->programTypeLabel->setText("");
  mpTechDataWidget->cleanUp();
  _reset_status_info();
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

    const STiiDataEntry * pTr = mTiiHandler.get_transmitter_data((mChannel.realChannel ? mChannel.channelName : "any"), mChannel.Eid, tii.mainId, tii.subId);
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
      mpHttpHandler->add_transmitter_location_entry(MAP_NORM_TRANS, pTr, theTime.toString(Qt::TextDate),
                                                    bd.strength_dB, bd.distance_km, bd.corner_deg, bd.isNonEtsiPhase);
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
  // qCDebug(sLogRadioInterface) << Q_FUNC_INFO << iEId << iEnsName << iEnsNameShort;

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
  if (mpFibContentTable != nullptr)
  {
    const bool isShown = mpFibContentTable->is_visible();
    mpFibContentTable->hide();
    mpFibContentTable.reset();
    if (isShown) return;
  }

  i32 numCols = 0;
  const QStringList s = mpDabProcessor->get_fib_decoder()->get_fib_content_str_list(numCols); // every list entry is one line
  mpFibContentTable.reset(new FibContentTable(this, &Settings::Storage::instance(), mChannel.channelName, numCols));
  connect(mpFibContentTable.get(), &FibContentTable::signal_go_service_id, this, &DabRadio::slot_handle_fib_content_selector);

  for (const auto & sl : s)
  {
    mpFibContentTable->add_line(sl);
  }
  mpFibContentTable->show();
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

// In those case we are sure not to have an operating dabProcessor, we hide some buttons
void DabRadio::enable_ui_elements_for_safety(const bool iEnable) const
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

void DabRadio::_set_http_server_button(const EHttpButtonState iHttpServerState)
{
  switch (iHttpServerState)
  {
  case EHttpButtonState::Off:
    ui->btnHttpServer->setStyleSheet(get_bg_style_sheet(0x45bb24));
    ui->btnHttpServer->setEnabled(true);
    break;
  case EHttpButtonState::On:
    ui->btnHttpServer->setStyleSheet(get_bg_style_sheet(0xf97903));
    ui->btnHttpServer->setEnabled(true);
    break;
  case EHttpButtonState::Waiting:
    // ui->btnHttpServer->setStyleSheet(get_bg_style_sheet(0x777777));
    ui->btnHttpServer->setEnabled(false);
    break;
  }

  ui->btnHttpServer->setFixedSize(QSize(32, 32));
}
