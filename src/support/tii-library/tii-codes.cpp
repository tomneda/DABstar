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
#include  "dab-constants.h"
#include  "tii-codes.h"
#include  <array>
#include  <QMessageBox>
#include  <cstdio>
#include  <QDir>
#include  <QString>
#include  <cmath>
#include  <cstdio>
#include  <string>
#include  <fstream>

enum
{
  SEPARATOR = ';',
  ID = 0,
  COUNTRY = 1,
  CHANNEL = 2,
  LABEL = 3,
  EID = 4,
  TII = 5,
  LOCATION = 6,
  LATITUDE = 7,
  LONGITUDE = 8,
  ALTITUDE = 9,
  HEIGHT = 10,
  POLARIZATION = 11,
  FREQUENCY = 12,
  POWER = 13,
  DIRECTION = 14,
  NR_COLUMNS = 15
};

TiiHandler::~TiiHandler()
{
  if (mpFn_close_tii != nullptr)
  {
    mpFn_close_tii(mpTiiLibHandler);
  }
}

bool TiiHandler::load_library()
{
  mpFn_init_tii = nullptr;
  mpFn_close_tii = nullptr;
  mpFn_loadTable = nullptr;

#ifndef _WIN32 // we cannot read the TII lib in windows
  std::string libFile = "libtii-lib";
  mphandle = new QLibrary(libFile.c_str());
  mphandle->load();

  if (mphandle->isLoaded())
  {
    if (_load_dyn_library_functions())
    {
      mpTiiLibHandler = mpFn_init_tii();

      if (mpTiiLibHandler != nullptr)
      {
        fprintf(stdout, "Opening '%s' and initialization was successful\n", libFile.c_str());
        delete (mphandle);
        return true;
      }
      else
      {
        fprintf(stderr, "Opening '%s' and initialization failed with error %s\n", libFile.c_str(), dlerror());
      }
    }
    else
    {
      mpTiiLibHandler = nullptr;
      fprintf(stderr, "Failed to open functions in library %s with error '%s'\n", libFile.c_str(), dlerror());
    }
    delete (mphandle);
  }
  else
  {
    fprintf(stderr, "Failed to load library %s with error '%s'\n", libFile.c_str(), dlerror());
  }
#endif
  return false;
}

void TiiHandler::loadTable(const QString & iTiiFileName)
{
  if (!is_valid())
  {
    load_library();
  }

  if (mpFn_loadTable != nullptr)
  {
    mpFn_loadTable(mpTiiLibHandler, iTiiFileName.toStdString());
  }
}

bool TiiHandler::fill_cache_from_tii_file(const QString & iTiiFileName)
{
  bool dataLoaded = false;
  if (iTiiFileName.isEmpty())
  {
    return false;
  }
  mBlackListVec.resize(0);
  mContentCacheVec.resize(0);

  QFile fp(iTiiFileName);

  if (fp.open(QIODevice::ReadOnly))
  {
    fprintf(stdout, "TiiFile is %s\n", iTiiFileName.toUtf8().data());
    dataLoaded = true;
    _read_file(fp);
    fp.close();
  }

  return dataLoaded;
}

const SCacheElem * TiiHandler::get_transmitter_name(const QString & channel,
                                                    uint16_t Eid, uint8_t mainId, uint8_t subId)
{
  //fprintf(stdout, "looking for %s %X %d %d\n", channel.toLatin1().data(), / Eid, mainId, subId);

  for (const auto & i : mContentCacheVec)
  {
    if ((channel == "any" || channel == i.channel) &&
        i.Eid == Eid &&
        i.mainId == mainId &&
        i.subId == subId &&
        !(i.transmitterName.contains("tunnel", Qt::CaseInsensitive)))
    {
      return &i;
    }
  }
  return nullptr;
}

void TiiHandler::get_coordinates(float * latitude, float * longitude, float * power, const QString & channel, const QString & transmitter)
{
  for (const auto & i : mContentCacheVec)
  {
    if (((channel == "any") || (channel == i.channel)) && (i.transmitterName == transmitter))
    {
      *latitude = i.latitude;
      *longitude = i.longitude;
      *power = i.power;
      return;
    }
  }
  *latitude = 0;
  *longitude = 0;
}

//	Great circle distance https://towardsdatascience.com/calculating-the-distance-between-two-locations-using-geocodes-1136d810e517 and
//	https://www.movable-type.co.uk/scripts/latlong.html
//	Haversine formula applied
double TiiHandler::_distance_2(float latitude1, float longitude1, float latitude2, float longitude2) const
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
  double dy = _distance_2(latitude1, longitude2, latitude2, longitude2);
  if (dy_sign)
  {    // lat1 is "higher" than lat2
    dx = _distance_2(latitude1, longitude1, latitude1, longitude2);
  }
  else
  {
    dx = _distance_2(latitude2, longitude1, latitude2, longitude2);
  }
  return (float) sqrt(dx * dx + dy * dy);
}

