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
#include "dabradio.h"
#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QSettings>
#include <QString>
#include <QTranslator>
//#ifdef _WIN32
//  #include <windows.h>
//#endif

i32 main(i32 argc, char ** argv)
{
// #ifdef _WIN32
//   // Enable ANSI escape codes for Windows console
//   HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
//   DWORD dwMode = 0;
//   GetConsoleMode(hOut, &dwMode);
//   dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
//   SetConsoleMode(hOut, dwMode);
// #endif

  qRegisterMetaType<QVector<i32>>("QVector<i32>");  // windows needs that...

  const QString configPath = QDir::homePath() + "/.config/" APP_NAME "/";
  const QString initFileName02 = QDir::toNativeSeparators(configPath +  "settings02.ini");
  const QString initFileName03 = QDir::toNativeSeparators(configPath +  "settings03.ini");
  const QString dbFileName = QDir::toNativeSeparators(configPath + "servicelist02.db");

  // Default values
  i32 dataPort = 8888;
  QString altFreqList = "";

  QCoreApplication::setApplicationName(PRJ_NAME);
  QCoreApplication::setApplicationVersion(QString(PRJ_VERS) + " Git: " + GITHASH);

  i32 opt;
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

  const auto dabSettings03(std::make_unique<QSettings>(initFileName03, QSettings::IniFormat));
  Settings::Storage::instance(dabSettings03.get()); // create instance of settingstorage

  QApplication a(argc, argv);

  // qSetMessagePattern("[%{time yyyy-MM-dd hh:mm:ss.zzz}] %{message}");
  // qSetMessagePattern("[%{time hh:mm:ss.zzz}] [%{type}] [%{function}]: %{message}");
#ifdef NDEBUG
  qSetMessagePattern("\033[94m[%{time hh:mm:ss.zzz}] \033[0m" // Blue prefix
#else
  qSetMessagePattern("\033[94m[%{time hh:mm:ss.zzz}] [%{type}] [%{function}] \033[0m" // Blue prefix
#endif
                     "%{if-debug}\033[97m%{endif}"       // White for debug
                     "%{if-info}\033[96m%{endif}"        // Cyan for info
                     "%{if-warning}\033[93m%{endif}"     // Yellow for warning
                     "%{if-critical}\033[91m%{endif}"    // Red for critical
                     "%{if-fatal}\033[91;1m%{endif}"     // Bright red for fatal
                     "%{message}\033[0m");               // Reset color at end
                     
  // qDebug() << "This is a debug message";
  // qInfo() << "This is an info message";
  // qWarning() << "This is a warning message";
  // qCritical() << "This is a critical message";
  // qFatal() << "This is a fatal message";

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

  QApplication::setWindowIcon(QIcon(":res/logo/dabstar.png"));

  // we changed the setting file name, so inform the user who already used DABstar in a former version
  if (!QFile::exists(initFileName03) && QFile::exists(initFileName02)) // no new file yet but had already the former file?
  {
    QMessageBox::warning(nullptr, "Warning", "The setting configurations have changed. "
                                             "Therefore, some settings need to be re-entered, "
                                             "such as the map coordinates.");
  }

  const auto radioInterface(std::make_unique<DabRadio>(dabSettings03.get(), dbFileName, altFreqList, dataPort, nullptr));
  radioInterface->show();

  QApplication::exec();

  fflush(stdout);
  fflush(stderr);
  qDebug("It is done\n");
  return 0;
}
