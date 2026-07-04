/*
 * Copyright (c) 2024
 * Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is acknowledged.
 */

#pragma once

#include "device_handler.h"
#include <memory>
#include <QString>
#include <QStringList>

class QSettings;

class IDeviceSelector
{
public:
  virtual ~IDeviceSelector() = default;

  virtual QStringList get_device_name_list() const = 0;
  virtual std::unique_ptr<IDeviceHandler> create_device(const QString & iDeviceNameOrFileName, bool iIsFileDevice, bool iSuppressWarnings) = 0;
  virtual const QString & get_message() const = 0;
};

// Factory function — declared here, defined in the devices library
std::unique_ptr<IDeviceSelector> create_device_selector(QSettings * ipSettings);
