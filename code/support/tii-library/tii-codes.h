/*
 *    Copyright (C) 2014 .. 2022
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation recorder 2 of the License.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef  TII_HANDLER_H
#define  TII_HANDLER_H

#include <stdint.h>
#include <QString>
#include <QSettings>
#include "dlfcn.h"
#include "dab-constants.h"

struct cacheElement
{
  QString country;
  QString channel;
  QString ensemble;
  uint16_t Eid;
  uint8_t mainId;
  uint8_t subId;
  QString transmitterName;
  float latitude;
  float longitude;
  float power;
};

struct black
{
  uint16_t Eid;
  uint8_t mainId;
  uint8_t subId;
};

// DLL and ".so" function prototypes
using init_tii_P  = void *(*)();
using close_tii_P = void  (*)(void *);
using loadTable_P = void  (*)(void *, const std::string &);

class TiiHandler
{
public:
  TiiHandler();
  ~TiiHandler();
  bool tiiFile(const QString &);
  QString get_transmitterName(const QString &, uint16_t, uint8_t, uint8_t);
  void get_coordinates(float *, float *, float *, const QString &, const QString &);
  int distance_2(float, float, float, float) const;
  double distance(float, float, float, float) const;
  int corner(float, float, float, float);
  bool is_black(uint16_t, uint8_t, uint8_t);
  void set_black(uint16_t, uint8_t, uint8_t);
  void loadTable(const QString & tf);
  bool valid();
private:
  std::vector<black> blackList;
  std::vector<cacheElement> cache;
  QString tiifileName;

  void * handler;

  float convert(const QString & s) const;
  uint16_t get_Eid(const QString & s) const;
  uint8_t get_mainId(const QString & s) const;
  uint8_t get_subId(const QString & s) const;
  int readColumns(std::vector<QString> &, char *, int);
  void readFile(FILE *);
  char * eread(char *, int, FILE *);
  uint8_t shift;

  HINSTANCE Handle;
  bool loadFunctions();
  init_tii_P init_tii_L;
  close_tii_P close_tii_L;
  loadTable_P loadTable_L;
};

#endif


