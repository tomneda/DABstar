/*
 * Copyright (c) 2024 by Thomas Neder (https://github.com/tomneda)
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

#include "configuration.h"
#include "setting-helper.h"
#include "ui_configuration.h"
#include "dabradio.h"
#include <QDir>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>

class CoordDoubleValidator : public QDoubleValidator
{
  using QDoubleValidator::QDoubleValidator;

  QValidator::State validate(QString & input, i32 &) const override
  {
    const QRegularExpression regExpAllowed("^-?\\d*(\\.\\d*)?$");
    if (const QRegularExpressionMatch match = regExpAllowed.match(input); !match.hasMatch())
    {
      return Invalid;
    }
    const QLocale englishLocale(QLocale::English, QLocale::UnitedStates);
    const QLocale previousLocale = QLocale::system();
    QLocale::setDefault(englishLocale);
    const f64 val = input.toDouble();
    QLocale::setDefault(previousLocale);
    if (val < bottom() || val > top())
    {
      return Invalid;
    }
    return Acceptable;
  }
};


Configuration::Configuration(DabRadio * ipRI) :
  Ui_configWidget(),
  mpRadioInterface(ipRI)
{
  setupUi(this);

  Settings::Config::posAndSize.read_widget_geometry(this, false, true);

  setWindowFlag(Qt::Tool, true); // does not generate a task bar icon

  // register UI elements to setting manager and provide default setting
  Settings::Config::sbTiiThreshold.register_widget_and_update_ui_from_setting(sbTiiThreshold, 8);
  Settings::Config::sbTiiSubId.register_widget_and_update_ui_from_setting(sbTiiSubId, 2);
  Settings::Config::cbCloseDirect.register_widget_and_update_ui_from_setting(cbCloseDirect, 2);
  Settings::Config::cbUseStrongestPeak.register_widget_and_update_ui_from_setting(cbUseStrongestPeak, 0);
  Settings::Config::cbUseNativeFileDialog.register_widget_and_update_ui_from_setting(cbUseNativeFileDialog, 0);
  Settings::Config::cbUseNativeIqFormat.register_widget_and_update_ui_from_setting(cbUseNativeIqFormat, 2);
  Settings::Config::cbUseUtcTime.register_widget_and_update_ui_from_setting(cbUseUtcTime, 0);
  Settings::Config::cbAlwaysOnTop.register_widget_and_update_ui_from_setting(cbAlwaysOnTop, 0);
  Settings::Config::cbManualBrowserStart.register_widget_and_update_ui_from_setting(cbManualBrowserStart, 0);
  Settings::Config::cmbMotObjectSaving5.register_widget_and_update_ui_from_setting(cmbMotObjectSaving5, "");
  Settings::Config::cmbEpgObjectSaving5.register_widget_and_update_ui_from_setting(cmbEpgObjectSaving5, "");
  Settings::Config::cbSaveTransToCsv.register_widget_and_update_ui_from_setting(cbSaveTransToCsv, 0);
  Settings::Config::cbUseDcAvoidance.register_widget_and_update_ui_from_setting(cbUseDcAvoidance, 0);
  Settings::Config::cbDoDcCorrOnly.register_widget_and_update_ui_from_setting(cbDoDcCorrOnly, 0);
  Settings::Config::cbDoDcAndIqCorr.register_widget_and_update_ui_from_setting(cbDoDcAndIqCorr, 0);
  Settings::Config::cbTiiCollisions.register_widget_and_update_ui_from_setting(cbTiiCollisions, 0);
  Settings::Config::cbUrlClickable.register_widget_and_update_ui_from_setting(cbUrlClickable, 2);
  Settings::Config::cbAutoIterTiiEntries.register_widget_and_update_ui_from_setting(cbAutoIterTiiEntries, 2);
  Settings::Config::cmbSoundOutput.register_widget_and_update_ui_from_setting(cmbSoundOutput, "default");
  Settings::Config::cbCheckForUpdates.register_widget_and_update_ui_from_setting(cbCheckForUpdates, 2);
  Settings::Config::sbUpdateCheckDays.register_widget_and_update_ui_from_setting(sbUpdateCheckDays, 1);
  Settings::Config::sbPeakLevelDelay.register_widget_and_update_ui_from_setting(sbPeakLevelDelay, 4);

  const QString defaultBasePath = QDir(QDir::homePath()).filePath(PRJ_NAME"5") + "/";  // as the file structure with v5 has changed, add a "5" to the default path
  Settings::Config::varDataBasePath5.define_default_value(defaultBasePath);
  lblDataBasePath->setText(Settings::Config::varDataBasePath5.read().toString());

  Settings::Config::varSkipFile.define_default_value("");
  Settings::Config::varTiiFile.define_default_value("");

  connect(btnSelectBasePath, &QPushButton::clicked, this, &Configuration::_slot_select_base_path);
  connect(cbDoDcCorrOnly, &QCheckBox::clicked, this, &Configuration::_slot_handle_dc_corr);
  connect(cbDoDcAndIqCorr, &QCheckBox::clicked, this, &Configuration::_slot_handle_dc_and_iq_corr);
  connect(leLatitudeVal, &QLineEdit::editingFinished, this, &Configuration::_slot_coordinate_edited);
  connect(leLongitudeVal, &QLineEdit::editingFinished, this, &Configuration::_slot_coordinate_edited);
  connect(leMapPortVal, &QLineEdit::editingFinished, this, &Configuration::_slot_map_port_edited);

  leLatitudeVal->setValidator(new CoordDoubleValidator(-90.0, 90.0, 5, leLatitudeVal));
  leLatitudeVal->setText(Settings::Config::varLatitude.read().toString());

  leLongitudeVal->setValidator(new CoordDoubleValidator(-180.0, 180.0, 5, leLongitudeVal));
  leLongitudeVal->setText(Settings::Config::varLongitude.read().toString());

  leMapPortVal->setValidator(new QIntValidator(1, 65535, leMapPortVal));
  leMapPortVal->setText(Settings::Config::varMapPort.read().toString());
}

void Configuration::save_position_and_config()
{
  Settings::Config::posAndSize.write_widget_geometry(this);
}

void Configuration::_slot_handle_dc_corr(const bool iChecked)
{
  if (iChecked)
  {
    cbDoDcAndIqCorr->setChecked(false); // fields are mutual excluded to be active
  }
  emit signal_handle_dc_and_iq_corr(iChecked, false);
}

void Configuration::_slot_map_port_edited()
{
  Settings::Config::varMapPort.write(leMapPortVal->text());
}

void Configuration::_slot_coordinate_edited()
{
  Settings::Config::varLatitude.write(leLatitudeVal->text());
  Settings::Config::varLongitude.write(leLongitudeVal->text());
  emit signal_coordinates_changed();
}

void Configuration::_slot_handle_dc_and_iq_corr(const bool iChecked)
{
  if (iChecked)
  {
    cbDoDcCorrOnly->setChecked(false); // fields are mutual excluded to be active
  }
  emit signal_handle_dc_and_iq_corr(iChecked, iChecked);
}

void Configuration::_slot_select_base_path()
{
  const QString current = Settings::Config::varDataBasePath5.read().toString();
  const bool useNativeFileDialog = Settings::Config::cbUseNativeFileDialog.read().toBool();

  QString dir = QFileDialog::getExistingDirectory(this, "Select Data Base Path", current, QFileDialog::ShowDirsOnly | (useNativeFileDialog ? QFileDialog::Options() : QFileDialog::DontUseNativeDialog));

  if (dir.isEmpty())
  {
    return;
  }
  if (!dir.endsWith('/'))
  {
    dir += '/';
  }
  Settings::Config::varDataBasePath5.write(dir);
  lblDataBasePath->setText(dir);
  emit signal_data_base_path_changed();
}
