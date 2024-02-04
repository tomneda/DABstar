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
  setFixedSize(455, 350);

  mpSH->read_widget_geometry(SettingHelper::configWidget, this);

  setWindowFlag(Qt::Tool, true); // does not generate a task bar icon

  sliderTest->hide(); // only used for test

  mpSH->register_ui_element<SettingHelper::switchDelay>(switchDelaySetting);
  mpSH->register_ui_element<SettingHelper::closeDirect>(closeDirect);
  mpSH->register_ui_element<SettingHelper::tiiDetector>(tii_detectorMode);
  mpSH->register_ui_element<SettingHelper::useNativeFileDialogs>(cbNativeFileDialog);
  mpSH->register_ui_element<SettingHelper::utcSelector>(utcSelector);
  mpSH->register_ui_element<SettingHelper::epg2xml>(epg2xmlSelector);
  mpSH->register_ui_element<SettingHelper::onTop>(onTop);
  mpSH->register_ui_element<SettingHelper::autoBrowser>(autoBrowser);
  mpSH->register_ui_element<SettingHelper::transmitterTags>(transmitterTags);
  mpSH->register_ui_element<SettingHelper::saveSlides>(saveSlides);
  mpSH->register_ui_element<SettingHelper::saveLocations>(transmSelector);
  mpSH->register_ui_element<SettingHelper::dcAvoidance>(cbDcAvoidance);
  mpSH->register_ui_element<SettingHelper::dcRemoval>(cbDcRemovalFilter);
  // loggerButton is not stored in settings
  // eti_activeSelector is not stored in settings

  _sync_ui_elements(false);

  QPalette lcdPalette;
#ifndef __MAC__
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
  lcdPalette.setColor(QPalette::Window, Qt::white);
#else
  lcdPalette. setColor (QPalette::Background, Qt::white);
#endif
  lcdPalette.setColor(QPalette::Base, Qt::black);
#endif

  cpuMonitor->setPalette(lcdPalette);
  cpuMonitor->setAutoFillBackground(true);

  connect(loadTableButton, &QPushButton::clicked, mpRadioInterface, &RadioInterface::slot_load_table);
  connect(tii_detectorMode, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_tii_detector_mode);
  connect(onTop, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_on_top);
  connect(transmSelector, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_transm_selector);
  connect(saveSlides, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_save_slides);
  connect(sliderTest, &QSlider::valueChanged, mpRadioInterface, &RadioInterface::slot_test_slider);
  connect(autoBrowser, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_auto_browser);
  connect(transmitterTags, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_transmitter_tags);
  connect(dlTextButton, &QPushButton::clicked, mpRadioInterface,  &RadioInterface::slot_handle_dl_text_button);
  connect(loggerButton, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_logger_button);
  connect(streamoutSelector, qOverload<int>(&QComboBox::activated), mpRadioInterface, &RadioInterface::slot_set_stream_selector);
  connect(portSelector, &QPushButton::clicked, mpRadioInterface, &RadioInterface::slot_handle_port_selector);
  connect(set_coordinatesButton, &QPushButton::clicked, mpRadioInterface, &RadioInterface::slot_handle_set_coordinates_button);
  connect(eti_activeSelector, &QCheckBox::stateChanged, mpRadioInterface, &RadioInterface::slot_handle_eti_active_selector);
  connect(cbNativeFileDialog, &QCheckBox::stateChanged, this, &Configuration::_slot_use_native_file_dialogs);
  connect(cbDcAvoidance, &QCheckBox::clicked, mpRadioInterface, &RadioInterface::slot_handle_dc_avoidance_algorithm);
  connect(cbDcRemovalFilter, &QCheckBox::clicked, mpRadioInterface, &RadioInterface::slot_handle_dc_removal);
}

void Configuration::save_position_and_config()
{
  mpSH->write_widget_geometry(SettingHelper::configWidget, this);
  _sync_ui_elements(true);
}

void Configuration::_slot_use_native_file_dialogs(bool iChecked)
{
  mpSH->write(SettingHelper::EElem::useNativeFileDialogs, iChecked);
}

void Configuration::_sync_ui_elements(const bool iWriteSettings)
{
  mpSH->sync_ui_state(SettingHelper::switchDelay, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::closeDirect, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::tiiDetector, iWriteSettings);
  // loggerButton is not stored in settings
  mpSH->sync_ui_state(SettingHelper::useNativeFileDialogs, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::utcSelector, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::epg2xml, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::onTop, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::autoBrowser, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::transmitterTags, iWriteSettings);
  // eti_activeSelector is not stored in settings
  mpSH->sync_ui_state(SettingHelper::saveSlides, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::saveLocations, iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::dcAvoidance,  iWriteSettings);
  mpSH->sync_ui_state(SettingHelper::dcRemoval, iWriteSettings);
}
