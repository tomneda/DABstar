//
// Created by tomneda on 2025-10-24.
//
#include "dabradio.h"
#include "ui_dabradio.h"
#include "service-list-handler.h"
#include "setting-helper.h"
#include "techdata.h"
#include "itu-regions.h"
#include "copyright_info.h"
#include "map-http-server.h"
#include "spectrum-viewer.h"
#include "cir-viewer.h"
#include "configuration.h"
#include "gui-helpers.h"
#include "audio_manager.h"
#include "mot-content-types.h"
#include "mot_slide_progress.h"

template<typename T>
void DabRadio::_add_status_label_elem(StatusInfoElem<T> & ioElem, const u32 iColor, const QString & iName, const QString & iToolTip)
{
  ioElem.colorOn = iColor;
  ioElem.colorOff = 0x444444;

  // invalidate cache
  if constexpr (std::is_same_v<T, const char *>)
    ioElem.value = nullptr;
  else
    ioElem.value = !T();

  ioElem.pLbl = new QLabel(this);

  ioElem.pLbl->setObjectName("lbl" + iName);
  ioElem.pLbl->setText(iName); // important for bool elements
  ioElem.pLbl->setToolTip(iToolTip);

  QFont font = ioElem.pLbl->font();
  font.setBold(true);
  // font.setWeight(QFont::Thin);
  ioElem.pLbl->setFont(font);

  ui->layoutStatus->addWidget(ioElem.pLbl);
}

