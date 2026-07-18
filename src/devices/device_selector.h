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

#include "device_selector_if.h"
#include "openfiledialog.h"
#include <QString>

class QSettings;

class DeviceSelector : public IDeviceSelector
{
public:
  explicit DeviceSelector(QSettings * ipSettings);
  ~DeviceSelector() override;

  QStringList get_device_name_list() const override;
  std::unique_ptr<IDeviceHandler> create_device(const QString & iDeviceNameOrFileName, bool iIsFileDevice, bool iSuppressWarnings) override;
  const QString & get_message() const override { return mMessage; }

private:
  QSettings * const mpSettings;
  const QString mVersionStr{ PRJ_VERS };
  OpenFileDialog mOpenFileDialog;
  mutable QString mMessage;

  std::unique_ptr<IDeviceHandler> _create_device(const QString & iDeviceNameOrFileName, bool iIsFileDevice) const;
  std::unique_ptr<IDeviceHandler> _open_input_file_device_from_file_type(const QString & iFilepath) const;
};
