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
#include "ui_configuration.h"
#include "radio.h"
#include <QSettings>

Configuration::Configuration(RadioInterface * ipRI, QSettings * ipS) :
  Ui_configWidget(),
  mpSettings(ipS),
  mpRadioInterface(ipRI)
{
  setupUi(this);
  setFixedSize(455, 350);

  const int x = mpSettings->value("configWidget-x", 200).toInt();
  const int y = mpSettings->value("configWidget-y", 200).toInt();
  const int wi = mpSettings->value("configWidget-w", 200).toInt();
  const int he = mpSettings->value("configWidget-h", 150).toInt();

  resize(QSize(wi, he));
  move(QPoint(x, y));
  setWindowFlag(Qt::Tool, true); // does not generate a task bar icon

  sliderTest->hide(); // only used for test

  onTop->setChecked(mpSettings->value("onTop", 0).toInt() != 0);
  transmSelector->setChecked(mpSettings->value("saveLocations", 0).toInt() != 0);
  saveSlides->setChecked(mpSettings->value("saveSlides", 0).toInt() != 0);
  switchDelaySetting->setValue(mpSettings->value("switchDelay", RadioInterface::SWITCH_DELAY).toInt());
  utcSelector->setChecked(mpSettings->value("utcSelector", 0).toInt() != 0);
  saveServiceSelector->setChecked(mpSettings->value("has-presetName", 0).toInt() != 0);
  closeDirect->setChecked(mpSettings->value("closeDirect", 0).toInt() != 0);
  epg2xmlSelector->setChecked(mpSettings->value("epg2xml", 0).toInt() != 0);
  autoBrowser->setChecked(mpSettings->value("autoBrowser", 1).toInt() != 0);
  transmitterTags->setChecked(mpSettings->value("transmitterTags", 1).toInt() != 0);
  cbNativeFileDialog->setChecked(mpSettings->value("useNativeFileDialogs", 0).toInt() != 0);

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
}

void Configuration::save_position_and_config()
{
  mpSettings->setValue("configWidget-x", pos().x());
  mpSettings->setValue("configWidget-y", pos().y());
  mpSettings->setValue("configWidget-w", size().width());
  mpSettings->setValue("configWidget-h", size().height());

  mpSettings->setValue("utcSelector", utcSelector->isChecked() ? 1 : 0);
  mpSettings->setValue("epg2xml", epg2xmlSelector->isChecked() ? 1 : 0);
}

void Configuration::_slot_use_native_file_dialogs(bool)
{
  mpSettings->setValue("useNativeFileDialogs", cbNativeFileDialog->isChecked() ? 1 : 0);
}