template<typename T>
void DabRadio::_set_status_info_status(StatusInfoElem<T> & iElem, T const iValue) const
{
  if constexpr (std::is_same_v<T, const char *>)
  {
    if (iValue == nullptr)
    {
      iElem.pLbl->setStyleSheet("QLabel { color: " + iElem.colorOff.name() + " }");
      iElem.pLbl->setText("--- --- ---"); // only for protection level used currently
    }
    else
    {
      iElem.pLbl->setStyleSheet("QLabel { color: " + iElem.colorOn.name() + " }");
      iElem.pLbl->setText(iValue);
    }
  }
  else if (iElem.value != iValue)
  {
    iElem.value = iValue;
    iElem.pLbl->setStyleSheet(iElem.value ? "QLabel { color: " + iElem.colorOn.name() + " }"
                                          : "QLabel { color: " + iElem.colorOff.name() + " }");

    // trick a bit: i32 are bit rates, u32 are sample rates
    if constexpr (std::is_same_v<T, i32>)
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
    else if constexpr (std::is_same_v<T, u32>)
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

template void DabRadio::_set_status_info_status<bool>(StatusInfoElem<bool> &, bool) const;
template void DabRadio::_set_status_info_status<u32>(StatusInfoElem<u32> &, u32) const;
template void DabRadio::_set_status_info_status<i32>(StatusInfoElem<i32> &, i32) const;
template void DabRadio::_set_status_info_status<const char *>(StatusInfoElem<const char *> &, const char * const) const;

void DabRadio::_reset_status_info(StatusInfo & ioStatusInfo) const
{
  _set_status_info_status(ioStatusInfo.InpBitRate, (i32)0);
  _set_status_info_status(ioStatusInfo.ProtLevel, (const char *)nullptr);
  _set_status_info_status(ioStatusInfo.OutSampRate, (u32)0);
  _set_status_info_status(ioStatusInfo.Stereo, false);
  _set_status_info_status(ioStatusInfo.MOT, false);
  _set_status_info_status(ioStatusInfo.EPG, false);
  _set_status_info_status(ioStatusInfo.SBR, false);
  _set_status_info_status(ioStatusInfo.PS, false);
  _set_status_info_status(ioStatusInfo.Announce, false);
  _set_status_info_status(ioStatusInfo.RsError, false);
  _set_status_info_status(ioStatusInfo.CrcError, false);
}

void DabRadio::_initialize_ui_elements()
{
  // handle device/file player radio button
  QButtonGroup * grpSLS = new QButtonGroup(this);
  grpSLS->addButton(ui->rbDevicePlayer, (int)EServiceListSrc::DEVICE_PLAYER);
  grpSLS->addButton(ui->rbFilePlayer, (int)EServiceListSrc::FILE_PLAYER);
  grpSLS->setExclusive(true);

  connect(grpSLS, &QButtonGroup::idClicked, this, &DabRadio::_slot_service_list_src_change);

  const i32 deviceFilePlayerId = Settings::Main::varDeviceFilePlayerId.read().toInt();
  ui->rbDevicePlayer->setChecked(deviceFilePlayerId == (int)EServiceListSrc::DEVICE_PLAYER);
  ui->rbFilePlayer->setChecked(deviceFilePlayerId == (int)EServiceListSrc::FILE_PLAYER);

  // setup styles and colors of controls
  ui->btnMuteAudio->setStyleSheet(get_bg_style_sheet(0xFF3C3C));       // red      — mute (warning)
  ui->btnDeviceWidget->setStyleSheet(get_bg_style_sheet(0x38A8C0));    // cyan     — hardware device
  ui->configButton->setStyleSheet(get_bg_style_sheet(0x509B50));       // green    — settings
  ui->btnTechDetails->setStyleSheet(get_bg_style_sheet(0xD8C040));     // yellow   — info panel
  ui->btnSpectrumScope->setStyleSheet(get_bg_style_sheet(0x9050C8));   // purple   — spectrum view
  ui->btnPrevService->setStyleSheet(get_bg_style_sheet(0xC86128));     // orange   — navigate prev
  ui->btnNextService->setStyleSheet(get_bg_style_sheet(0xC86128));     // orange   — navigate next
  ui->btnTargetService->setStyleSheet(get_bg_style_sheet(0x308878));   // teal     — target service
  ui->btnToggleFavorite->setStyleSheet(get_bg_style_sheet(0x6464FF));  // blue     — favourite
  ui->btnFib->setStyleSheet(get_bg_style_sheet(0xB84878));             // rose     — FIB data
  ui->btnTii->setStyleSheet(get_bg_style_sheet(0x6868A8));             // lavender — TII
  ui->btnCir->setStyleSheet(get_bg_style_sheet(0xB030A0));             // magenta  — CIR view
  ui->btnEnsembleList->setStyleSheet(get_bg_style_sheet(0x4848C8));    // indigo   — ensemble list
  ui->btnOpenPicFolder->setStyleSheet(get_bg_style_sheet(0xDCB42D));   // gold     — picture folder

  ui->cmbDeviceSelect->setStyleSheet(get_combo_style_sheet(0x2E8EA3)); // cyan     — device (matches btnDeviceWidget)
  ui->cmbTiiList->setStyleSheet(get_combo_style_sheet(0x6868A8));      // slate blue — TII transmitter list

  _set_http_server_button(EHttpButtonState::Off);
  mpEpgMotHandler->slot_handle_mot_saving_selector(mpConfig->cmbMotObjectSaving5->currentIndex());

  // only the queued call will consider the button size
  QMetaObject::invokeMethod(this, "_slot_update_mute_state", Qt::QueuedConnection, Q_ARG(bool, false));
  QMetaObject::invokeMethod(this, "_slot_favorite_changed", Qt::QueuedConnection, Q_ARG(bool, false));
  QMetaObject::invokeMethod(this, &DabRadio::_slot_set_static_button_style, Qt::QueuedConnection);
}

void DabRadio::_initialize_status_info()
{
  const char * const protLevToolTip =
    R"(Protection level<br>)"
    R"(<table>)"
    R"( <thead>)"
    R"(  <tr><th>ProtLevel</th><th>CodeRate</th></tr>)"
    R"( </thead>)"
    R"( <tbody>)"
    R"(  <tr><td>EEP 1-A</td><td>1/4 (= 2/8)</td></tr>)"
    R"(  <tr><td>EEP 2-A</td><td>3/8 (= 3/8)</td></tr>)"
    R"(  <tr><td>EEP 3-A</td><td>1/2 (= 4/8)</td></tr>)"
    R"(  <tr><td>EEP 4-A</td><td>3/4 (= 6/8)</td></tr>)"
    R"(  <tr><td>EEP 1-B</td><td>4/9 (~ 7/16)</td></tr>)"
    R"(  <tr><td>EEP 2-B</td><td>4/7 (~ 9/16)</td></tr>)"
    R"(  <tr><td>EEP 3-B</td><td>4/6 (~ 11/16)</td></tr>)"
    R"(  <tr><td>EEP 4-B</td><td>4/5 (~ 13/16)</td></tr>)"
    R"(  <tr><td>UEP 1</td><td>1/3 (~ 5/15)</td></tr>)"
    R"(  <tr><td>UEP 2</td><td>2/5 (~ 6/15)</td></tr>)"
    R"(  <tr><td>UEP 3</td><td>1/2 (~ 8/15)</td></tr>)"
    R"(  <tr><td>UEP 4</td><td>3/5 (~ 9/15)</td></tr>)"
    R"(  <tr><td>UEP 5</td><td>3/4 (~ 11/15)</td></tr>)"
    R"( </tbody>)"
    R"(</table>)";

  ui->layoutStatus->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

  _add_status_label_elem(mStatusInfo.ProtLevel,   0x6AB45B, "ProtLevel",   protLevToolTip);
  _add_status_label_elem(mStatusInfo.InpBitRate,  0x40c6db, "InpBitRate",  "Input bit-rate of the audio decoder");
  _add_status_label_elem(mStatusInfo.OutSampRate, 0xDE9769, "OutSampRate", "Output sample-rate of the audio decoder");
  _add_status_label_elem(mStatusInfo.Stereo,      0xf2c629, "Stereo",      "Stereo");
  _add_status_label_elem(mStatusInfo.PS,          0xf2c629, "PS",          "Parametric Stereo");
  _add_status_label_elem(mStatusInfo.SBR,         0xf2c629, "SBR",         "Spectral Band Replication");
  _add_status_label_elem(mStatusInfo.MOT,         0x7F8CFF, "MOT",         "Multimedia Object Transfer<br>for slide show");
  _add_status_label_elem(mStatusInfo.EPG,         0xf2c629, "EPG",         "Electronic Program Guide");
  _add_status_label_elem(mStatusInfo.Announce,    0xf2c629, "ANN",         "Announcement");
  _add_status_label_elem(mStatusInfo.RsError,     0xFF5749, "RS",          "Reed Solomon Error occurred");
  _add_status_label_elem(mStatusInfo.CrcError,    0xFF5749, "CRC",         "CRC Error occurred");

  ui->layoutStatus->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

  _reset_status_info(mStatusInfo);
}

void DabRadio::_initialize_dynamic_label() const
{
  ui->lblDynLabel->setTextFormat(Qt::RichText);
  ui->lblDynLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  ui->lblDynLabel->setOpenExternalLinks(true);
}

void DabRadio::_initialize_device_selector(SChannelDescriptor & ioChannelDesc) const
{
  ui->cmbDeviceSelect->addItems(mDeviceSelector.get_device_name_list());

  const QString h = Settings::Main::varSdrDevice.read().toString();
  const i32 k = ui->cmbDeviceSelect->findText(h);

  if (k != -1)
  {
    ui->cmbDeviceSelect->setCurrentIndex(k);
    ioChannelDesc.set_device_name(h);
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


void DabRadio::_set_favorite_button_style()
{
  ui->btnToggleFavorite->setIcon(QIcon(mCurFavoriteState ? ":res/icons/starfilled24.png" : ":res/icons/starempty24.png"));
  ui->btnToggleFavorite->setIconSize(QSize(24, 24));
  ui->btnToggleFavorite->setFixedSize(QSize(32, 32));
}

QStringList DabRadio::_get_soft_bit_gen_names() const
{
  QStringList sl;

  // ATTENTION: use same sequence as in ESoftBitType
  sl << "Optimal 3";       // ESoftBitType::SOFTDEC1
  sl << "Optimal 2";       // ESoftBitType::SOFTDEC2
  sl << "Optimal 1";       // ESoftBitType::SOFTDEC3
  return sl;
}

void DabRadio::_cleanup_ui() const
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
      ui->lblDabTime->setStyleSheet(get_bg_style_sheet(0xff3c3c, 0xffffff));
      mClockActiveStyle = true;
    }
    ui->lblDabTime->setText(iText);
  }
  else
  {
    if (mClockActiveStyle)
    {
      ui->lblDabTime->setStyleSheet(get_bg_style_sheet(0x440000, 0x333333));
      mClockActiveStyle = false;
    }
    //ui->lblLocalTime->setText("YYYY-MM-DD hh:mm:ss");
    ui->lblDabTime->setText("0000-00-00 00:00:00");
  }
}

void DabRadio::_show_epg_label(const bool iShowLabel)
{
  _set_status_info_status(mStatusInfo.EPG, iShowLabel);
}

void DabRadio::_adapt_gui_for_device_or_file_play(const bool iPlayingDevice) const
{
#if 1
  ui->cmbDeviceSelect->setEnabled(iPlayingDevice);
  ui->cbAutoNextService->setEnabled(!iPlayingDevice);
#else
  mpConfig->dumpButton->setEnabled(iPlayingDevice);
  btnScanning->setEnabled(iPlayingDevice);
  mpConfig->setEnabled(iPlayingDevice);
  channelSelector->setEnabled(iPlayingDevice);
#endif
}

void DabRadio::slot_handle_journaline_viewer_closed(i32 iSubChannel)
{
  if (mpDabProcessor)
  {
    mpDabProcessor->stop_service(iSubChannel, EProcessFlag::Primary);
    mpDabProcessor->stop_service(iSubChannel, EProcessFlag::Secondary);
  }
}


void DabRadio::_slot_handle_favorite_button(bool /*iClicked*/)
{
  mCurFavoriteState = !mCurFavoriteState;
  _set_favorite_button_style();
  mpServiceListHandler->set_favorite_state(mCurFavoriteState);
}

void DabRadio::_slot_set_static_button_style()
{
  ui->btnPrevService->setIconSize(QSize(24, 24));
  ui->btnPrevService->setFixedSize(QSize(32, 32));
  ui->btnNextService->setIconSize(QSize(24, 24));
  ui->btnNextService->setFixedSize(QSize(32, 32));
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
  ui->btnFib->setFixedSize(QSize(32, 32));
  ui->btnTii->setFixedSize(QSize(32, 32));
  ui->btnCir->setFixedSize(QSize(32, 32));
  ui->btnEnsembleList->setFixedSize(QSize(32, 32));
  ui->btnOpenPicFolder->setIconSize(QSize(24, 24));
  ui->btnOpenPicFolder->setFixedSize(QSize(32, 32));
}

void DabRadio::_show_or_hide_windows_from_config()
{
  if (Settings::SpectrumViewer::varUiVisible.read().toBool())
  {
    mpSpectrumViewer->show();
  }

  if (Settings::TechDataViewer::varUiVisible.read().toBool())
  {
    mpTechDataWidget->show();
  }
}

void DabRadio::_slot_handle_config_button()
{
  mpConfig->setVisible(mpConfig->isHidden());
}

void DabRadio::slot_set_and_show_freq_corr_rf_Hz(const i32 iFreqCorrRF)
{
  if (mpInputDevice != nullptr && mChannelDesc.deferredData.nomFreqHz.has_value())
  {
    mpInputDevice->setVFOFrequency(mChannelDesc.deferredData.nomFreqHz.value() + iFreqCorrRF);
  }

  mpSpectrumViewer->show_freq_corr_rf_Hz(iFreqCorrRF);
}

void DabRadio::slot_show_freq_corr_bb_Hz(i32 iFreqCorrBB)
{
  mChannelDesc.deferredData.bbOffset = iFreqCorrBB;
  mpSpectrumViewer->show_freq_corr_bb_Hz(iFreqCorrBB);
}

void DabRadio::_get_ymd_from_mod_julian_date(i32 & oYear, i32 & oMonth, i32 & oDay, const i32 iMJD) const
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

QString DabRadio::_conv_to_time_str(i32 iYear, i32 iMonth, i32 iDay, i32 iHours, i32 iMinutes, i32 iSeconds) const
{
  QString t = QString::number(iYear).rightJustified(4, '0') + "-" +
              QString::number(iMonth).rightJustified(2, '0') + "-" +
              QString::number(iDay).rightJustified(2, '0') + "  " +
              QString::number(iHours).rightJustified(2, '0') + ":" +
              QString::number(iMinutes).rightJustified(2, '0');

  if (iSeconds >= 0)
  {
    t += ":" + QString::number(iSeconds).rightJustified(2, '0');
  }
  return t;
}

void DabRadio::slot_fib_time(const IFibDecoder::SUtcTimeSet & iFibTimeInfo)
{
  // convert to local time and adapt date if needed
  const i32 daysOffs = _get_local_time(mLocalTime.hour, mLocalTime.minute, iFibTimeInfo.Hour, iFibTimeInfo.Minute, iFibTimeInfo.LTO_Minutes);

  mUTC.hour = iFibTimeInfo.Hour;
  mUTC.minute = iFibTimeInfo.Minute;
  mUTC.second = mLocalTime.second = iFibTimeInfo.Second;

  _get_ymd_from_mod_julian_date(mUTC.year, mUTC.month, mUTC.day, iFibTimeInfo.ModJulianDate);

  if (daysOffs != 0) // save a bit calculation time if local date is same than UTC date
  {
    _get_ymd_from_mod_julian_date(mLocalTime.year, mLocalTime.month, mLocalTime.day, iFibTimeInfo.ModJulianDate + daysOffs);
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

void DabRadio::_clean_screen(StatusInfo & ioStatusInfo) const
{
  if (!mIsScanning) ui->lblDynLabel->setText("");
  ui->lblServiceName->setText("");
  ui->lblProgType->setText("");
  mpTechDataWidget->cleanUp();
  _reset_status_info(ioStatusInfo);
}

// called from the MP4 decoder
void DabRadio::slot_show_frame_errors(i32 iFrameErrors)
{
  if (!mIsChannelRunning.load())
  {
    return;
  }
  if (!mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->slot_show_frame_error_bar(iFrameErrors);
  }
}

// called from the MP4 decoder
void DabRadio::slot_show_rs_errors(i32 iRsErrors)
{
  if (!mIsChannelRunning.load())
  {    // should not happen
    return;
  }

  if (!mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->slot_show_rs_error_bar(iRsErrors);
  }
}

// called from the aac decoder
void DabRadio::slot_show_aac_errors(i32 iAacErrors)
{
  if (!mIsChannelRunning.load())
  {
    return;
  }

  if (!mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->slot_show_aac_error_bar(iAacErrors);
  }
}

// called from the ficHandler
void DabRadio::slot_show_fic_status(const i32 iSuccessPercent, const f32 iBER)
{
  if (!mIsChannelRunning.load())
  {
    return;
  }

  if (ui->progBarFicError->value() != iSuccessPercent)
  {
    if (iSuccessPercent < 85)
    {
      ui->progBarFicError->setStyleSheet("QProgressBar { color: #555555; } QProgressBar::chunk { background-color: #CC3333; }");
    }
    else
    {
      ui->progBarFicError->setStyleSheet("QProgressBar { color: #555555; } QProgressBar::chunk { background-color: #52A824; }");
    }

    ui->progBarFicError->setValue(iSuccessPercent);
  }

  if (!mpSpectrumViewer->is_hidden())
  {
    mpSpectrumViewer->show_fic_ber(iBER);
  }
}

// called from the PAD handler
void DabRadio::slot_show_label(const QString & iLabel)
{
  if (mIsChannelRunning.load())
  {
    ui->lblDynLabel->setStyleSheet("color: white");
    ui->lblDynLabel->setText(_convert_links_to_clickable(iLabel));
  }

  // if DL text is ON, some work is still to be done
  FILE * const pDlTextFile = mpEpgMotHandler->get_dl_text_file();
  if ((pDlTextFile == nullptr) || (mDynLabelCache.add_if_new(iLabel)))
  {
    return;
  }

  QDateTime theDateTime = QDateTime::currentDateTime();
  fprintf(pDlTextFile,
          "%s.%s %4d-%02d-%02d %02d:%02d:%02d  %s\n",
          mChannelDesc.get_fId_or_ch().toUtf8().data(),
          mCurPrimaryAudioService.serviceLabel.toUtf8().data(),
          mLocalTime.year,
          mLocalTime.month,
          mLocalTime.day,
          mLocalTime.hour,
          mLocalTime.minute,
          mLocalTime.second,
          iLabel.toUtf8().data());
}

void DabRadio::slot_set_stereo(const bool iStereo)
{
  _set_status_info_status(mStatusInfo.Stereo, iStereo);
}

void DabRadio::slot_show_tii(const std::vector<STiiResult> & iTiiList)
{
  mpTiiManager->show_tii(iTiiList);
}

void DabRadio::slot_show_spectrum(i32 /*amount*/) const
{
  if (!mIsChannelRunning.load())
  {
    return;
  }

  mpSpectrumViewer->show_spectrum(mpInputDevice->getVFOFrequency());
}

void DabRadio::slot_show_cir() const
{
  if (!mIsChannelRunning.load() || mpCirViewer->is_hidden())
  {
    return;
  }

  mpCirViewer->show_cir();
}

void DabRadio::slot_show_iq(i32 iAmount, f32 iAvg) const
{
  if (!mIsChannelRunning.load())
  {
    return;
  }

  mpSpectrumViewer->show_iq(iAmount, iAvg);
}

void DabRadio::slot_show_lcd_data(const OfdmDecoder::SLcdData & iQD)
{
  if (!mIsChannelRunning.load())
  {
    return;
  }

  // The SNR/MER could swing-in for a while, so check if it's within a reasonable range
  if (std::abs(iQD.SNR - mChannelDesc.snrLast) < 0.5f)
  {
    mChannelDesc.deferredData.snr = iQD.SNR;
  }

  if (std::abs(iQD.MER - mChannelDesc.merLast) < 0.5f)
  {
    mChannelDesc.deferredData.mer = iQD.MER;
  }

  mChannelDesc.snrLast = iQD.SNR;
  mChannelDesc.merLast = iQD.MER;

  if (!mpSpectrumViewer->is_hidden())
  {
    mpSpectrumViewer->show_lcd_data(iQD.CurOfdmSymbolNo, iQD.MER, iQD.TestData1, iQD.TestData2, iQD.MeanSigmaSqFreqCorr, iQD.SNR);
  }
}

void DabRadio::slot_show_digital_peak_and_rms_level(const f32 iLevelPeak, const f32 iLevelRms) const
{
  if (!mIsChannelRunning.load())
  {
    return;
  }

  mpSpectrumViewer->show_digital_peak_and_rms_level(iLevelPeak, iLevelRms);
}

void DabRadio::slot_pad_mot_progress(const i32 iPercent) const
{
  mpMotSlideProgress->do_progress(iPercent);
}

// called from the MP4 decoder
void DabRadio::slot_show_rs_corrections(const i32 iC, const i32 iEc)
{
  if (!mIsChannelRunning)
  {
    return;
  }

  _set_status_info_status(mStatusInfo.RsError, iC > 0);
  _set_status_info_status(mStatusInfo.CrcError, iEc > 0);

  if (!mpTechDataWidget->isHidden())
  {
    mpTechDataWidget->slot_show_rs_corrections(iC, iEc);
  }
}

// called from the DAB processor
void DabRadio::slot_show_clock_error(const f32 e) const
{
  if (!mIsChannelRunning.load())
  {
    return;
  }
  if (!mpSpectrumViewer->is_hidden())
  {
    mpSpectrumViewer->show_clock_error(e);
  }
}

// called from the phasesynchronizer
void DabRadio::slot_show_correlation(const f32 iThreshold, const QVector<i32> & iV)
{
  if (!mIsChannelRunning.load())
  {
    return;
  }

  mpSpectrumViewer->show_correlation(iThreshold, iV, mpTiiManager->get_transmitter_ids());
  mChannelDesc.nrTransmitters = iV.size();
}

[[maybe_unused]] static QString hex_to_str(i32 v)
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

  if (!mIsChannelRunning.load())
  {
    return;
  }

  QFont font = ui->lblEnsName->font();
  font.setPointSize(14);
  ui->lblEnsName->setFont(font);
  ui->lblEnsName->setText(iEnsName);
  // ui->lblEnsName->setText(iEnsName + QString("(") + hex_to_str(iEId) + QString(")"));

  mChannelDesc.ensembleName = iEnsName;
  mChannelDesc.ensembleNameShort = iEnsNameShort;
  mChannelDesc.Eid = iEId;
  mpEpgMotHandler->set_channel_info(mChannelDesc.get_fId_or_ch_descr(), iEnsName);
  mpTiiManager->set_channel_info(mChannelDesc.get_fId_or_ch(),
                                 mChannelDesc.Eid,
                                 mChannelDesc.deferredData.countryName.value_or("-"),
                                 mChannelDesc.get_type_info());
  if (!mChannelDesc.itu_code_decoded)
  {
    _check_for_itu_code();
  }
}

void DabRadio::_slot_handle_content_button()
{
  if (mpDabProcessor == nullptr)
  {
    return;
  }

  if (mpFibContentTable != nullptr)
  {
    const bool isShown = mpFibContentTable->is_visible();
    mpFibContentTable->hide();
    mpFibContentTable.reset();
    if (isShown) return;
  }

  i32 numCols = 0;
  const QStringList s = mpDabProcessor->get_fib_decoder()->get_fib_content_str_list(numCols); // every list entry is one line
  mpFibContentTable.reset(new FibContentTable(this, &Settings::Storage::instance(), mChannelDesc.get_fId_or_ch_descr(), numCols));
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
}

void DabRadio::_slot_handle_next_service_button()
{
  mpServiceListHandler->jump_entries(+1);
}

void DabRadio::_slot_handle_file_looped()
{
  if (!mIsScanning && mIsFileMode && ui->cbAutoNextService->isChecked())
  {
    _slot_handle_next_service_button();
  }
}

void DabRadio::_slot_handle_target_service_button()
{
  mpServiceListHandler->jump_entries(0);
}

// In those case we are sure not to have an operating dabProcessor, we hide some buttons
void DabRadio::_enable_ui_elements_for_safety(const bool iEnable) const
{
  mpConfig->dumpButton->setEnabled(iEnable);
  mpConfig->etiButton->setEnabled(iEnable);
  ui->btnToggleFavorite->setEnabled(iEnable);
  ui->btnPrevService->setEnabled(iEnable);
  ui->btnNextService->setEnabled(iEnable);
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
  mpAudioManager->slot_set_mute(mMutingActive);
}

void DabRadio::_set_http_server_button(const EHttpButtonState iHttpServerState) const
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

QString DabRadio::_get_scan_message(const bool iEndMsg) const
{
  QString s = "<span style='color:yellow;'>";
  s += (iEndMsg ? "Scan ended - Select a service on the left" : "Scanning " + mChannelDesc.get_type_info());
  s += "</span>";
  s += "<br><span style='color:lightblue; font-size:small;'>Found DAB ensembles: " + QString::number(mScanResult.nrChannels) + "</span>";
  s += "<br><span style='color:lightblue; font-size:small;'>Found audio services: " + QString::number(mScanResult.nrAudioServices) + "</span>";
  s += "<br><span style='color:lightblue; font-size:small;'>Found packet services: " + QString::number(mScanResult.nrNonAudioServices) + "</span>";
  return s;
}
