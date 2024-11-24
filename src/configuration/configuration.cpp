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
  setFixedSize(650, 330);

  mpSH->read_widget_geometry(SettingHelper::configWidget, this);

  setWindowFlag(Qt::Tool, true); // does not generate a task bar icon

  sliderTest->hide(); // only used for test

  mpSH->register_and_connect_ui_element<SettingHelper::cbCloseDirect>(cbCloseDirect);
  mpSH->register_and_connect_ui_element<SettingHelper::cbUseNewTiiDetector>(cbUseNewTiiDetector);
  mpSH->register_and_connect_ui_element<SettingHelper::cbUseNativeFileDialog>(cbUseNativeFileDialog);
  mpSH->register_and_connect_ui_element<SettingHelper::cbUseUtcTime>(cbUseUtcTime);
  mpSH->register_and_connect_ui_element<SettingHelper::cbGenXmlFromEpg>(cbGenXmlFromEpg);
  mpSH->register_and_connect_ui_element<SettingHelper::cbAlwaysOnTop>(cbAlwaysOnTop);
  mpSH->register_and_connect_ui_element<SettingHelper::cbManualBrowserStart>(cbManualBrowserStart);
  mpSH->register_and_connect_ui_element<SettingHelper::cbShowOnlyCurrTrans>(cbShowOnlyCurrTrans);
  mpSH->register_and_connect_ui_element<SettingHelper::cbSaveSlides>(cbSaveSlides);
  mpSH->register_and_connect_ui_element<SettingHelper::cbSaveTransToCsv>(cbSaveTransToCsv);
  mpSH->register_and_connect_ui_element<SettingHelper::cbUseDcAvoidance>(cbUseDcAvoidance);
  mpSH->register_and_connect_ui_element<SettingHelper::cbUseDcRemoval>(cbUseDcRemoval);
  mpSH->register_and_connect_ui_element<SettingHelper::cbShowNonAudioInServiceList>(cbShowNonAudioInServiceList);
  // cbActivateLogger is not stored in settings
  // cbActivateEti is not stored in settings

  _sync_ui_elements(false);

  QPalette lcdPalette;
#ifndef __MAC__
  lcdPalette.setColor(QPalette::Window, Qt::white);
  lcdPalette.setColor(QPalette::Base, Qt::black);
#endif

  cpuMonitor->setPalette(lcdPalette);
  cpuMonitor->setAutoFillBackground(true);

  connect(loadTableButton, &QPushButton::clicked, mpRadioInterface, &RadioInterface::slot_load_table);
  connect(cbUseNewTiiDetector, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_tii_detector_mode);
  connect(sliderTest, &QSlider::valueChanged, mpRadioInterface, &RadioInterface::slot_test_slider);
  connect(cbShowOnlyCurrTrans, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_transmitter_tags);
  connect(dlTextButton, &QPushButton::clicked, mpRadioInterface,  &RadioInterface::slot_handle_dl_text_button);
  connect(cbActivateLogger, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_logger_button);
  connect(streamoutSelector, qOverload<int>(&QComboBox::activated), mpRadioInterface, &RadioInterface::slot_set_stream_selector);
  connect(portSelector, &QPushButton::clicked, mpRadioInterface, &RadioInterface::slot_handle_port_selector);
  connect(set_coordinatesButton, &QPushButton::clicked, mpRadioInterface, &RadioInterface::slot_handle_set_coordinates_button);
  connect(cbActivateEti, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_eti_active_selector);
  connect(cbUseDcAvoidance, &QCheckBox::clicked, mpRadioInterface, &RadioInterface::slot_handle_dc_avoidance_algorithm);
  connect(cbUseDcRemoval, &QCheckBox::clicked, mpRadioInterface, &RadioInterface::slot_handle_dc_removal);
}

void Configuration::save_position_and_config()
{
  mpSH->write_widget_geometry(SettingHelper::configWidget, this);
  _sync_ui_elements(true);
}

void Configuration::_sync_ui_elements(const bool iWriteSettings)
{
  mpSH->sync_ui_state(SettingHelper::cbCloseDirect, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::cbUseNewTiiDetector, iWriteSettings);
  // cbActivateLogger is not stored in settings
  mpSH->sync_ui_state(SettingHelper::cbUseNativeFileDialog, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::cbUseUtcTime, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::cbGenXmlFromEpg, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::cbAlwaysOnTop, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::cbManualBrowserStart, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::cbShowOnlyCurrTrans, iWriteSettings);
  // cbActivateEti is not stored in settings
  mpSH->sync_ui_state(SettingHelper::cbSaveSlides, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::cbSaveTransToCsv, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::cbUseDcAvoidance, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::cbUseDcRemoval, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::cbShowNonAudioInServiceList, iWriteSettings);
}
