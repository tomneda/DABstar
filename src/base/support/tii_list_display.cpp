/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2024
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
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
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "tii_list_display.h"
#include "dabradio.h"
#include "openfiledialog.h"
#include "gui_helpers.h"
#include "setting_helper.h"
#include "qt_compat.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include <cstdio>

constexpr f32 cRefLuminance = 0.40f;

TiiListDisplay::TiiListDisplay()
{
  mpWidget.reset(new QWidget(nullptr));
  mpWidget->setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  mpWidget->setWindowTitle("TII list");

  mpTableWidget = new QTableWidget(0, cColNr, mpWidget.get());
  mpTableWidget->setHorizontalHeaderLabels(QStringList() << "Main" << "Sub" << "Level" << "Phase" << "Transmitter"
                                           << "Distance" << "Dir" << "Power" << "Altitude" << "Height");

  mpCbKeepPersistent = new QCheckBox("Keep persistent", mpWidget.get());
  mpCbKeepPersistent->setToolTip("Keep transmitters from previous scans visible (grayed out) when not detected in current scan");

  mpCbIgnoreNoDb = new QCheckBox("Ignore without DB entry", mpWidget.get());
  mpCbIgnoreNoDb->setToolTip("Ignore detected transmitters that have no matching database entry");

  mpBtnSaveList = new QPushButton("Save to CSV or KML", mpWidget.get());
  mpBtnSaveList->setStyleSheet(get_bg_style_sheet(0x509B50, Qt::white, cRefLuminance));
  mpBtnSaveList->setToolTip("Save the displayed transmitter list to a CSV or KML file");
  mpBtnSaveList->setEnabled(false);

  mpBtnOpenFolder = new QPushButton("Open save folder", mpWidget.get());
  mpBtnOpenFolder->setStyleSheet(get_bg_style_sheet(0xDCB42D, Qt::white, cRefLuminance));
  mpBtnOpenFolder->setToolTip("Open storage folder in file manager and select the last saved file");

  auto * pBottomLayout = new QHBoxLayout();
  pBottomLayout->addWidget(mpCbKeepPersistent);
  pBottomLayout->addWidget(mpCbIgnoreNoDb);
  pBottomLayout->addStretch();
  pBottomLayout->addWidget(mpBtnSaveList);
  pBottomLayout->addWidget(mpBtnOpenFolder);

  auto * pMainLayout = new QVBoxLayout(mpWidget.get());
  pMainLayout->addWidget(mpTableWidget);
  pMainLayout->addLayout(pBottomLayout);

  Settings::TiiViewer::posAndSize.read_widget_geometry(mpWidget.get());

  connect(mpCbKeepPersistent, &QCheckBox::stateChangedSubst, this, &TiiListDisplay::_slot_keep_persistent_changed);
  connect(mpCbIgnoreNoDb, &QCheckBox::stateChangedSubst, this, &TiiListDisplay::_slot_ignore_no_db_changed);
  connect(mpBtnSaveList, &QPushButton::clicked, this, &TiiListDisplay::_slot_save_list);
  connect(mpBtnOpenFolder, &QPushButton::clicked, this, &TiiListDisplay::_slot_open_folder);

  Settings::TiiViewer::cbKeepPersistent.register_widget_and_update_ui_from_setting(mpCbKeepPersistent, 0);
  Settings::TiiViewer::cbIgnoreNoDb.register_widget_and_update_ui_from_setting(mpCbIgnoreNoDb, 0);
}

TiiListDisplay::~TiiListDisplay()
{
  Settings::TiiViewer::posAndSize.write_widget_geometry(mpWidget.get());
  if (mpTableWidget != nullptr)
  {
    mpTableWidget->setRowCount(0);
  }
}

void TiiListDisplay::set_window_title(const QString & channel)
{
  if (mpWidget != nullptr)
  {
    mpWidget->setWindowTitle(channel);
  }
}

void TiiListDisplay::start_adding()
{
  mCurrentScanEntries.clear();
  if (mpWidget != nullptr)
  {
    mpWidget->setWindowTitle("TII list");
  }
}

