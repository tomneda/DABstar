/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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

bool TiiHandler::_load_library()
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
    _load_library();
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

  mContentCacheMap.clear();

  QFile fp(iTiiFileName);

  if (fp.open(QIODevice::ReadOnly))
  {
    qInfo() << "Reading TII file" << iTiiFileName;
    dataLoaded = true;
    _read_file(fp);
    fp.close();
  }

  return dataLoaded;
}

const SCacheElem * TiiHandler::get_transmitter_name(const QString & channel,
                                                    u16 Eid, u8 mainId, u8 subId)
{
  //fprintf(stdout, "looking for %s %X %d %d\n", channel.toLatin1().data(), / Eid, mainId, subId);
  const u64 key = SCacheElem::make_key_base(Eid, mainId, subId);

  const auto it = mContentCacheMap.find(key);

  if (it != mContentCacheMap.end())
  {
    const SCacheElem & ce = it->second;
    if (channel != "any" && ce.channel != channel)
    {
      qWarning() << "TII database channel mismatch" << ce.channel << channel << "for EId/TII" << Eid << mainId << subId << "- Transmittername" << ce.transmitterName;
    }
    return &ce;
  }

  return nullptr;
}

//	Great circle distance https://towardsdatascience.com/calculating-the-distance-between-two-locations-using-geocodes-1136d810e517 and
//	https://www.movable-type.co.uk/scripts/latlong.html
//	Haversine formula applied
f64 TiiHandler::_distance_2(f32 latitude1, f32 longitude1, f32 latitude2, f32 longitude2) const
{
  const f64 R = 6371;
  const f64 Phi1 = latitude1 * M_PI / 180;
  const f64 Phi2 = latitude2 * M_PI / 180;
  // const f64	dPhi	= (latitude2 - latitude1) * M_PI / 180;
  const f64 dDelta = (longitude2 - longitude1) * M_PI / 180;

  if ((latitude2 == 0) || (longitude2 == 0))
  {
    return -32768;
  }
  // const f64 a = sin(dPhi / 2) * sin(dPhi / 2) + cos(Phi1) * cos(Phi2) * sin(dDelta / 2) * sin(dDelta / 2);
  // const f64 c = 2 * atan2(sqrt(a), sqrt(1 - a));

  const f64 x = dDelta * cos((Phi1 + Phi2) / 2);
  const f64 y = (Phi2 - Phi1);
  const f64 d = sqrt(x * x + y * y);

  return (R * d + 0.5);
}

f32 TiiHandler::distance(f32 latitude1, f32 longitude1, f32 latitude2, f32 longitude2) const
{
  const bool dy_sign = latitude1 > latitude2;
  f64 dx;
  const f64 dy = _distance_2(latitude1, longitude2, latitude2, longitude2);

  if (dy_sign)
  {    // lat1 is "higher" than lat2
    dx = _distance_2(latitude1, longitude1, latitude1, longitude2);
  }
  else
  {
    dx = _distance_2(latitude2, longitude1, latitude2, longitude2);
  }

  return (f32) sqrt(dx * dx + dy * dy);
}

f32 TiiHandler::corner(f32 latitude1, f32 longitude1, f32 latitude2, f32 longitude2) const
{
  const bool dx_sign = longitude1 - longitude2 > 0;
  const bool dy_sign = latitude1 - latitude2 > 0;
  f32 dx;
  const f32 dy = distance(latitude1, longitude2, latitude2, longitude2);

  if (dy_sign)
  {    // lat1 is "higher" than lat2
    dx = distance(latitude1, longitude1, latitude1, longitude2);
  }
  else
  {
    dx = distance(latitude2, longitude1, latitude2, longitude2);
  }

  const f32 azimuth = std::atan2(dy, dx);

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

f32 TiiHandler::_convert(const QString & s) const
{
  bool flag;
  f32 v = s.trimmed().toFloat(&flag);
  if (!flag)
  {
    v = 0;
  }
  return v;
}

u16 TiiHandler::_get_E_id(const QString & s) const
{
  bool flag;
  u16 res = s.trimmed().toInt(&flag, 16);
  if (!flag)
  {
    res = 0;
  }
  return res;
}

u8 TiiHandler::_get_main_id(const QString & s) const
{
  bool flag;
  u16 res = s.trimmed().toInt(&flag);
  if (!flag)
  {
    res = 0;
  }
  return res / 100;
}

u8 TiiHandler::_get_sub_id(const QString & s) const
{
  bool flag;
  u16 res = s.trimmed().toInt(&flag);
  if (!flag)
  {
    res = 0;
  }
  return res % 100;
}

void TiiHandler::_read_file(QFile & fp)
{
  u32 count = 0;
  u32 countDuplicates = 0;
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
    i32 columns = _read_columns(columnVector, buffer.data(), NR_COLUMNS);
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
    ed.patch_channel_name();
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

    const u32 key = ed.make_key_base();

    if (mContentCacheMap.find(key) == mContentCacheMap.end())
    {
      // qInfo() << QString::number(key, 16) << " : channel" << ed.channel << "Eid" << QString::number(ed.Eid, 16) << "TII" << ed.mainId << ed.subId;

      if (!ed.transmitterName.contains("tunnel", Qt::CaseInsensitive))
      {
        mContentCacheMap.insert(std::make_pair(key, ed));
      }
      else
      {
        // qWarning() << "tunnel found" << ed.transmitterName;
      }
    }
    else
    {
      countDuplicates++;
      // qWarning() << "duplicate found" << ed.transmitterName;
    }

    count++;
  }

  qInfo() << "Read" << mContentCacheMap.size() << "valid entries from" << count << "entries from database with" << countDuplicates << "duplicates";
}

i32 TiiHandler::_read_columns(std::vector<QString> & oV, const char * b, i32 N) const
{
  i32 charp = 0;
  std::array<char, 256> tb;
  i32 elementCount = 0;
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

char * TiiHandler::_eread(char * buffer, i32 amount, QFile & fp) const
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

