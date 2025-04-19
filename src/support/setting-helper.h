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

#ifndef SETTING_HELPER_H
#define SETTING_HELPER_H

#include <QObject>
#include <QVariant>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QSettings>


class SettingsStorage
{
public:
  static QSettings & instance(QSettings * ipSettings = nullptr)
  {
    static SettingsStorage instance(ipSettings);
    return *instance.mpSettings;
  }

private:
  explicit SettingsStorage(QSettings * ipSettings)
  : mpSettings(ipSettings)
  {}

  QSettings * const mpSettings;
};


class SettingVariant
{
public:
  explicit SettingVariant(const QString & key);
  explicit SettingVariant(const QString & key, const QVariant & iDefaultValue);
  void define_default_value(const QVariant & iDefaultValue);
  QVariant get_variant() const;
  void set(const QVariant & iValue) const;

private:
  QString mKey;
  QVariant mDefaultValue;
};


class SettingWidget : public QObject
{
  Q_OBJECT
public:
  explicit SettingWidget(const QString & key);

  void register_widget_and_update_ui_from_setting(QWidget * const ipWidget, const QVariant & iDefaultValue);
  QVariant get_variant() const;

private:
  void _update_ui_state_from_setting() const;
  void _update_ui_state_to_setting() const;

  QString mKey;
  QWidget * mpWidget = nullptr;
  QVariant mDefaultValue;
};

class SettingPosAndSize
{
public:
  explicit SettingPosAndSize(const QString & iCat);
  void read_widget_geometry(QWidget * iopWidget, int32_t iXPosDef, int32_t iYPosDef, int32_t iWidthDef, int32_t iHeightDef, bool iIsFixedSized) const;
  void write_widget_geometry(const QWidget * ipWidget) const;

private:
  QString mCat;
};

struct Settings
{
  struct General // namespace for main window data
  {
    static inline SettingPosAndSize posAndSize{""};

  };

  struct Spectrum // namespace for the spectrum window
  {
    #define catSpectrumViewer "spectrumViewer/" // did not find nicer way to declare that once
    static inline SettingPosAndSize posAndSize{catSpectrumViewer};
  };

  struct Config  // namespace for the configuration window
  {
    #define catConfiguration "configuration/" // did not find nicer way to declare that once
    static inline SettingPosAndSize posAndSize{catConfiguration};

    static inline SettingVariant width{catConfiguration "width", 800};

    static inline SettingWidget cbCloseDirect{catConfiguration "cbCloseDirect"};
    static inline SettingWidget cbUseStrongestPeak{catConfiguration "cbUseStrongestPeak"};
    static inline SettingWidget cbUseNativeFileDialog{catConfiguration "cbUseNativeFileDialog"};
    static inline SettingWidget cbUseUtcTime{catConfiguration "cbUseUtcTime"};
    static inline SettingWidget cbGenXmlFromEpg{catConfiguration "cbGenXmlFromEpg"};
    static inline SettingWidget cbAlwaysOnTop{catConfiguration "cbAlwaysOnTop"};
    static inline SettingWidget cbManualBrowserStart{catConfiguration "cbManualBrowserStart"};
    static inline SettingWidget cbSaveSlides{catConfiguration "cbSaveSlides"};
    static inline SettingWidget cbSaveTransToCsv{catConfiguration "cbSaveTransToCsv"};
    static inline SettingWidget cbUseDcAvoidance{catConfiguration "cbUseDcAvoidance"};
    static inline SettingWidget cbUseDcRemoval{catConfiguration "cbUseDcRemoval"};
    static inline SettingWidget cbShowNonAudioInServiceList{catConfiguration "cbShowNonAudioInService"};
    static inline SettingWidget sbTiiThreshold{catConfiguration "sbTiiThreshold"};
    static inline SettingWidget cbTiiCollisions{catConfiguration "cbTiiCollisions"};
    static inline SettingWidget sbTiiSubId{catConfiguration "sbTiiSubId"};
    static inline SettingWidget cbUrlClickable{catConfiguration "cbUrlClickable"};
    static inline SettingWidget cbAutoIterTiiEntries{catConfiguration "cbAutoIterTiiEntries"};
  };

  struct CirViewer // namespace for the CIR viewer window
  {
    #define catCirViewer "cirViewer/" // did not find nicer way to declare that once
    static inline SettingPosAndSize posAndSize{catCirViewer};
    
  };
};



// legacy stuff

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
    soundChannel,
    picturesPath,
    filePath,
    epgPath,
    skipFile,
    tiiFile,
    device,
    deviceFile,
    deviceVisible,
    spectrumVisible,
    cirVisible,
    techDataVisible,
    showDeviceWidget,
    presetName,
    channel,
    epgWidth,
    browserAddress,
    mapPort,
    epgFlag,

    // needed in config widget
    hidden,
    latitude,
    longitude,
    saveDirAudioDump,
    saveDirSampleDump,
    saveDirContent,
    serviceListSortCol,
    serviceListSortDesc,
    cbShowTiiList,
    // tii_subid,

    // special enums for windows position and size storage
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

#endif // SETTING_HELPER_H
