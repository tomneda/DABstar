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
#include <QSettings>
#include <QDir>


Configuration::Configuration(DabRadio * ipRI) :
  Ui_configWidget(),
  mpRadioInterface(ipRI)
{
  setupUi(this);

  Settings::Config::posAndSize.read_widget_geometry(this, 720, 276, true);

  setWindowFlag(Qt::Tool, true); // does not generate a task bar icon

  sliderTest->hide(); // only used for test

  // register UI elements to setting manager and provide default setting
  Settings::Config::sbTiiThreshold.register_widget_and_update_ui_from_setting(sbTiiThreshold, 8);
  Settings::Config::sbTiiSubId.register_widget_and_update_ui_from_setting(sbTiiSubId, 2);
  Settings::Config::cbCloseDirect.register_widget_and_update_ui_from_setting(cbCloseDirect, 0);
  Settings::Config::cbUseStrongestPeak.register_widget_and_update_ui_from_setting(cbUseStrongestPeak, 0);
  Settings::Config::cbUseNativeFileDialog.register_widget_and_update_ui_from_setting(cbUseNativeFileDialog, 0);
  Settings::Config::cbUseUtcTime.register_widget_and_update_ui_from_setting(cbUseUtcTime, 0);
  Settings::Config::cbAlwaysOnTop.register_widget_and_update_ui_from_setting(cbAlwaysOnTop, 0);
  Settings::Config::cbManualBrowserStart.register_widget_and_update_ui_from_setting(cbManualBrowserStart, 0);
  Settings::Config::cmbMotObjectSaving.register_widget_and_update_ui_from_setting(cmbMotObjectSaving, "");
  Settings::Config::cmbEpgObjectSaving.register_widget_and_update_ui_from_setting(cmbEpgObjectSaving, "");
  Settings::Config::cbSaveTransToCsv.register_widget_and_update_ui_from_setting(cbSaveTransToCsv, 0);
  Settings::Config::cbUseDcAvoidance.register_widget_and_update_ui_from_setting(cbUseDcAvoidance, 0);
  Settings::Config::cbUseDcRemoval.register_widget_and_update_ui_from_setting(cbUseDcRemoval, 0);
  Settings::Config::cbTiiCollisions.register_widget_and_update_ui_from_setting(cbTiiCollisions, 0);
  Settings::Config::cbUrlClickable.register_widget_and_update_ui_from_setting(cbUrlClickable, 2);
  Settings::Config::cbAutoIterTiiEntries.register_widget_and_update_ui_from_setting(cbAutoIterTiiEntries, 2);
  Settings::Config::cmbSoundOutput.register_widget_and_update_ui_from_setting(cmbSoundOutput, "default");

  // QDir tempPath = QDir::tempPath();
  QDir tempPath = QDir::homePath();
  tempPath.setPath(tempPath.filePath(PRJ_NAME));
  const QString tempPicPath = tempPath.filePath("PIC").append('/');
  const QString tempMotPath = tempPath.filePath("MOT").append('/');
  const QString tempEpgPath = tempPath.filePath("EPG").append('/');

  Settings::Config::varPicturesPath.define_default_value(tempPicPath);
  Settings::Config::varMotPath.define_default_value(tempMotPath);
  Settings::Config::varEpgPath.define_default_value(tempEpgPath);
  Settings::Config::varSkipFile.define_default_value("");
  Settings::Config::varTiiFile.define_default_value("");

  QPalette lcdPalette;
#ifndef __MAC__
  lcdPalette.setColor(QPalette::Window, Qt::white);
  lcdPalette.setColor(QPalette::Base, Qt::black);
#endif

  connect(loadTableButton, &QPushButton::clicked, mpRadioInterface, &DabRadio::slot_load_table);
  connect(sliderTest, &QSlider::valueChanged, mpRadioInterface, &DabRadio::slot_test_slider);
  connect(dlTextButton, &QPushButton::clicked, mpRadioInterface,  &DabRadio::slot_handle_dl_text_button);
  connect(cmbSoundOutput, qOverload<i32>(&QComboBox::activated), mpRadioInterface, &DabRadio::slot_set_stream_selector);
  connect(portSelector, &QPushButton::clicked, mpRadioInterface, &DabRadio::slot_handle_port_selector);
  connect(set_coordinatesButton, &QPushButton::clicked, mpRadioInterface, &DabRadio::slot_handle_set_coordinates_button);
  connect(cbUseDcAvoidance, &QCheckBox::clicked, mpRadioInterface, &DabRadio::slot_handle_dc_avoidance_algorithm);
  connect(cbUseDcRemoval, &QCheckBox::clicked, mpRadioInterface, &DabRadio::slot_handle_dc_removal);
  connect(sbTiiThreshold, &QSpinBox::valueChanged, mpRadioInterface, &DabRadio::slot_handle_tii_threshold);
  connect(cbTiiCollisions, &QCheckBox::clicked, mpRadioInterface, &DabRadio::slot_handle_tii_collisions);
  connect(sbTiiSubId, &QSpinBox::valueChanged, mpRadioInterface, &DabRadio::slot_handle_tii_subid);
  connect(cmbMotObjectSaving, qOverload<i32>(&QComboBox::activated), mpRadioInterface, &DabRadio::slot_handle_mot_saving_selector);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(cbUseStrongestPeak, &QCheckBox::checkStateChanged, mpRadioInterface, &DabRadio::slot_use_strongest_peak);
  connect(cbActivateLogger, &QCheckBox::checkStateChanged, mpRadioInterface, &DabRadio::slot_handle_logger_button);
#else
  connect(cbUseStrongestPeak, &QCheckBox::stateChanged, mpRadioInterface, &DabRadio::slot_use_strongest_peak);
  connect(cbActivateLogger, &QCheckBox::stateChanged, mpRadioInterface, &DabRadio::slot_handle_logger_button);
#endif
}

void Configuration::save_position_and_config()
{
  Settings::Config::posAndSize.write_widget_geometry(this);
}
