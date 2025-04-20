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
  explicit Widget(const QString & key);

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
  QString mCat;
};

struct General // namespace for main window data
{
  static inline PosAndSize posAndSize{""};
};

struct Spectrum // namespace for the spectrum window
{
  #define catSpectrumViewer "SpectrumViewer/" // did not find nicer way to declare that once
  static inline PosAndSize posAndSize{catSpectrumViewer};
};

struct Config  // namespace for the configuration window
{
  #define catConfiguration "Configuration/" // did not find nicer way to declare that once
  static inline PosAndSize posAndSize{catConfiguration};

  static inline Variant varPicturesPath{catConfiguration "varPicturesPath"};
  static inline Variant varMotPath{catConfiguration "varFilePath"};
  static inline Variant varEpgPath{catConfiguration "varEpgPath"};
  static inline Variant varSkipFile{catConfiguration "varSkipFile"};
  static inline Variant varTiiFile{catConfiguration "varTiiFile"};
  static inline Variant varBrowserAddress{catConfiguration "varBrowserAddress", "http://localhost"};
  static inline Variant varMapPort{catConfiguration "varMapPort", 8080};
  static inline Variant varLatitude{catConfiguration "varLatitude", 0};
  static inline Variant varLongitude{catConfiguration "varLongitude", 0};
  static inline Variant varSdrDevice{catConfiguration "varSdrDevice", "no device"};
  static inline Variant varDeviceFile{catConfiguration "varDeviceFile", ""};
  static inline Variant varDeviceVisible{catConfiguration "varDeviceVisible", true};
  static inline Variant varSpectrumVisible{catConfiguration "varSpectrumVisible", false};
  static inline Variant varCirVisible{catConfiguration "varCirVisible", false};
  static inline Variant varTechDataVisible{catConfiguration "varTechDataVisible", false};
  static inline Variant varShowDeviceWidget{catConfiguration "varShowDeviceWidget", false};
  static inline Variant varPresetName{catConfiguration "varPresetName", ""};
  static inline Variant varChannel{catConfiguration "varChannel", ""};

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
  #define catCirViewer "CirViewer/" // did not find nicer way to declare that once
  static inline PosAndSize posAndSize{catCirViewer};
};

struct TiiList // namespace for the TII list viewer window
{
  #define catTiiList "TiiList/" // did not find nicer way to declare that once
  static inline PosAndSize posAndSize{catTiiList};

  static inline Variant varShowTiiList{catConfiguration "varShowTiiList", false};
};

struct ServiceList // namespace for the TII list viewer window
{
  #define catServiceList "ServiceList/" // did not find nicer way to declare that once
  static inline Variant varSortCol{catServiceList "varSortCol", 0};
  static inline Variant varSortDesc{catServiceList "varSSortDesc", false};
};

} // namespace Settings

#endif // SETTING_HELPER_H