float TiiHandler::corner(float latitude1, float longitude1, float latitude2, float longitude2) const
{
  bool dx_sign = longitude1 - longitude2 > 0;
  bool dy_sign = latitude1 - latitude2 > 0;
  float dx;
  float dy = distance(latitude1, longitude2, latitude2, longitude2);
  if (dy_sign)
  {    // lat1 is "higher" than lat2
    dx = distance(latitude1, longitude1, latitude1, longitude2);
  }
  else
  {
    dx = distance(latitude2, longitude1, latitude2, longitude2);
  }
  const float azimuth = std::atan2(dy, dx);

  if (dx_sign && dy_sign)
  {    // first quadrant
    return ((F_M_PI_2 - azimuth) / F_M_PI * 180);
  }
  else if (dx_sign) // dy_sign == false
  {  // second quadrant
    return ((F_M_PI_2 + azimuth) / F_M_PI * 180);
  }
  else if (!dy_sign)
  {  // third quadrant
    return ((3 * F_M_PI_2 - azimuth) / F_M_PI * 180);
  }
  return ((3 * F_M_PI_2 + azimuth) / F_M_PI * 180);
}

bool TiiHandler::is_black(uint16_t Eid, uint8_t mainId, uint8_t subId)
{
  for (auto & i : mBlackListVec)
  {
    if ((i.Eid == Eid) && (i.mainId == mainId) && (i.subId == subId))
    {
      return true;
    }
  }
  return false;
}

void TiiHandler::set_black(uint16_t Eid, uint8_t mainId, uint8_t subId)
{
  SBlackListElem element;
  element.Eid = Eid;
  element.mainId = mainId;
  element.subId = subId;
  mBlackListVec.push_back(element);
}

float TiiHandler::_convert(const QString & s) const
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

uint16_t TiiHandler::_get_E_id(const QString & s) const
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

uint8_t TiiHandler::_get_main_id(const QString & s) const
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

uint8_t TiiHandler::_get_sub_id(const QString & s) const
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

void TiiHandler::_read_file(QFile & fp)
{
  uint32_t count = 0;
  std::array<char, 1024> buffer;
  std::vector<QString> columnVector;

  fp.read(reinterpret_cast<char *>(&mShift), 1);

  while (_eread(buffer.data(), buffer.size(), fp) != nullptr)
  {
    SCacheElem ed;
    if (fp.atEnd())
    {
      break;
    }
    columnVector.resize(0);
    int columns = _read_columns(columnVector, buffer.data(), NR_COLUMNS);
    if (columns < NR_COLUMNS)
    {
      continue;
    }
    ed.id = _convert(columnVector[ID]);
    ed.country = columnVector[COUNTRY].trimmed();
    ed.Eid = _get_E_id(columnVector[EID]);
    ed.mainId = _get_main_id(columnVector[TII]);
    ed.subId = _get_sub_id(columnVector[TII]);
    ed.channel = columnVector[CHANNEL].trimmed();
    if (ed.channel.length() < 3) ed.channel = "0" + ed.channel;  // patch as channel input data are now with leading zeros also
    ed.ensemble = columnVector[LABEL].trimmed();
    ed.transmitterName = columnVector[LOCATION];
    ed.latitude = _convert(columnVector[LATITUDE]);
    ed.longitude = _convert(columnVector[LONGITUDE]);
    ed.power = _convert(columnVector[POWER]);
    ed.altitude = _convert(columnVector[ALTITUDE]);
    ed.height = _convert(columnVector[HEIGHT]);
    ed.polarization = columnVector[POLARIZATION].trimmed();
    ed.frequency = _convert(columnVector[FREQUENCY]);
    ed.direction = columnVector[DIRECTION].trimmed();
    if (count >= mContentCacheVec.size())
    {
      mContentCacheVec.resize(mContentCacheVec.size() + 500);
    }
    mContentCacheVec.at(count) = ed;
    count++;
  }
  mContentCacheVec.resize(count);
}

int TiiHandler::_read_columns(std::vector<QString> & oV, const char * b, int N) const
{
  int charp = 0;
  std::array<char, 256> tb;
  int elementCount = 0;
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

char * TiiHandler::_eread(char * buffer, int amount, QFile & fp) const
{
  char * bufferP;
  if (fp.readLine(buffer, amount) < 0)
  {
    return nullptr;
  }
  bufferP = buffer;
  while (*bufferP != 0)
  {
    if (mShift != 0xAA)
    {
      *bufferP -= mShift;
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

bool TiiHandler::is_valid() const
{
  return mpTiiLibHandler != nullptr;
}

bool TiiHandler::_load_dyn_library_functions()
{
  mpFn_init_tii = (TpFn_init_tii) mphandle->resolve("init_tii_L");

  if (mpFn_init_tii == nullptr)
  {
    fprintf(stderr, "init_tii_L not loaded\n");
    return false;
  }

  mpFn_close_tii = (TpFn_close_tii) mphandle->resolve("close_tii_L");

  if (mpFn_close_tii == nullptr)
  {
    fprintf(stderr, "close_tii_L not loaded\n");
    return false;
  }

  mpFn_loadTable = (TpFn_loadTable) mphandle->resolve("loadTableL");

  if (mpFn_loadTable == nullptr)
  {
    fprintf(stderr, "loadTable_L not loaded\n");
    return false;
  }

  return true;
}

