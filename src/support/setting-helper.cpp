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
#include <QSettings>
#include <QDir>

SettingHelper::SettingHelper(QSettings * ipSettings) :
  mpSettings(ipSettings)
{
  Q_ASSERT(mpSettings != nullptr);
  _fill_map_with_defaults();
  _fill_map_from_settings();
}

void SettingHelper::_fill_map_with_defaults()
{
  mMap.insert(autoBrowser, { "autoBrowser", false });
  mMap.insert(closeDirect, { "closeDirect", false });
  mMap.insert(dcAvoidance, { "dcAvoidance", false });
  mMap.insert(dcRemoval, { "dcRemoval", false });
  mMap.insert(deviceVisible, { "deviceVisible", false });
  mMap.insert(epg2xml, { "epg2xml", false });
  mMap.insert(hidden, { "hidden", true });
  mMap.insert(latitude, { "latitude", 0 });
  mMap.insert(longitude, { "longitude", 0 });
  mMap.insert(onTop, { "onTop", false });
  mMap.insert(saveDirAudioDump, { "saveDirAudioDump", QDir::homePath() });
  mMap.insert(saveDirSampleDump, { "saveDirSampleDump", QDir::homePath() });
  mMap.insert(saveDirContent, { "saveDirContent", QDir::homePath() });
  mMap.insert(saveLocations, { "saveLocations", false });
  mMap.insert(saveSlides, { "saveSlides", false });
  mMap.insert(serviceListSortCol, { "serviceListSortCol", false });
  mMap.insert(serviceListSortDesc, { "serviceListSortDesc", false });
  mMap.insert(spectrumVisible, { "spectrumVisible", false });
  mMap.insert(switchDelay, { "switchDelay", 3 });
  mMap.insert(techDataVisible, { "techDataVisible", false });
  mMap.insert(tii_detector, { "tii_detector", false });
  mMap.insert(transmitterTags, { "transmitterTags", false });
  mMap.insert(useNativeFileDialogs, { "useNativeFileDialogs", false });
  mMap.insert(utcSelector, { "utcSelector", false });
  mMap.insert(saveServiceSelector, { "saveServiceSelector", false });
}

void SettingHelper::_fill_map_from_settings()
{
  for (auto & it : mMap)
  {
    it.KeyVal = mpSettings->value(it.KeyStr, it.KeyVal);
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
    mpSettings->setValue(me.KeyStr, me.KeyVal);
  }
}
