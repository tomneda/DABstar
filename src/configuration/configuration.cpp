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
#include "radio.h"
#include <QSettings>

Configuration::Configuration(RadioInterface * ipRI) :
  Ui_configWidget(),
  mpSH(&SettingHelper::get_instance()),
  mpRadioInterface(ipRI)
{
  setupUi(this);

  Settings::Config::posAndSize.read_widget_geometry(this, 100, 100, 700, 330, true);

  setWindowFlag(Qt::Tool, true); // does not generate a task bar icon

  sliderTest->hide(); // only used for test

  // register UI elements to setting manager and provide default setting
  Settings::Config::sbTiiThreshold.register_widget_and_update_ui_from_setting(sbTiiThreshold, 8);
  Settings::Config::sbTiiSubId.register_widget_and_update_ui_from_setting(sbTiiSubId, 2);
  Settings::Config::cbCloseDirect.register_widget_and_update_ui_from_setting(cbCloseDirect, 0);
  Settings::Config::cbUseStrongestPeak.register_widget_and_update_ui_from_setting(cbUseStrongestPeak, 0);
  Settings::Config::cbUseNativeFileDialog.register_widget_and_update_ui_from_setting(cbUseNativeFileDialog, 0);
  Settings::Config::cbUseUtcTime.register_widget_and_update_ui_from_setting(cbUseUtcTime, 0);
  Settings::Config::cbGenXmlFromEpg.register_widget_and_update_ui_from_setting(cbGenXmlFromEpg, 0);
  Settings::Config::cbAlwaysOnTop.register_widget_and_update_ui_from_setting(cbAlwaysOnTop, 0);
  Settings::Config::cbManualBrowserStart.register_widget_and_update_ui_from_setting(cbManualBrowserStart, 0);
  Settings::Config::cbSaveSlides.register_widget_and_update_ui_from_setting(cbSaveSlides, 0);
  Settings::Config::cbSaveTransToCsv.register_widget_and_update_ui_from_setting(cbSaveTransToCsv, 0);
  Settings::Config::cbUseDcAvoidance.register_widget_and_update_ui_from_setting(cbUseDcAvoidance, 0);
  Settings::Config::cbUseDcRemoval.register_widget_and_update_ui_from_setting(cbUseDcRemoval, 0);
  Settings::Config::cbShowNonAudioInServiceList.register_widget_and_update_ui_from_setting(cbShowNonAudioInServiceList, 0);
  Settings::Config::cbTiiCollisions.register_widget_and_update_ui_from_setting(cbTiiCollisions, 0);
  Settings::Config::cbUrlClickable.register_widget_and_update_ui_from_setting(cbUrlClickable, 2);
  Settings::Config::cbAutoIterTiiEntries.register_widget_and_update_ui_from_setting(cbAutoIterTiiEntries, 2);
  Settings::Config::cmbSoundOutput.register_widget_and_update_ui_from_setting(cmbSoundOutput, "default");

  QPalette lcdPalette;
#ifndef __MAC__
  lcdPalette.setColor(QPalette::Window, Qt::white);
  lcdPalette.setColor(QPalette::Base, Qt::black);
#endif

  cpuMonitor->setPalette(lcdPalette);
  cpuMonitor->setAutoFillBackground(true);

  connect(loadTableButton, &QPushButton::clicked, mpRadioInterface, &RadioInterface::slot_load_table);
  connect(sliderTest, &QSlider::valueChanged, mpRadioInterface, &RadioInterface::slot_test_slider);
  connect(dlTextButton, &QPushButton::clicked, mpRadioInterface,  &RadioInterface::slot_handle_dl_text_button);
  connect(cmbSoundOutput, qOverload<int>(&QComboBox::activated), mpRadioInterface, &RadioInterface::slot_set_stream_selector);
  connect(portSelector, &QPushButton::clicked, mpRadioInterface, &RadioInterface::slot_handle_port_selector);
  connect(set_coordinatesButton, &QPushButton::clicked, mpRadioInterface, &RadioInterface::slot_handle_set_coordinates_button);
  connect(cbUseDcAvoidance, &QCheckBox::clicked, mpRadioInterface, &RadioInterface::slot_handle_dc_avoidance_algorithm);
  connect(cbUseDcRemoval, &QCheckBox::clicked, mpRadioInterface, &RadioInterface::slot_handle_dc_removal);
  connect(sbTiiThreshold, &QSpinBox::valueChanged, mpRadioInterface, &RadioInterface::slot_handle_tii_threshold);
  connect(cbTiiCollisions, &QCheckBox::clicked, mpRadioInterface, &RadioInterface::slot_handle_tii_collisions);
  connect(sbTiiSubId, &QSpinBox::valueChanged, mpRadioInterface, &RadioInterface::slot_handle_tii_subid);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(cbUseStrongestPeak, &QCheckBox::checkStateChanged, mpRadioInterface, &RadioInterface::slot_use_strongest_peak);
  connect(cbActivateLogger, &QCheckBox::checkStateChanged, mpRadioInterface, &RadioInterface::slot_handle_logger_button);
  connect(cbActivateEti, &QCheckBox::checkStateChanged, mpRadioInterface, &RadioInterface::slot_handle_eti_active_selector);
#else
  connect(cbUseStrongestPeak, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_use_strongest_peak);
  connect(cbActivateLogger, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_logger_button);
  connect(cbActivateEti, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_eti_active_selector);
#endif
}

void Configuration::save_position_and_config()
{
  Settings::Config::posAndSize.write_widget_geometry(this);
}
