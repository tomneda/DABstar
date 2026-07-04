/*
 * Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "tii_manager.h"
#include "configuration.h"
#include "dab_processor.h"
#include "map_http_server.h"
#include "setting_helper.h"
#include "compass_direction.h"
#include <QDir>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QLabel>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QWidget>

Q_LOGGING_CATEGORY(sLogTii, "TiiManager", QtInfoMsg)

static QString tiiNumber(const i32 n)
{
  return n < 10 ? QString("0%1").arg(n) : QString::number(n);
}

TiiManager::TiiManager(const SResourceConfig & cfg, QObject * parent)
  : QObject(parent)
  , mpConfig(cfg.pConfig)
  , mpCmbTiiList(cfg.pCmbTiiList)
  , mpLblTii(cfg.pLblTii)
  , mpParentWidget(cfg.pParentWidget)
{
  assert(mpConfig != nullptr);
  assert(mpCmbTiiList != nullptr);
  assert(mpLblTii != nullptr);

  mTiiIndexCntTimer.setSingleShot(true);
  mTiiIndexCntTimer.setInterval(2000);  // cTiiIndexCntTimeoutMs
  connect(&mTiiIndexCntTimer, &QTimer::timeout, this, &TiiManager::_slot_tii_index_cnt_timeout);

  connect(&mTiiListDisplay, &TiiListDisplay::signal_frame_closed, this, &TiiManager::slot_handle_tii_viewer_closed);

  mShowTiiListWindow = Settings::TiiViewer::varUiVisible.read().toBool();
}

void TiiManager::set_channel_info(const QString & fIdOrCh, const u32 eid, const QString & countryName, const QString & typeInfo)
{
  mCurFIdOrCh     = fIdOrCh;
  mCurEid         = eid;
  mCurCountryName = countryName;
  mCurTypeInfo    = typeInfo;
}

bool TiiManager::init_tii_file()
{
  QString tiiFileName = Settings::Config::varTiiFile.read().toString();

  if (tiiFileName.isEmpty() || !QFile(tiiFileName).exists())
  {
    tiiFileName = ":res/txdata.tii";
  }

  const bool ok = mTiiHandler.fill_cache_from_tii_file(tiiFileName);

  if (!ok)
  {
    const QString toolTip = "File '" + tiiFileName + "' could not be loaded. So this feature is switched off.";
    emit signal_tii_file_status(false, toolTip);
  }
  else
  {
    emit signal_tii_file_status(true, {});
  }

  return ok;
}

bool TiiManager::fill_tii_cache(const QString & fileName)
{
  return mTiiHandler.fill_cache_from_tii_file(fileName);
}

void TiiManager::hide_tii_display()
{
  mTiiListDisplay.hide();
}

void TiiManager::clear_tii_list_and_label()
{
  mpCmbTiiList->clear();
  mpLblTii->setText("");
  mTiiIndex = 0;
}

void TiiManager::load_table()
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
    QMessageBox::information(mpParentWidget, QObject::tr("success"), QObject::tr("Loading and installing TII database completed\n"));
    fill_tii_cache(tableFile);
    emit signal_tii_file_status(true, {});
  }
  else
  {
    QMessageBox::information(mpParentWidget, QObject::tr("fail"), QObject::tr("Loading database failed\n"));
    emit signal_tii_file_status(false, "Failed to load TII database.");
  }
}

void TiiManager::show_tii(const std::vector<STiiResult> & iTiiList)
{
  if (!mIsChannelRunning)
  {
    return;
  }

  const bool isDropDownVisible = mpCmbTiiList->view()->isVisible();
  const f32 ownLatitude   = real(mLocalPos);
  const f32 ownLongitude  = imag(mLocalPos);
  const bool ownCoordinatesSet = (ownLatitude != 0.0f && ownLongitude != 0.0f);

  if (mShowTiiListWindow)
  {
    mTiiListDisplay.start_adding();
  }

  if (!isDropDownVisible)
  {
    mpCmbTiiList->clear();
  }

  if (mShowTiiListWindow)
  {
    mTiiListDisplay.show();
    mTiiListDisplay.set_window_title("TII list of " + mCurTypeInfo);
  }

  mTransmitterIds = iTiiList;
  STiiDataEntry tr_fb; // fallback data storage if no database entry found

  for (u32 index = 0; index < mTransmitterIds.size(); index++)
  {
    const STiiResult & tii = mTransmitterIds[index];

    if (tii.mainId == 0xFF)
    {
      continue;
    }

    if (index == 0) // show only the "strongest" TII number in the label
    {
      const QString a = "TII: " + tiiNumber(tii.mainId) + "-" + tiiNumber(tii.subId);
      mpLblTii->setAlignment(Qt::AlignRight);
      mpLblTii->setText(a);
    }

    const STiiDataEntry * pTr = mTiiHandler.get_transmitter_data((mIsFileMode ? "any" : mCurFIdOrCh), mCurEid, tii.mainId, tii.subId);
    const bool dataValid = (pTr != nullptr);

    if (!dataValid)
    {
      tr_fb = {};
      tr_fb.mainId = tii.mainId;
      tr_fb.subId  = tii.subId;
      tr_fb.country = mCurCountryName;
      tr_fb.Eid    = mCurEid;
      tr_fb.transmitterName = "(no database entry)";
      pTr = &tr_fb;
    }

    TiiListDisplay::SDerivedData bd;

    if (dataValid)
    {
      if (ownCoordinatesSet)
      {
        bd.distance_km = mTiiHandler.distance(pTr->latitude, pTr->longitude, ownLatitude, ownLongitude);
        bd.corner_deg  = mTiiHandler.corner(pTr->latitude, pTr->longitude, ownLatitude, ownLongitude);
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
    bd.strength_dB     = log10_times_10(tii.strength);
    bd.phase_deg       = tii.phaseDeg;
    bd.isNonEtsiPhase  = tii.isNonEtsiPhase;

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
          mpCmbTiiList->addItem(QString("%1/%2: %3  %4km  %5  %6m + %7m")
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
          mpCmbTiiList->addItem(QString("%1/%2: %3 (set map coord. for dist./dir.) %4m + %5m")
                                .arg(index + 1)
                                .arg(mTransmitterIds.size())
                                .arg(pTr->transmitterName)
                                .arg(pTr->altitude)
                                .arg(pTr->height));
        }
      }
      else
      {
        mpCmbTiiList->addItem(QString("%1/%2: No database entry found for TII %3-%4")
                              .arg(index + 1)
                              .arg(mTransmitterIds.size())
                              .arg(tiiNumber(tii.mainId))
                              .arg(tiiNumber(tii.subId)));
      }
    }

    if (mpHttpHandler != nullptr && dataValid)
    {
      const QDateTime theTime = (Settings::Config::cbUseUtcTime.read().toBool()
                                 ? QDateTime::currentDateTimeUtc()
                                 : QDateTime::currentDateTime());
      mpHttpHandler->add_transmitter_location_entry(MAP_NORM_TRANS, pTr, theTime.toString(Qt::TextDate),
                                                    bd.strength_dB, bd.distance_km, bd.corner_deg, bd.isNonEtsiPhase);
    }
  }

  // iterate over combobox entries if auto-iterate is active
  if (mpConfig->cbAutoIterTiiEntries->isChecked() && mpCmbTiiList->count() > 0 && !isDropDownVisible)
  {
    mpCmbTiiList->setCurrentIndex((i32)mTiiIndex % mpCmbTiiList->count());

    if (!mTiiIndexCntTimer.isActive())
    {
      mTiiIndex = (mTiiIndex + 1) % (u32)mpCmbTiiList->count();
      mTiiIndexCntTimer.start();
    }
  }

  if (mShowTiiListWindow)
  {
    mTiiListDisplay.finish_adding();
  }
}

void TiiManager::slot_use_strongest_peak(const bool iIsChecked)
{
  if (mpDabProcessor != nullptr)
  {
    mpDabProcessor->set_sync_on_strongest_peak(iIsChecked);
  }
}

void TiiManager::slot_handle_tii_collisions(const bool iIsChecked)
{
  if (mpDabProcessor != nullptr)
  {
    mpDabProcessor->set_tii_collisions(iIsChecked);
  }
}

void TiiManager::slot_handle_tii_threshold(const i32 trs)
{
  if (mpDabProcessor != nullptr)
  {
    mpDabProcessor->set_tii_threshold(trs);
  }
}

void TiiManager::slot_handle_tii_subid(const i32 subid)
{
  if (mpDabProcessor != nullptr)
  {
    mpDabProcessor->set_tii_sub_id(subid);
  }
}

void TiiManager::slot_handle_tii_button()
{
  mShowTiiListWindow = !mShowTiiListWindow;
  mTiiListDisplay.setVisible(mShowTiiListWindow);
  Settings::TiiViewer::varUiVisible.write(mShowTiiListWindow);
}

void TiiManager::slot_handle_tii_viewer_closed()
{
  if (mShowTiiListWindow)
  {
    slot_handle_tii_button();
  }
}

void TiiManager::_slot_tii_index_cnt_timeout()
{
  // just reset the timer-active flag — next call to show_tii will increment and restart
}
