/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB 
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *      Main program
 */
#include "radio.h"
#include <QApplication>
#include <QDir>
#include <QSettings>
#include <QString>
#include <QTranslator>
#include <unistd.h>

int main(int argc, char ** argv)
{
  const QString configPath = QDir::homePath() + "/.config/" APP_NAME "/";
  const QString initFileName = QDir::toNativeSeparators(configPath +  "settings.ini");
  const QString dbFileName = QDir::toNativeSeparators(configPath + "service_list_v01.db");

  // Default values
  int32_t dataPort = 8888;
  int32_t clockPort = 8889;
  int opt;
  QString freqExtension = "";
  bool error_report = false;
  int fmFrequency = 110000;

  QCoreApplication::setApplicationName(PRJ_NAME);
  QCoreApplication::setApplicationVersion(QString(PRJ_VERS) + " Git: " + GITHASH);

  while ((opt = getopt(argc, argv, "C:P:Q:A:TM:F:")) != -1)
  {
    switch (opt)
    {
    case 'P': dataPort = atoi(optarg);
      break;
    case 'C': clockPort = atoi(optarg);
      break;
    case 'A': freqExtension = optarg;
      break;
    case 'T': error_report = true;
      break;
    case 'F': fmFrequency = atoi(optarg);
      break;
    default: ;
    }
  }

  auto dabSettings(std::make_unique<QSettings>(initFileName, QSettings::IniFormat));

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
  QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

  QApplication a(argc, argv);

  // read stylesheet from resource file
  if (QFile file(":res/globstyle.qss");
      file.open(QFile::ReadOnly | QFile::Text))
  {
    a.setStyleSheet(file.readAll());
    file.close();
  }
  else
  {
    qFatal("Could not open stylesheet resource");
  }

  QApplication::setWindowIcon(QIcon(":res/dabstar128x128.png")); // used for all dialog windows except main window (is overwritten)

  auto radioInterface(std::make_unique<RadioInterface>(dabSettings.get(), dbFileName, freqExtension, error_report, dataPort, clockPort, fmFrequency, nullptr));
  radioInterface->show();

  QApplication::exec();

  fflush(stdout);
  fflush(stderr);
  qDebug("It is done\n");
  return 0;
}