void TiiListDisplay::add_row(const STiiDataEntry & iTr, const SDerivedData & iDD, bool iHasDbEntry)
{
  const QString theTime = (Settings::Config::cbUseUtcTime.read().toBool()
                             ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
                             : QDateTime::currentDateTime().toString(Qt::ISODate));

  STableRowEntry entry;
  entry.tr = iTr;
  entry.dd = iDD;
  entry.dateTime = theTime;
  entry.isCurrent = true;
  entry.hasDbEntry = iHasDbEntry && (iTr.transmitterName != "(no database entry)");
  mCurrentScanEntries.push_back(entry);
}

void TiiListDisplay::finish_adding()
{
  if (mpCbKeepPersistent != nullptr && mpCbKeepPersistent->isChecked())
  {
    std::vector<STableRowEntry> inactiveEntries;
    for (auto & prev : mEntries)
    {
      const bool foundInCurrent = std::any_of(mCurrentScanEntries.begin(), mCurrentScanEntries.end(), [&](const STableRowEntry & cur)
      {
        return cur.tr.mainId == prev.tr.mainId && cur.tr.subId == prev.tr.subId;
      });

      if (!foundInCurrent)
      {
        prev.isCurrent = false;
        inactiveEntries.push_back(prev);
      }
    }

    mEntries = mCurrentScanEntries;
    mEntries.insert(mEntries.end(), inactiveEntries.begin(), inactiveEntries.end());
  }
  else
  {
    mEntries = mCurrentScanEntries;
  }

  _update_table();
}

void TiiListDisplay::_update_table()
{
  if (mpTableWidget == nullptr)
  {
    return;
  }

  const bool ignoreNoDb = (mpCbIgnoreNoDb != nullptr && mpCbIgnoreNoDb->isChecked());

  mpTableWidget->setRowCount(0);
  i32 row = 0;
  for (const auto & e : mEntries)
  {
    if (ignoreNoDb && !e.hasDbEntry)
    {
      continue;
    }

    const auto & iTr = e.tr;
    const auto & iDD = e.dd;

    mpTableWidget->insertRow(row);
    mpTableWidget->setItem(row, 0, new QTableWidgetItem(QString::number(iTr.mainId)));
    mpTableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(iTr.subId)));
    mpTableWidget->setItem(row, 2, new QTableWidgetItem(QString::number(iDD.strength_dB, 'f', 1) + "dB"));
    mpTableWidget->setItem(row, 3, new QTableWidgetItem(QString::number(iDD.phase_deg, 'f', 0) + "°" + (iDD.isNonEtsiPhase ? '*' : ' ')));
    mpTableWidget->setItem(row, 4, new QTableWidgetItem(iTr.transmitterName));
    mpTableWidget->setItem(row, 5, new QTableWidgetItem(QString::number(iDD.distance_km, 'f', 1) + "km"));
    mpTableWidget->setItem(row, 6, new QTableWidgetItem(QString::number(iDD.corner_deg, 'f', 1) + "°"));
    mpTableWidget->setItem(row, 7, new QTableWidgetItem(QString::number(iTr.power, 'f', 1) + "kW"));
    mpTableWidget->setItem(row, 8, new QTableWidgetItem(QString::number(iTr.altitude) + "m"));
    mpTableWidget->setItem(row, 9, new QTableWidgetItem(QString::number(iTr.height) + "m"));

    const QBrush textBrush = e.isCurrent ? mpTableWidget->palette().text() : QBrush(Qt::gray);

    for (i32 colIdx = 0; colIdx < cColNr; colIdx++)
    {
      auto * item = mpTableWidget->item(row, colIdx);
      if (item != nullptr)
      {
        if (colIdx == 4) // transmitter name is left aligned
        {
          item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        }
        else
        {
          item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
        item->setForeground(textBrush);
      }
    }
    row++;
  }
  mpTableWidget->resizeColumnsToContents();
  if (mpBtnSaveList != nullptr)
  {
    mpBtnSaveList->setEnabled(row > 0);
  }
}

