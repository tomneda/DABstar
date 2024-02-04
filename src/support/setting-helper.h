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

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QAbstractButton>
#include <QSpinBox>
#include <QSettings>

class QWidget;

class SettingHelper : public QObject
{
  Q_OBJECT
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
    // main widget
    dabMode,
    threshold,
    diffLength,
    tiiDelay,
    tiiDepth,
    echoDepth,
    latency,
    soundchannel,
    picturesPath,
    filePath,
    epgPath,
    dabBand,
    skipFile,
    tiiFile,
    device,
    deviceVisible,
    spectrumVisible,
    techDataVisible,
    showDeviceWidget,
    presetName,
    channel,
    epgWidth,
    browserAddress,
    mapPort,
    epgFlag,

    // needed in config widget
    autoBrowser,
    closeDirect,
    dcAvoidance,
    dcRemoval,
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
    switchDelay,
    tiiDetector,
    transmitterTags,
    useNativeFileDialogs,
    utcSelector,
    saveServiceSelector,

    // special enums for windows position and size storage
    configWidget,
    mainWidget
  };

  QVariant read(const EElem iElem) const;
  void write(const EElem iElem, const QVariant & iVal);

  void read_widget_geometry(const EElem iElem, QWidget * const iopWidget) const;
  void write_widget_geometry(const EElem iElem, const QWidget * const ipWidget);

  template<EElem iElem> void register_ui_element(QAbstractButton * const ipPushButton);
  template<EElem iElem> void register_ui_element(QSpinBox * const ipSpinBox);
  void sync_ui_state(const EElem iElem, const bool iWriteSetting);

  void sync() const;
  QSettings * get_settings() const { return mpSettings; } // for a direct access to the QSettings (should be removed at last)

private:
  explicit SettingHelper(QSettings * ipSettings);
  ~SettingHelper() override;

  QSettings * const mpSettings;

  struct SMapElem
  {
    QString  Group;
    QString  KeyStr;
    QVariant KeyVal;
    QWidget * pWidget = nullptr;
  };
  
  QMap<EElem, SMapElem> mMap;

  void _fill_map_from_settings();
  void _fill_map_with_defaults();
};

template<SettingHelper::EElem iElem> void SettingHelper::register_ui_element(QAbstractButton * const ipPushButton)
{
  auto it = mMap.find(iElem);
  Q_ASSERT(it != mMap.end());
  SMapElem & me = it.value();
  me.pWidget = ipPushButton;

  if (auto const * const pAB = dynamic_cast<QAbstractButton *>(me.pWidget);
      pAB != nullptr)
  {
    connect(pAB, &QAbstractButton::clicked, [this, &me](bool iClicked)
    {
      // save changed value to setting file
      if (me.KeyVal != iClicked)
      {
        me.KeyVal = iClicked;
        mpSettings->beginGroup(me.Group);
        mpSettings->setValue(me.KeyStr, me.KeyVal);
        mpSettings->endGroup();
      }
    });
  }
}

template<SettingHelper::EElem iElem> void SettingHelper::register_ui_element(QSpinBox * const ipSpinBox)
{
  auto it = mMap.find(iElem);
  Q_ASSERT(it != mMap.end());
  SMapElem & me = it.value();
  me.pWidget = ipSpinBox;

  if (auto const * const pSB = dynamic_cast<QSpinBox *>(me.pWidget);
      pSB != nullptr)
  {
    connect(pSB, qOverload<int>(&QSpinBox::valueChanged), [this, &me](int ival)
    {
      // save changed value to setting file
      if (me.KeyVal != ival)
      {
        me.KeyVal = ival;
        mpSettings->beginGroup(me.Group);
        mpSettings->setValue(me.KeyStr, me.KeyVal);
        mpSettings->endGroup();
      }
    });
  }
}

#endif // DABSTAR_SETTING_HELPER_H