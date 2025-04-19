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

  SettingsManager::Configuration::posAndSize.read_widget_geometry(this, 100, 100, 700, 330, true);

  setWindowFlag(Qt::Tool, true); // does not generate a task bar icon

  sliderTest->hide(); // only used for test


  SettingsManager::Configuration::sbTiiThreshold.register_widget_and_update_ui_from_setting(tii_threshold, 7);
  SettingsManager::Configuration::cbCloseDirect.register_widget_and_update_ui_from_setting(cbCloseDirect, 0);
  SettingsManager::Configuration::cbUse_strongest_peak.register_widget_and_update_ui_from_setting(cbUse_strongest_peak, 0);
  SettingsManager::Configuration::cbUseNativeFileDialog.register_widget_and_update_ui_from_setting(cbUseNativeFileDialog, 0);
  SettingsManager::Configuration::cbUseUtcTime.register_widget_and_update_ui_from_setting(cbUseUtcTime, 0);
  SettingsManager::Configuration::cbGenXmlFromEpg.register_widget_and_update_ui_from_setting(cbGenXmlFromEpg, 0);
  SettingsManager::Configuration::cbAlwaysOnTop.register_widget_and_update_ui_from_setting(cbAlwaysOnTop, 0);
  SettingsManager::Configuration::cbManualBrowserStart.register_widget_and_update_ui_from_setting(cbManualBrowserStart, 0);
  SettingsManager::Configuration::cbSaveSlides.register_widget_and_update_ui_from_setting(cbSaveSlides, 0);
  SettingsManager::Configuration::cbSaveTransToCsv.register_widget_and_update_ui_from_setting(cbSaveTransToCsv, 0);
  SettingsManager::Configuration::cbUseDcAvoidance.register_widget_and_update_ui_from_setting(cbUseDcAvoidance, 0);
  SettingsManager::Configuration::cbUseDcRemoval.register_widget_and_update_ui_from_setting(cbUseDcRemoval, 0);
  SettingsManager::Configuration::cbShowNonAudioInServiceList.register_widget_and_update_ui_from_setting(cbShowNonAudioInServiceList, 0);
  SettingsManager::Configuration::cbTiiCollisions.register_widget_and_update_ui_from_setting(cbTiiCollisions, 0);
  SettingsManager::Configuration::cbUrlClickable.register_widget_and_update_ui_from_setting(cbUrlClickable, 2);
  SettingsManager::Configuration::cbAutoIterTiiEntries.register_widget_and_update_ui_from_setting(cbAutoIterTiiEntries, 2);

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
  connect(streamoutSelector, qOverload<int>(&QComboBox::activated), mpRadioInterface, &RadioInterface::slot_set_stream_selector);
  connect(portSelector, &QPushButton::clicked, mpRadioInterface, &RadioInterface::slot_handle_port_selector);
  connect(set_coordinatesButton, &QPushButton::clicked, mpRadioInterface, &RadioInterface::slot_handle_set_coordinates_button);
  connect(cbUseDcAvoidance, &QCheckBox::clicked, mpRadioInterface, &RadioInterface::slot_handle_dc_avoidance_algorithm);
  connect(cbUseDcRemoval, &QCheckBox::clicked, mpRadioInterface, &RadioInterface::slot_handle_dc_removal);
  connect(tii_threshold, &QSpinBox::valueChanged, mpRadioInterface, &RadioInterface::slot_handle_tii_threshold);
  connect(cbTiiCollisions, &QCheckBox::clicked, mpRadioInterface, &RadioInterface::slot_handle_tii_collisions);
  connect(tii_subid, &QSpinBox::valueChanged, mpRadioInterface, &RadioInterface::slot_handle_tii_subid);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(cbUse_strongest_peak, &QCheckBox::checkStateChanged, mpRadioInterface, &RadioInterface::slot_use_strongest_peak);
  connect(cbActivateLogger, &QCheckBox::checkStateChanged, mpRadioInterface, &RadioInterface::slot_handle_logger_button);
  connect(cbActivateEti, &QCheckBox::checkStateChanged, mpRadioInterface, &RadioInterface::slot_handle_eti_active_selector);
#else
  connect(cbUse_strongest_peak, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_use_strongest_peak);
  connect(cbActivateLogger, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_logger_button);
  connect(cbActivateEti, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_eti_active_selector);
#endif
}

void Configuration::save_position_and_config()
{
  SettingsManager::Configuration::posAndSize.write_widget_geometry(this);
}