void TiiListDisplay::clear()
{
  mEntries.clear();
  mCurrentScanEntries.clear();
  if (mpTableWidget != nullptr)
  {
    mpTableWidget->setRowCount(0);
  }
  if (mpBtnSaveList != nullptr)
  {
    mpBtnSaveList->setEnabled(false);
  }
  if (mpWidget != nullptr)
  {
    mpWidget->setWindowTitle("TII list");
  }
}

i32 TiiListDisplay::get_nr_rows() const
{
  return mpTableWidget != nullptr ? mpTableWidget->rowCount() : 0;
}

void TiiListDisplay::show() const
{
  if (mpWidget != nullptr)
  {
    mpWidget->show();
  }
}

void TiiListDisplay::hide() const
{
  if (mpWidget != nullptr)
  {
    mpWidget->hide();
  }
}

void TiiListDisplay::_slot_keep_persistent_changed(i32 /*iState*/)
{
  if (mpCbKeepPersistent != nullptr && !mpCbKeepPersistent->isChecked())
  {
    mEntries.erase(
      std::remove_if(mEntries.begin(), mEntries.end(), [](const STableRowEntry & e) { return !e.isCurrent; }),
      mEntries.end()
    );
    _update_table();
  }
}

void TiiListDisplay::_slot_ignore_no_db_changed(i32 /*iState*/)
{
  _update_table();
}

