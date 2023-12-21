/*
 *    Copyright (C) 2014 .. 2017
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
#include  <cstdio>
#include  <QDir>
#include  <QString>
#include  <math.h>
#include  "dab-constants.h"
#include  "tii-codes.h"
#include  <QMessageBox>
#include  <stdio.h>
#include  <string>

enum
{
  SEPARATOR = ';',
  COUNTRY = 1,
  CHANNEL = 2,
  LABEL = 3,
  EID = 4,
  TII = 5,
  LOCATION = 6,
  LATITUDE = 7,
  LONGITUDE = 8,
  POWER = 13,
  NR_COLUMNS = 14
};

#ifdef __MINGW32__
#define GETPROCADDRESS  GetProcAddress
#else
#define GETPROCADDRESS  dlsym
#endif

TiiHandler::TiiHandler()
{
}

TiiHandler::~TiiHandler()
{
  if (close_tii_L != nullptr)
  {
    close_tii_L(mTiiLibHandler);
  }
}

bool TiiHandler::load_library()
{
  init_tii_L = nullptr;
  close_tii_L = nullptr;
  loadTable_L = nullptr;

  std::string libFile = "libtii-lib.so";
  mHandle = dlopen(libFile.c_str(), RTLD_NOW | RTLD_GLOBAL);

  if (mHandle == nullptr)
  {
    libFile = std::string("/usr/local/lib/libtii-lib.so");
    mHandle = dlopen(libFile.c_str(), RTLD_NOW | RTLD_GLOBAL);
  }

  if (mHandle == nullptr)
  {
    libFile = std::string("/usr/local/lib/tii-lib.so");
    mHandle = dlopen(libFile.c_str(), RTLD_NOW | RTLD_GLOBAL);
  }

  if (mHandle != nullptr)
  {
    if (load_dyn_library_functions())
    {
      mTiiLibHandler = init_tii_L();

      if (mTiiLibHandler != nullptr)
      {
        fprintf(stdout, "Opening '%s' and initialization was successful\n", libFile.c_str());
        return true;
      }
      else
      {
        fprintf(stderr, "Opening '%s' and initialization failed with error %s\n", libFile.c_str(), dlerror());
      }
    }
    else
    {
      mTiiLibHandler = nullptr;
      fprintf(stderr, "Failed to open functions in library %s with error '%s'\n", libFile.c_str(), dlerror());
    }
  }
  else
  {
    fprintf(stderr, "Failed to load library %s with error '%s'\n", libFile.c_str(), dlerror());
  }
  return false;
}

bool TiiHandler::fill_cache_from_tii_file(const QString & s)
{
  bool res = false;
  if (s == "")
  {
    return false;
  }
  blackList.resize(0);
  cache.resize(0);
  FILE * f = fopen(s.toUtf8().data(), "r+b");
  if (f != nullptr)
  {
    fprintf(stdout, "TiiFile is %s\n", s.toUtf8().data());
    res = true;
    readFile(f);
    fclose(f);
  }
  return res;
}

QString TiiHandler::get_transmitter_name(const QString & channel, uint16_t Eid, uint8_t mainId, uint8_t subId)
{
  //fprintf(stdout, "looking for %s %X %d %d\n", channel.toLatin1().data(), / Eid, mainId, subId);

  for (int i = 0; i < (int)(cache.size()); i++)
  {
    if ((channel == "any" || channel == cache[i].channel) && cache[i].Eid == Eid && cache[i].mainId == mainId && cache[i].subId == subId)
    {
      return cache[i].transmitterName;
    }
  }
  return "";
}

void TiiHandler::get_coordinates(float * latitude, float * longitude, float * power, const QString & channel, const QString & transmitter)
{
  for (int i = 0; i < (int)(cache.size()); i++)
  {
    if (((channel == "any") || (channel == cache[i].channel)) && (cache[i].transmitterName == transmitter))
    {
      *latitude = cache[i].latitude;
      *longitude = cache[i].longitude;
      *power = cache[i].power;
      return;
    }
  }
  *latitude = 0;
  *longitude = 0;
}

//
//	Great circle distance (https://towardsdatascience.com/calculating-the-distance-between-two-locations-using-geocodes-1136d810e517)
//	en
//	https://www.movable-type.co.uk/scripts/latlong.html
//	Haversine formula applied
double TiiHandler::distance_2(float latitude1, float longitude1, float latitude2, float longitude2) const
{
  double R = 6371;
  double Phi1 = latitude1 * M_PI / 180;
  double Phi2 = latitude2 * M_PI / 180;
  //double	dPhi	= (latitude2 - latitude1) * M_PI / 180;
  double dDelta = (longitude2 - longitude1) * M_PI / 180;

  if ((latitude2 == 0) || (longitude2 == 0))
  {
    return -32768;
  }
  //double a = sin(dPhi / 2) * sin(dPhi / 2) + cos(Phi1) * cos(Phi2) * sin(dDelta / 2) * sin(dDelta / 2);
  //double c = 2 * atan2(sqrt(a), sqrt(1 - a));

  double x = dDelta * cos((Phi1 + Phi2) / 2);
  double y = (Phi2 - Phi1);
  double d = sqrt(x * x + y * y);

  //return (int)(R * c + 0.5);
  return (R * d + 0.5);
}

float TiiHandler::distance(float latitude1, float longitude1, float latitude2, float longitude2) const
{
  bool dy_sign = latitude1 > latitude2;
  double dx;
  double dy = distance_2(latitude1, longitude2, latitude2, longitude2);
  if (dy_sign)
  {    // lat1 is "higher" than lat2
    dx = distance_2(latitude1, longitude1, latitude1, longitude2);
  }
  else
  {
    dx = distance_2(latitude2, longitude1, latitude2, longitude2);
  }
  return (float)sqrt(dx * dx + dy * dy);
}

float TiiHandler::corner(float latitude1, float longitude1, float latitude2, float longitude2)
{
  bool dx_sign = longitude1 - longitude2 > 0;
  bool dy_sign = latitude1 - latitude2 > 0;
  double dx;
  double dy = distance(latitude1, longitude2, latitude2, longitude2);
  if (dy_sign)
  {    // lat1 is "higher" than lat2
    dx = distance(latitude1, longitude1, latitude1, longitude2);
  }
  else
  {
    dx = distance(latitude2, longitude1, latitude2, longitude2);
  }
  float azimuth = std::atan2(dy, dx);

  if (dx_sign && dy_sign)
  {    // first quadrant
    return ((M_PI / 2 - azimuth) / M_PI * 180);
  }
  else if (dx_sign) // dy_sign == false
  {  // second quadrant
    return ((M_PI / 2 + azimuth) / M_PI * 180);
  }
  else if (!dy_sign)
  {  // third quadrant
    return ((3 * M_PI / 2 - azimuth) / M_PI * 180);
  }
  return ((3 * M_PI / 2 + azimuth) / M_PI * 180);
}

bool TiiHandler::is_black(uint16_t Eid, uint8_t mainId, uint8_t subId)
{
  for (uint32_t i = 0; i < blackList.size(); i++)
  {
    if ((blackList[i].Eid == Eid) && (blackList[i].mainId == mainId) && (blackList[i].subId == subId))
    {
      return true;
    }
  }
  return false;
}

void TiiHandler::set_black(uint16_t Eid, uint8_t mainId, uint8_t subId)
{
  black element;
  element.Eid = Eid;
  element.mainId = mainId;
  element.subId = subId;
  blackList.push_back(element);
}

float TiiHandler::convert(const QString & s) const
{
  bool flag;
  float v;
  v = s.trimmed().toFloat(&flag);
  if (!flag)
  {
    v = 0;
  }
  return v;
}

uint16_t TiiHandler::get_Eid(const QString & s) const
{
  bool flag;
  uint16_t res;
  res = s.trimmed().toInt(&flag, 16);
  if (!flag)
  {
    res = 0;
  }
  return res;
}

uint8_t TiiHandler::get_mainId(const QString & s) const
{
  bool flag;
  uint16_t res;
  res = s.trimmed().toInt(&flag);
  if (!flag)
  {
    res = 0;
  }
  return res / 100;
}

uint8_t TiiHandler::get_subId(const QString & s) const
{
  bool flag;
  uint16_t res;
  res = s.trimmed().toInt(&flag);
  if (!flag)
  {
    res = 0;
  }
  return res % 100;
}

void TiiHandler::readFile(FILE * f)
{
  uint32_t count = 0;
  std::array<char, 1024> buffer;
  std::vector<QString> columnVector;

  this->shift = fgetc(f);
  while (eread(buffer.data(), 1024, f) != nullptr)
  {
    cacheElement ed;
    if (feof(f))
    {
      break;
    }
    columnVector.resize(0);
    int columns = readColumns(columnVector, buffer.data(), NR_COLUMNS);
    if (columns < NR_COLUMNS)
    {
      continue;
    }
    ed.country = columnVector[COUNTRY].trimmed();
    ed.Eid = get_Eid(columnVector[EID]);
    ed.mainId = get_mainId(columnVector[TII]);
    ed.subId = get_subId(columnVector[TII]);
    ed.channel = columnVector[CHANNEL].trimmed();
    if (ed.channel.length() < 3) ed.channel = "0" + ed.channel;  // patch as channel input data are now with leading zeros also
    ed.ensemble = columnVector[LABEL].trimmed();
    ed.transmitterName = columnVector[LOCATION];
    ed.latitude = convert(columnVector[LATITUDE]);
    ed.longitude = convert(columnVector[LONGITUDE]);
    ed.power = convert(columnVector[POWER]);
    if (count >= cache.size())
    {
      cache.resize(cache.size() + 500);
    }
    cache.at(count) = ed;
    count++;
  }
  cache.resize(count);
}

int TiiHandler::readColumns(std::vector<QString> & oV, const char * b, int N)
{
  int charp = 0;
  std::array<char, 256> tb;
  int elementCount = 0;
  QString element;
  oV.resize(0);
  while ((*b != 0) && (*b != '\n'))
  {
    if (*b == SEPARATOR)
    {
      tb[charp] = 0;
      QString ss = QString::fromUtf8(tb.data());
      oV.push_back(ss);
      charp = 0;
      elementCount++;
      if (elementCount >= N)
      {
        return N;
      }
    }
    else
    {
      tb[charp++] = *b;
    }
    b++;
  }
  return elementCount;
}

char * TiiHandler::eread(char * buffer, int amount, FILE * f)
{
  char * bufferP;
  if (fgets(buffer, amount, f) == nullptr)
  {
    return nullptr;
  }
  bufferP = buffer;
  while (*bufferP != 0)
  {
    if (shift != 0xAA)
    {
      *bufferP -= shift;
    }
    else
    {
      *bufferP ^= 0xAA;
    }
    bufferP++;
  }
  *bufferP = 0;
  return buffer;
}

bool TiiHandler::is_valid()
{
  return mTiiLibHandler != nullptr;
}

bool TiiHandler::load_dyn_library_functions()
{
  init_tii_L = (init_tii_P)GETPROCADDRESS(mHandle, "init_tii_L");

  if (init_tii_L == nullptr)
  {
    fprintf(stderr, "init_tii_L not loaded\n");
    return false;
  }

  close_tii_L = (close_tii_P)GETPROCADDRESS(mHandle, "close_tii_L");

  if (close_tii_L == nullptr)
  {
    fprintf(stderr, "close_tii_L not loaded\n");
    return false;
  }

  loadTable_L = (loadTable_P)GETPROCADDRESS(mHandle, "loadTableL");

  if (loadTable_L == nullptr)
  {
    fprintf(stderr, "loadTable_L not loaded\n");
    return false;
  }

  return true;
}

void TiiHandler::loadTable(const QString & tf)
{
  if (!is_valid())
  {
    load_library();
  }

  if (loadTable_L != nullptr)
  {
    loadTable_L(mTiiLibHandler, tf.toStdString());
  }
}
