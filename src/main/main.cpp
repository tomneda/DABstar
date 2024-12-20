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
#include "setting-helper.h"
#include "radio.h"
#include <QApplication>
#include <QDir>
#include <QSettings>
#include <QString>
#include <QTranslator>
//#include <unistd.h>

int main(int argc, char ** argv)
{
  qRegisterMetaType<QVector<int>>("QVector<int>");  // windows needs that...

  const QString configPath = QDir::homePath() + "/.config/" APP_NAME "/";
  const QString initFileName = QDir::toNativeSeparators(configPath +  "settings02.ini");
  const QString dbFileName = QDir::toNativeSeparators(configPath + "servicelist02.db");

  // Default values
  int32_t dataPort = 8888;
  QString altFreqList = "";

  QCoreApplication::setApplicationName(PRJ_NAME);
  QCoreApplication::setApplicationVersion(QString(PRJ_VERS) + " Git: " + GITHASH);

  int opt;
  while ((opt = getopt(argc, argv, "C:P:Q:A:TM:F:")) != -1)
  {
    switch (opt)
    {
    case 'P': dataPort = atoi(optarg);
      break;
    case 'A': altFreqList = optarg;
      break;
    default: ;
    }
  }

  auto dabSettings(std::make_unique<QSettings>(initFileName, QSettings::IniFormat));
  SettingHelper::get_instance(dabSettings.get()); // create instance of setting helper

  QApplication a(argc, argv);

  qSetMessagePattern("[%{time yyyy-MM-dd hh:mm:ss.zzz}] %{message}");

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

  QApplication::setWindowIcon(QIcon(":res/logo/dabstar.png")); // used for all dialog windows except main window (is overwritten)

  auto radioInterface(std::make_unique<RadioInterface>(dabSettings.get(), dbFileName, altFreqList, dataPort, nullptr));
  radioInterface->show();

  QApplication::exec();

  fflush(stdout);
  fflush(stderr);
  qDebug("It is done\n");
  return 0;
}