void TiiListDisplay::_slot_save_list()
{
  const bool ignoreNoDb = (mpCbIgnoreNoDb != nullptr && mpCbIgnoreNoDb->isChecked());

  std::vector<STableRowEntry> exportEntries;
  for (const auto & e : mEntries)
  {
    if (ignoreNoDb && !e.hasDbEntry)
    {
      continue;
    }
    exportEntries.push_back(e);
  }

  if (exportEntries.empty())
  {
    return;
  }

  const bool useNativeFileDialog = Settings::Config::cbUseNativeFileDialog.read().toBool();
  const QString storedDir = Settings::Storage::instance().value(sSettingContentStorageDir, QDir::homePath()).toString();

  QString baseFileName = PRJ_NAME "-TII-" + (mCurChannel.isEmpty() ? "unknown" : mCurChannel) + "-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");

  const QString invalidChars = "\\/:*?\"<>|";
  for (const QChar c : invalidChars)
  {
    baseFileName.remove(c);
  }

  const QString defaultPath = QDir(storedDir).filePath(baseFileName + ".csv");
  QString selectedFilter;

  const QString fileName = QFileDialog::getSaveFileName(
    mpWidget.get(),
    "Save transmitter list ...",
    defaultPath,
    "CSV files (*.csv);;KML files (*.kml)",
    &selectedFilter,
    useNativeFileDialog ? QFileDialog::Options() : QFileDialog::DontUseNativeDialog
  );

  if (fileName.isEmpty())
  {
    return;
  }

  QString finalFileName = fileName;
  bool isKml = false;
  if (selectedFilter.contains("*.kml", Qt::CaseInsensitive) || finalFileName.endsWith(".kml", Qt::CaseInsensitive))
  {
    isKml = true;
    if (!finalFileName.endsWith(".kml", Qt::CaseInsensitive))
    {
      finalFileName += ".kml";
    }
  }
  else
  {
    if (!finalFileName.endsWith(".csv", Qt::CaseInsensitive))
    {
      finalFileName += ".csv";
    }
  }

  finalFileName = QDir::toNativeSeparators(finalFileName);

  FILE * fp = OpenFileDialog::open_file(finalFileName, "w");
  if (fp == nullptr)
  {
    QMessageBox::critical(mpWidget.get(), "Error", "Could not open file for writing:\n" + finalFileName);
    return;
  }

  if (isKml)
  {
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp, "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n");
    fprintf(fp, "<Document>\n");
    fprintf(fp, "  <name>DAB Transmitters</name>\n");
    fprintf(fp, "  <description>DABstar transmitter locations</description>\n");
    fprintf(fp, "  <Style id=\"transmitterIcon\">\n");
    fprintf(fp, "    <IconStyle>\n");
    fprintf(fp, "      <Icon>\n");
    fprintf(fp, "        <href>http://maps.google.com/mapfiles/kml/shapes/donut.png</href>\n");
    fprintf(fp, "      </Icon>\n");
    fprintf(fp, "    </IconStyle>\n");
    fprintf(fp, "  </Style>\n");

    for (const auto & e : exportEntries)
    {
      if (!e.hasDbEntry)
      {
        continue;
      }

      const QString channel = e.tr.channel.isEmpty() ? mCurChannel : e.tr.channel;
      const QString dir = e.tr.direction.isEmpty() ? "undefined" : e.tr.direction;

      fprintf(fp, "  <Placemark>\n");
      fprintf(fp, "    <name>%s</name>\n", e.tr.transmitterName.toUtf8().data());
      fprintf(fp, "    <description><![CDATA[\n");
      fprintf(fp, "      <b>Channel:</b> %s<br/>\n", channel.toUtf8().data());
      fprintf(fp, "      <b>TII:</b> %02d-%02d<br/>\n", e.tr.mainId, e.tr.subId);
      fprintf(fp, "      <b>Strength:</b> %.1f dB<br/>\n", e.dd.strength_dB);
      fprintf(fp, "      <b>Distance:</b> %.1f km<br/>\n", e.dd.distance_km);
      fprintf(fp, "      <b>Azimuth:</b> %.1f°<br/>\n", e.dd.corner_deg);
      fprintf(fp, "      <b>Power:</b> %.3f kW<br/>\n", e.tr.power);
      fprintf(fp, "      <b>Altitude:</b> %d m<br/>\n", e.tr.altitude);
      fprintf(fp, "      <b>Height:</b> %d m<br/>\n", e.tr.height);
      fprintf(fp, "      <b>Polarization:</b> %s<br/>\n", e.tr.polarization.toUtf8().data());
      fprintf(fp, "      <b>Direction:</b> %s<br/>\n", dir.toUtf8().data());
      fprintf(fp, "      <b>Date/Time:</b> %s<br/>\n", e.dateTime.toUtf8().data());
      fprintf(fp, "    ]]></description>\n");
      fprintf(fp, "    <styleUrl>#transmitterIcon</styleUrl>\n");
      fprintf(fp, "    <Point>\n");
      fprintf(fp, "      <coordinates>%f,%f,%d</coordinates>\n", e.tr.longitude, e.tr.latitude, e.tr.altitude);
      fprintf(fp, "    </Point>\n");
      fprintf(fp, "  </Placemark>\n");
    }

    fprintf(fp, "</Document>\n");
    fprintf(fp, "</kml>\n");
  }
  else
  {
    fprintf(fp, "Channel; Latitude; Longitude; Transmitter; Altitude; Height; Date and time; MainId; SubId; Strength; Distance; Azimuth; Power\n");
    for (const auto & e : exportEntries)
    {
      const QString channel = e.tr.channel.isEmpty() ? mCurChannel : e.tr.channel;
      fprintf(fp,
              "%s; %f; %f; %s; %d; %d; %s; %d; %d; %.1f; %.1f; %.1f; %.3f\n",
              channel.toUtf8().data(),
              e.tr.latitude,
              e.tr.longitude,
              e.tr.transmitterName.toUtf8().data(),
              e.tr.altitude,
              e.tr.height,
              e.dateTime.toUtf8().data(),
              e.tr.mainId,
              e.tr.subId,
              e.dd.strength_dB,
              e.dd.distance_km,
              e.dd.corner_deg,
              e.tr.power);
    }
  }

  fclose(fp);

  Settings::Storage::instance().setValue(sSettingContentStorageDir, QFileInfo(finalFileName).path());
  mLastSavedFilePath = finalFileName;
}

void TiiListDisplay::_slot_open_folder()
{
  const QString storedDir = Settings::Storage::instance().value(sSettingContentStorageDir, QDir::homePath()).toString();
  open_folder_and_select_file(mLastSavedFilePath, storedDir);
}
