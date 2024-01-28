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

#ifndef DABSTAR_SETTING_HELPER_H
#define DABSTAR_SETTING_HELPER_H

#include <QMap>
#include <QVariant>

class QSettings;

class SettingHelper
{
public:
  static SettingHelper & get_instance(QSettings * ipSettings = nullptr)
  {
    static SettingHelper instance(ipSettings);
    return instance;
  }

  SettingHelper(SettingHelper &other) = delete;
  void operator=(const SettingHelper &) = delete;

  enum EElem
  {
    autoBrowser,
    closeDirect,
    dcAvoidance,
    dcRemoval,
    deviceVisible,
    epg2xml,
    hidden,
    latitude,
    longitude,
    onTop,
    saveDirAudioDump,
    saveDirSampleDump,
    saveDirContent,
    saveLocations,
    saveSlides,
    serviceListSortCol,
    serviceListSortDesc,
    spectrumVisible,
    switchDelay,
    techDataVisible,
    tii_detector,
    transmitterTags,
    useNativeFileDialogs,
    utcSelector,
    saveServiceSelector
  };

  QVariant read(const EElem iElem) const;
  void write(const EElem iElem, const QVariant & iVal);

  QSettings * get_settings() const { return mpSettings; } // for a direct access to the QSettings ...

private:
  explicit SettingHelper(QSettings * ipSettings);
  ~SettingHelper() = default;

  QSettings * const mpSettings;

  struct SMapElem
  {
    QString KeyStr;
    QVariant KeyVal;
  };
  
  QMap<EElem, SMapElem> mMap;

  void _fill_map_from_settings();
  void _fill_map_with_defaults();
};

#endif // DABSTAR_SETTING_HELPER_H