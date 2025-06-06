/*
 * Copyright (c) 2024
 * Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is acknowledged.
 */

#include <memory>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "device-handler.h"
#include "openfiledialog.h"

class QSettings;

class DeviceSelector : public QWidget
{
  Q_OBJECT
public:
  explicit DeviceSelector(QSettings * ipSettings);
  ~DeviceSelector() override = default;

  QStringList get_device_name_list() const;
  std::unique_ptr<IDeviceHandler> create_device(const QString & s, bool & oRealDevice);
  bool reset_file_input_last_file(const QString & iDeviceName); // returns true if the file was reset
  const QString & get_device_name() const { return mCurDeviceName; }

private:
  QSettings * const mpSettings;
  const QString mVersionStr{ PRJ_VERS };
  OpenFileDialog mOpenFileDialog;
  QString mCurDeviceName;

  std::unique_ptr<IDeviceHandler> _create_device(const QString & iDeviceName, bool & oRealDevice);
};


