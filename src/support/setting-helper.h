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
  explicit Variant(const QString & cat, const QString & key);
  explicit Variant(const QString & cat, const QString & key, const QVariant & iDefaultValue);
  void define_default_value(const QVariant & iDefaultValue);
  [[nodiscard]] QVariant read() const;
  void write(const QVariant & iValue) const;

private:
  QString mKey;
  QVariant mDefaultValue;
};


class Widget : public QObject
{
  Q_OBJECT
public:
  explicit Widget(const QString & cat, const QString & key);

  void register_widget_and_update_ui_from_setting(QWidget * const ipWidget, const QVariant & iDefaultValue);
  [[nodiscard]] QVariant read() const;
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
  QString mKey;
};


struct Main // namespace for main window data
{
  static inline QString category = "Main";

  static inline PosAndSize posAndSize{category};

  static inline Variant varVolume{category, "varVolume", 100};
};

struct Config  // namespace for the configuration window
{
  static inline QString category = "Configuration";

  static inline PosAndSize posAndSize{category};

  static inline Variant varPicturesPath{category, "varPicturesPath"};
  static inline Variant varMotPath{category, "varFilePath"};
  static inline Variant varEpgPath{category, "varEpgPath"};
  static inline Variant varSkipFile{category, "varSkipFile"};
  static inline Variant varTiiFile{category, "varTiiFile"};
  static inline Variant varBrowserAddress{category, "varBrowserAddress", "http://localhost"};
  static inline Variant varMapPort{category, "varMapPort", 8080};
  static inline Variant varLatitude{category, "varLatitude", 0};
  static inline Variant varLongitude{category, "varLongitude", 0};
  static inline Variant varSdrDevice{category, "varSdrDevice", "no device"};
  static inline Variant varDeviceFile{category, "varDeviceFile", ""};
  static inline Variant varDeviceUiVisible{category, "varDeviceUiVisible", true};
  static inline Variant varPresetName{category, "varPresetName", ""};
  static inline Variant varChannel{category, "varChannel", ""};

  static inline Widget cbCloseDirect{category, "cbCloseDirect"};
  static inline Widget cbUseStrongestPeak{category, "cbUseStrongestPeak"};
  static inline Widget cbUseNativeFileDialog{category, "cbUseNativeFileDialog"};
  static inline Widget cbUseUtcTime{category, "cbUseUtcTime"};
  static inline Widget cbGenXmlFromEpg{category, "cbGenXmlFromEpg"};
  static inline Widget cbAlwaysOnTop{category, "cbAlwaysOnTop"};
  static inline Widget cbManualBrowserStart{category, "cbManualBrowserStart"};
  static inline Widget cbSaveSlides{category, "cbSaveSlides"};
  static inline Widget cbSaveTransToCsv{category, "cbSaveTransToCsv"};
  static inline Widget cbUseDcAvoidance{category, "cbUseDcAvoidance"};
  static inline Widget cbUseDcRemoval{category, "cbUseDcRemoval"};
  static inline Widget cbShowNonAudioInServiceList{category, "cbShowNonAudioInService"};
  static inline Widget sbTiiThreshold{category, "sbTiiThreshold"};
  static inline Widget cbTiiCollisions{category, "cbTiiCollisions"};
  static inline Widget sbTiiSubId{category, "sbTiiSubId"};
  static inline Widget cbUrlClickable{category, "cbUrlClickable"};
  static inline Widget cbAutoIterTiiEntries{category, "cbAutoIterTiiEntries"};
  static inline Widget cmbSoundOutput{category, "cmbSoundOutput"};
};

struct SpectrumViewer // namespace for the spectrum window
{
  static inline QString category = "SpectrumViewer";

  static inline PosAndSize posAndSize{category};

  static inline Variant varUiVisible{category, "varUiVisible", false};
};

struct CirViewer // namespace for the CIR viewer window
{
  static inline QString category = "CirViewer";

  static inline PosAndSize posAndSize{category};

  static inline Variant varUiVisible{category, "varUiVisible", false};
};

struct TiiViewer // namespace for the TII list viewer window
{
  static inline QString category = "TiiViewer";

  static inline PosAndSize posAndSize{category};

  static inline Variant varUiVisible{category, "varUiVisible", false};
};

struct ServiceList // namespace for the TII list viewer window
{
  static inline QString category = "ServiceList";

  static inline Variant varSortCol{category, "varSortCol", 0};
  static inline Variant varSortDesc{category, "varSSortDesc", false};
};

struct TechDataViewer // namespace for the TII list viewer window
{
  static inline QString category = "TechDataViewer";

  static inline PosAndSize posAndSize{category};

  static inline Variant varUiVisible{category, "varUiVisible", false};
};

} // namespace Settings

#endif // SETTING_HELPER_H
