/*
 * Copyright (c) 2024
 * Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is acknowledged.
 */

#include <QString>
#include <QStringList>
#include <QWidget>

#include "device-handler.h"

class QSettings;

class DeviceSelector : public QWidget
{
  Q_OBJECT
public:
  explicit DeviceSelector(QSettings * ipSettings);
  ~DeviceSelector() override = default;

  QStringList get_device_name_list();
  IDeviceHandler * create_device(const QString & s, bool & oRealDevice);

private:
  QSettings * const mpSettings;
};


