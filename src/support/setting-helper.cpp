/*
 * Copyright (c) 2024 by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
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

#include "setting-helper.h"
#include "dab-constants.h"
#include <QSettings>
#include <QDir>
#include <QWidget>
#include <QAbstractButton>
#include <QSpinBox>

SettingHelper::SettingHelper(QSettings * ipSettings) :
  mpSettings(ipSettings)
{
  Q_ASSERT(mpSettings != nullptr);
  _fill_map_with_defaults();
  _fill_map_from_settings();
}

SettingHelper::~SettingHelper()
{
  sync();
}

void SettingHelper::_fill_map_with_defaults()
{
  QDir tempPath = QDir::tempPath();
  tempPath.setPath(tempPath.filePath(PRJ_NAME));
  QString tempPicPath = tempPath.filePath("PIC").append('/');
  QString tempEpgPath = tempPath.filePath("EPG").append('/');

  mMap.insert(dabMode, { "", "dabMode", 1 });
  mMap.insert(threshold, { "", "threshold", 3 });
  mMap.insert(diffLength, { "", "diff_length", DIFF_LENGTH });
  mMap.insert(tiiDelay, { "", "tii_delay", 5 });
  mMap.insert(tiiDepth, { "", "tii_depth", 4 });
  mMap.insert(echoDepth, { "", "echo_depth", 1  });
  mMap.insert(latency, { "", "latency", 5 });
  mMap.insert(soundchannel, { "", "soundchannel", "default" });
  mMap.insert(picturesPath, { "", "picturesPath", tempPicPath });
  mMap.insert(filePath, { "", "filePath", tempPicPath });
  mMap.insert(epgPath, { "", "epgPath", tempEpgPath });
  mMap.insert(dabBand, { "", "dabBand", "VHF Band III" });
  mMap.insert(skipFile, { "", "skipFile", "" });
  mMap.insert(tiiFile, { "", "tiiFile", "" });
  mMap.insert(device, { "", "device", "no device" });
  mMap.insert(deviceVisible, { "", "deviceVisible", true });
  mMap.insert(spectrumVisible, { "", "spectrumVisible", false });
  mMap.insert(techDataVisible, { "", "techDataVisible", false });
  mMap.insert(showDeviceWidget, { "", "showDeviceWidget", false });
  mMap.insert(presetName, { "", "presetname", "" });
  mMap.insert(channel, { "", "channel", "" });
  mMap.insert(epgWidth, { "", "epgWidth", 70 });
  mMap.insert(browserAddress, { "", "browserAddress", "http://localhost" });
  mMap.insert(mapPort, { "", "mapPort", 8080 });
  mMap.insert(epgFlag, { "", "epgFlag", false });

  mMap.insert(autoBrowser, { "", "autoBrowser", false });
  mMap.insert(closeDirect, { "", "closeDirect", false });
  mMap.insert(dcAvoidance, { "", "dcAvoidance", false });
  mMap.insert(dcRemoval, { "", "dcRemoval", false });
  mMap.insert(epg2xml, { "", "epg2xml", false });
  mMap.insert(hidden, { "", "hidden", true });
  mMap.insert(latitude, { "", "latitude", 0 });
  mMap.insert(longitude, { "", "longitude", 0 });
  mMap.insert(onTop, { "", "onTop", false });
  mMap.insert(saveDirAudioDump, { "", "saveDirAudioDump", QDir::homePath() });
  mMap.insert(saveDirSampleDump, { "", "saveDirSampleDump", QDir::homePath() });
  mMap.insert(saveDirContent, { "", "saveDirContent", QDir::homePath() });
  mMap.insert(saveLocations, { "", "saveLocations", false });
  mMap.insert(saveSlides, { "", "saveSlides", false });
  mMap.insert(serviceListSortCol, { "", "serviceListSortCol", false });
  mMap.insert(serviceListSortDesc, { "", "serviceListSortDesc", false });
  mMap.insert(switchDelay, { "", "switchDelay", SWITCH_DELAY });
  mMap.insert(tiiDetector, { "", "tii_detector", false });
  mMap.insert(transmitterTags, { "", "transmitterTags", false });
  mMap.insert(useNativeFileDialogs, { "", "useNativeFileDialogs", false });
  mMap.insert(utcSelector, { "", "utcSelector", false });
  mMap.insert(saveServiceSelector, { "", "saveServiceSelector", false });

  // special enums for windows position and size storage
  mMap.insert(configWidget, { "", "configWidget", QVariant() });
  mMap.insert(mainWidget, { "", "mainWidget", QVariant() });
}

void SettingHelper::_fill_map_from_settings()
{
  for (auto & it : mMap)
  {
    mpSettings->beginGroup(it.Group);
    it.KeyVal = mpSettings->value(it.KeyStr, it.KeyVal);
    mpSettings->endGroup();
  }
}

QVariant SettingHelper::read(const EElem iElem) const
{
  auto it = mMap.find(iElem);
  Q_ASSERT(it != mMap.end());
  return it.value().KeyVal;
}

void SettingHelper::write(const EElem iElem, const QVariant & iVal)
{
  auto it = mMap.find(iElem);
  Q_ASSERT(it != mMap.end());
  SMapElem & me = it.value();

  // save changed value to setting file
  if (me.KeyVal != iVal)
  {
    me.KeyVal = iVal;
    mpSettings->beginGroup(me.Group);
    mpSettings->setValue(me.KeyStr, me.KeyVal);
    mpSettings->endGroup();
  }
}

void SettingHelper::read_widget_geometry(const SettingHelper::EElem iElem, QWidget * const iopWidget) const
{
  QVariant var = read(iElem);

  if(!var.canConvert<QByteArray>())
  {
    qDebug("Cannot retrieve widget geometry from settings.");
    return;
  }
  
  if (!iopWidget->restoreGeometry(var.toByteArray()))
  {
    qDebug("restoreGeometry() returns false");
  }
}

void SettingHelper::write_widget_geometry(const EElem iElem, const QWidget * const ipWidget)
{
  write(iElem, ipWidget->saveGeometry());
}

void SettingHelper::sync() const
{
  mpSettings->sync();
}

void SettingHelper::sync_ui_state(const EElem iElem, const bool iWriteSetting)
{
  auto it = mMap.find(iElem);
  Q_ASSERT(it != mMap.end());
  SMapElem & me = it.value();
  Q_ASSERT(me.pWidget != nullptr);

  if (auto * const pAB = dynamic_cast<QAbstractButton *>(me.pWidget);
      pAB != nullptr)
  {
    if (iWriteSetting)
    {
      write(iElem, pAB->isChecked());
    }
    else
    {
      pAB->setChecked(read(iElem).toBool());
    }
    return;
  }


  if (auto * const pSB = dynamic_cast<QSpinBox *>(me.pWidget);
      pSB != nullptr)
  {
    if (iWriteSetting)
    {
      write(iElem, pSB->value());
    }
    else
    {
      pSB->setValue(read(iElem).toInt());
    }
    return;
  }

  qFatal("Pointer to pWidget not valid for iElem %d", iElem);
}



