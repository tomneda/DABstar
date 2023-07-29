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

static const QString styleSheet =
//#include "./stylesheets/Adaptic.qss"
//#include "./stylesheets/Combinear.qss"
#include "./stylesheets/CombinearCompact.qss"
//#include "./stylesheets/Fibers.qss"
;

//void setTranslator(QTranslator *, QString Language);

int main(int argc, char ** argv)
{
  const QString configPath = QDir::homePath() + "/.config/" APP_NAME "/";
  const QString initFileName = QDir::toNativeSeparators(configPath +  "settings.ini");
  const QString presetsFileName = QDir::toNativeSeparators(configPath + "presets.xml");

  RadioInterface * MyRadioInterface;

  // Default values
  QSettings * dabSettings;// ini file
  int32_t dataPort = 8888;
  int32_t clockPort = 8889;
  int opt;
  QString freqExtension = "";
  bool error_report = false;
  int fmFrequency = 110000;

  //QTranslator theTranslator;
  //QCoreApplication::setOrganizationName("Lazy Chair Computing");
  //QCoreApplication::setOrganizationDomain("Lazy Chair Computing");
  QCoreApplication::setApplicationName(PRJ_NAME);
  QCoreApplication::setApplicationVersion(QString(PRJ_VERS) + " Git: " + GITHASH);

  while ((opt = getopt(argc, argv, "C:P:Q:A:TM:F:s:")) != -1)
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
    case 's': break;
    default: break;
    }
  }

  dabSettings = new QSettings(initFileName, QSettings::IniFormat);

  /*
   *      Before we connect control to the gui, we have to
   *      instantiate
   */
#if QT_VERSION >= 0x050600
  QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

  QApplication a(argc, argv);
  a.setStyleSheet(styleSheet);
  // setting the language
//  QString locale = QLocale::system().name();
//  qDebug() << "main:" << "Detected system language" << locale;
//  setTranslator(&theTranslator, locale);
  QApplication::setWindowIcon(QIcon(":res/DABplusLogoWBsoft.png")); // used for all dialog windows except main window (is overwritten)

  MyRadioInterface = new RadioInterface(dabSettings, presetsFileName, freqExtension, error_report, dataPort, clockPort, fmFrequency, nullptr);
  MyRadioInterface->show();

  qRegisterMetaType<QVector<int>>("QVector<int>");
  a.exec();
  /*
   *      done:
  */
  fprintf(stderr, "back in main program\n");
  fflush(stdout);
  fflush(stderr);
  qDebug("It is done\n");
  delete MyRadioInterface;
  delete dabSettings;
  return 1;
}

//void setTranslator(QTranslator * theTranslator, QString Language)
//{
//  // German is special (as always)
//  if ((Language == "de_AT") || (Language == "de_CH"))
//  {
//    Language = "de_DE";
//  }
//  //
//  // what about Dutch?
//  bool translatorLoaded = theTranslator->load(QString(":/i18n/") + Language);
//  qDebug() << "main:" << "Set language" << Language;
//  QCoreApplication::installTranslator(theTranslator);
//
//  if (!translatorLoaded)
//  {
//    qDebug() << "main:" << "Error while loading language specifics" << Language << "use English \"en_GB\" instead";
//    Language = "en_GB";
//  }
//
//  QLocale curLocale(QLocale((const QString &)Language));
//  QLocale::setDefault(curLocale);
//}
