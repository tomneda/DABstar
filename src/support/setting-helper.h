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
  static QString category;

  static PosAndSize posAndSize;

  static Variant varVolume;
};

struct Config  // namespace for the configuration window
{
  static QString category;

  static PosAndSize posAndSize;

  static Variant varPicturesPath;
  static Variant varMotPath;
  static Variant varEpgPath;
  static Variant varSkipFile;
  static Variant varTiiFile;
  static Variant varBrowserAddress;
  static Variant varMapPort;
  static Variant varLatitude;
  static Variant varLongitude;
  static Variant varSdrDevice;
  static Variant varDeviceFile;
  static Variant varDeviceUiVisible;
  static Variant varPresetName;
  static Variant varChannel;

  static Widget cbCloseDirect;
  static Widget cbUseStrongestPeak;
  static Widget cbUseNativeFileDialog;
  static Widget cbUseUtcTime;
  static Widget cbGenXmlFromEpg;
  static Widget cbAlwaysOnTop;
  static Widget cbManualBrowserStart;
  static Widget cbSaveSlides;
  static Widget cbSaveTransToCsv;
  static Widget cbUseDcAvoidance;
  static Widget cbUseDcRemoval;
  static Widget cbShowNonAudioInServiceList;
  static Widget sbTiiThreshold;
  static Widget cbTiiCollisions;
  static Widget sbTiiSubId;
  static Widget cbUrlClickable;
  static Widget cbAutoIterTiiEntries;
  static Widget cmbSoundOutput;
};

struct SpectrumViewer // namespace for the spectrum window
{
  static QString category;

  static PosAndSize posAndSize;

  static Variant varUiVisible;
};

struct CirViewer // namespace for the CIR viewer window
{
  static QString category;

  static PosAndSize posAndSize;

  static Variant varUiVisible;
};

struct TiiViewer // namespace for the TII list viewer window
{
  static QString category;

  static PosAndSize posAndSize;

  static Variant varUiVisible;
};

struct ServiceList // namespace for the TII list viewer window
{
  static QString category;

  static Variant varSortCol;
  static Variant varSortDesc;
};

struct TechDataViewer // namespace for the TII list viewer window
{
  static QString category;

  static PosAndSize posAndSize;

  static Variant varUiVisible;
};

} // namespace Settings

#endif // SETTING_HELPER_H
