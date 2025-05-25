/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *	Copyright (C) 2022
 *	Jan van Katwijk (J.vanKatwijk@gmail.com)
 *	Lazy Chair Computing
 *
 *	This file is part of Qt-DAB
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
 */

#ifndef  HTTP_HANDLER_H
#define  HTTP_HANDLER_H

#include "dab-constants.h"
#include "tii-codes.h"
#include  <QObject>
#include  <thread>
#include  <atomic>
#include  <string>
#include  <vector>
#include  <mutex>
#include  <QString>

class DabRadio;

typedef struct
{
  u8 type;
  cf32 coords;
  QString transmitterName;
  QString channelName;
  QString dateTime;
  QString ensemble;
  QString polarization;
  QString direction;
  u16 Eid;
  u8 mainId;
  u8 subId;
  f32 strength;
  int distance;
  int azimuth;
  f32 power;
  f32 frequency;
  int altitude;
  int height;
  bool non_etsi;
} httpData;

class HttpHandler : public QObject
{
  Q_OBJECT

public:
  HttpHandler(DabRadio *, const QString & mapPort, const QString & browserAddress, cf32 address, const QString & saveName, bool autoBrowse);
  ~HttpHandler();
  void start();
  void stop();
  void run();
  void putData(u8 type, const SCacheElem * tr, const QString & dateTime,
               f32 strength, int distance, int azimuth, bool non_etsi);

private:
  FILE * saveFile;
  QString * saveName;
  DabRadio * parent;
  QString mapPort;
  cf32 homeAddress;
  std::vector<httpData> transmitterVector;

#if defined(__MINGW32__) || defined(_WIN32)
  std::wstring	browserAddress;
#else
  std::string browserAddress;
#endif
  std::atomic<bool> running;
  std::thread threadHandle;
  std::string theMap(cf32 address);
  std::string coordinatesToJson(const std::vector<httpData> & t);
  std::vector<httpData> transmitterList;
  std::mutex locker;
  bool autoBrowser_off;
signals:
  void terminating();
};

#endif
