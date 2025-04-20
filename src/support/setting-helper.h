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

namespace Settings
{

class Storage
{
public:
  static QSettings & instance(QSettings * ipSettings = nullptr)
  {
    static Storage instance(ipSettings);
    return *instance.mpSettings;
  }

private:
  explicit Storage(QSettings * ipSettings)
  : mpSettings(ipSettings)
  {}

  QSettings * const mpSettings;
};


class Variant
{
public:
  explicit Variant(const QString & key);
  explicit Variant(const QString & key, const QVariant & iDefaultValue);
  void define_default_value(const QVariant & iDefaultValue);
  QVariant get_variant() const;
  void set(const QVariant & iValue) const;

private:
  QString mKey;
  QVariant mDefaultValue;
};


class Widget : public QObject
{
  Q_OBJECT
public:
  explicit Widget(const QString & key);

  void register_widget_and_update_ui_from_setting(QWidget * const ipWidget, const QVariant & iDefaultValue);
  QVariant get_variant() const;
  int32_t get_combobox_index() const;  // only usable for ComboBoxes, returns the last selected index

private:
  void _update_ui_state_from_setting();
  void _update_ui_state_to_setting() const;

  QString mKey;
  QWidget * mpWidget = nullptr;
  QVariant mDefaultValue;
};

class PosAndSize
{
public:
  explicit PosAndSize(const QString & iCat);
  void read_widget_geometry(QWidget * iopWidget, int32_t iWidthDef, int32_t iHeightDef, bool iIsFixedSized) const;
  void write_widget_geometry(const QWidget * ipWidget) const;

private:
  QString mCat;
};

struct General // namespace for main window data
{
  static inline PosAndSize posAndSize{""};
};

struct Spectrum // namespace for the spectrum window
{
  #define catSpectrumViewer "spectrumViewer/" // did not find nicer way to declare that once
  static inline PosAndSize posAndSize{catSpectrumViewer};
};

struct Config  // namespace for the configuration window
{
  #define catConfiguration "configuration/" // did not find nicer way to declare that once
  static inline PosAndSize posAndSize{catConfiguration};

  static inline Variant varPicturesPath{catConfiguration "varPicturesPath"};
  static inline Variant varMotPath{catConfiguration "varFilePath"};
  static inline Variant varEpgPath{catConfiguration "varEpgPath"};
  static inline Variant varSkipFile{catConfiguration "varSkipFile"};
  static inline Variant varTiiFile{catConfiguration "varTiiFile"};
  static inline Variant varShowTiiList{catConfiguration "varShowTiiList", false};
  static inline Variant varBrowserAddress{catConfiguration "varBrowserAddress", "http://localhost"};
  static inline Variant varMapPort{catConfiguration "varMapPort", 8080};

  static inline Widget cbCloseDirect{catConfiguration "cbCloseDirect"};
  static inline Widget cbUseStrongestPeak{catConfiguration "cbUseStrongestPeak"};
  static inline Widget cbUseNativeFileDialog{catConfiguration "cbUseNativeFileDialog"};
  static inline Widget cbUseUtcTime{catConfiguration "cbUseUtcTime"};
  static inline Widget cbGenXmlFromEpg{catConfiguration "cbGenXmlFromEpg"};
  static inline Widget cbAlwaysOnTop{catConfiguration "cbAlwaysOnTop"};
  static inline Widget cbManualBrowserStart{catConfiguration "cbManualBrowserStart"};
  static inline Widget cbSaveSlides{catConfiguration "cbSaveSlides"};
  static inline Widget cbSaveTransToCsv{catConfiguration "cbSaveTransToCsv"};
  static inline Widget cbUseDcAvoidance{catConfiguration "cbUseDcAvoidance"};
  static inline Widget cbUseDcRemoval{catConfiguration "cbUseDcRemoval"};
  static inline Widget cbShowNonAudioInServiceList{catConfiguration "cbShowNonAudioInService"};
  static inline Widget sbTiiThreshold{catConfiguration "sbTiiThreshold"};
  static inline Widget cbTiiCollisions{catConfiguration "cbTiiCollisions"};
  static inline Widget sbTiiSubId{catConfiguration "sbTiiSubId"};
  static inline Widget cbUrlClickable{catConfiguration "cbUrlClickable"};
  static inline Widget cbAutoIterTiiEntries{catConfiguration "cbAutoIterTiiEntries"};
  static inline Widget cmbSoundOutput{catConfiguration "cmbSoundOutput"};

};

struct CirViewer // namespace for the CIR viewer window
{
  #define catCirViewer "cirViewer/" // did not find nicer way to declare that once
  static inline PosAndSize posAndSize{catCirViewer};

};

} // namespace Settings

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
    device,
    deviceFile,
    deviceVisible,
    spectrumVisible,
    cirVisible,
    techDataVisible,
    showDeviceWidget,
    presetName,
    channel,

    // needed in config widget
    hidden,
    latitude,
    longitude,
    saveDirAudioDump,
    saveDirSampleDump,
    saveDirContent,
    serviceListSortCol,
    serviceListSortDesc,
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
