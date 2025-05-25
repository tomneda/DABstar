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

#include <cstdint>
#include <QString>
#include <QSettings>
#include <QLibrary>
#include <QFile>
#include "dab-constants.h"

struct SCacheElem
{
  int id;
  QString country;
  QString channel;
  QString ensemble;
  u16 Eid;
  u8 mainId;
  u8 subId;
  QString transmitterName;
  f32 latitude;
  f32 longitude;
  f32 power;
  int altitude;
  int height;
  QString polarization;
  f32 frequency;
  QString direction;
};

struct SBlackListElem
{
  u16 Eid;
  u8 mainId;
  u8 subId;
};

// DLL and ".so" function prototypes
using TpFn_init_tii = void * (*)();
using TpFn_close_tii = void (*)(void *);
using TpFn_loadTable = void (*)(void *, const std::string &);

class TiiHandler
{
public:
  TiiHandler() = default;
  ~TiiHandler();

  bool fill_cache_from_tii_file(const QString &);
  const SCacheElem * get_transmitter_name(const QString &, u16, u8, u8);
  void get_coordinates(f32 *, f32 *, f32 *, const QString &, const QString &);
  [[nodiscard]] f32 distance(f32, f32, f32, f32) const;
  f32 corner(f32, f32, f32, f32) const;
  bool is_black(u16, u8, u8);
  void set_black(u16, u8, u8);
  bool is_valid() const;
  void loadTable(const QString & iTiiFileName);

private:
  std::vector<SBlackListElem> mBlackListVec;
  std::vector<SCacheElem> mContentCacheVec;
  u8 mShift = 0;
  QString mTiiFileName;
  void * mpTiiLibHandler = nullptr;
  QLibrary * mphandle = nullptr;
  TpFn_init_tii mpFn_init_tii = nullptr;
  TpFn_close_tii mpFn_close_tii = nullptr;
  TpFn_loadTable mpFn_loadTable = nullptr;

  bool load_library();
  f32 _convert(const QString & s) const;
  u16 _get_E_id(const QString & s) const;
  u8 _get_main_id(const QString & s) const;
  u8 _get_sub_id(const QString & s) const;
  f64 _distance_2(f32, f32, f32, f32) const;
  int _read_columns(std::vector<QString> & oV, const char * b, int N) const;
  void _read_file(QFile & fp);
  char * _eread(char * buffer, int amount, QFile & fp) const;
  bool _load_dyn_library_functions();
};

#endif
