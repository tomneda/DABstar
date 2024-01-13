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

Configuration::Configuration(QSettings * ipSettings) :
  Ui_configWidget(),
  mpSettings(ipSettings)
{
  setupUi(this);
  setFixedSize(455, 350);

  const int x = mpSettings->value("configWidget-x", 200).toInt();
  const int y = mpSettings->value("configWidget-y", 200).toInt();
  const int wi = mpSettings->value("configWidget-w", 200).toInt();
  const int he = mpSettings->value("configWidget-h", 150).toInt();
  mpSettings.resize(QSize(wi, he));
  mpSettings.move(QPoint(x, y));
  mpSettings.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon

  sliderTest->hide(); // only used for test
}
