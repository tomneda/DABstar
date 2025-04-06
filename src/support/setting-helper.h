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
#include <QCheckBox>
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
    soundChannel,
    picturesPath,
    filePath,
    epgPath,
    dabBand,
    skipFile,
    tiiFile,
    device,
    deviceFile,
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
    cbManualBrowserStart,
    cbCloseDirect,
    cbUseDcAvoidance,
    cbUseDcRemoval,
    cbGenXmlFromEpg,
    hidden,
    latitude,
    longitude,
    cbAlwaysOnTop,
    saveDirAudioDump,
    saveDirSampleDump,
    saveDirContent,
    cbSaveTransToCsv,
    cbSaveSlides,
    serviceListSortCol,
    serviceListSortDesc,
    cbShowNonAudioInServiceList,
    cbUse_strongest_peak,
    cbTiiCollisions,
    cbUseNativeFileDialog,
    cbUseUtcTime,
    cbShowTiiList,
    tii_threshold,
    tii_subid,
    cbUrlClickable,

    // special enums for windows position and size storage
    configWidget,
    mainWidget
  };

  QVariant read(const EElem iElem) const;
  void write(const EElem iElem, const QVariant & iVal);

  void read_widget_geometry(const EElem iElem, QWidget * const iopWidget) const;
  void write_widget_geometry(const EElem iElem, const QWidget * const ipWidget);

  template<EElem iElem> void register_and_connect_ui_element(QCheckBox * const ipPushButton);
  template<EElem iElem> void register_and_connect_ui_element(QSpinBox * const ipSpinBox);
  void sync_ui_state(const EElem iElem, const bool iWriteSetting);
  void sync_all_ui_states(const bool iWriteSetting) noexcept;

  void sync_and_unregister_ui_elements();
  QSettings * get_settings() const { return mpSettings; } // for a direct access to the QSettings (should be removed at last)

private:
  explicit SettingHelper(QSettings * ipSettings);
  ~SettingHelper() override = default; // do not cleanup things here, this is one of the last live instances in the system

  QSettings * const mpSettings;

  struct SMapElem
  {
    QString  Group;
    QString  KeyStr;
    QVariant KeyVal;
    QWidget * pWidget = nullptr;
  };

  QMap<EElem, SMapElem> mMap;

  template<typename T> void _write_setting(SMapElem & me, const T iValue);
  void _fill_map_from_settings();
  void _fill_map_with_defaults();
  const SMapElem & _get_map_elem(SettingHelper::EElem iElem) const;
  SMapElem & _get_map_elem(SettingHelper::EElem iElem);
};

template<typename T>
void SettingHelper::_write_setting(SMapElem & me, const T iValue)
{
  if (me.KeyVal != iValue)
  {
    me.KeyVal = iValue;
    mpSettings->beginGroup(me.Group);
    mpSettings->setValue(me.KeyStr, me.KeyVal);
    mpSettings->endGroup();
  }
}

template<SettingHelper::EElem iElem>
void SettingHelper::register_and_connect_ui_element(QCheckBox * const ipPushButton)
{
  SettingHelper::SMapElem & me = _get_map_elem(iElem);
  me.pWidget = ipPushButton;

  const auto * const pAB = dynamic_cast<QCheckBox *>(me.pWidget);
  Q_ASSERT(pAB != nullptr);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(pAB, &QCheckBox::checkStateChanged, [this, &me](int iState){ _write_setting(me, iState); });
#else
  connect(pAB, &QCheckBox::stateChanged, [this, &me](int iState){ _write_setting(me, iState); });
#endif
}

template<SettingHelper::EElem iElem>
void SettingHelper::register_and_connect_ui_element(QSpinBox * const ipSpinBox)
{
  SettingHelper::SMapElem & me = _get_map_elem(iElem);
  me.pWidget = ipSpinBox;

  const auto * const pSB = dynamic_cast<QSpinBox *>(me.pWidget);
  Q_ASSERT(pSB != nullptr);
  connect(pSB, qOverload<int>(&QSpinBox::valueChanged), [this, &me](int iValue){ _write_setting(me, iValue); });
}

#endif // DABSTAR_SETTING_HELPER_H
