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
#include "dab-constants.h"

struct CacheElem
{
  int id;
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
  int altitude;
  int height;
  QString polarization;
  float frequency;
  QString direction;
};

struct BlackListElem
{
  uint16_t Eid;
  uint8_t mainId;
  uint8_t subId;
};
typedef struct {
        float   latitude;
        float   longitude;
} position;

// DLL and ".so" function prototypes
using TpFn_init_tii  = void *(*)();
using TpFn_close_tii = void  (*)(void *);
using TpFn_loadTable = void  (*)(void *, const std::string &);

class TiiHandler
{
public:
  TiiHandler() = default;
  ~TiiHandler();

  bool fill_cache_from_tii_file(const QString &);
  const CacheElem *get_transmitter_name(const QString &, uint16_t, uint8_t, uint8_t);
  void get_coordinates(float *, float *, float *, const QString &, const QString &);
  [[nodiscard]] float distance(float, float, float, float) const;
  float corner(float, float, float, float) const;
  bool is_black(uint16_t, uint8_t, uint8_t);
  void set_black(uint16_t, uint8_t, uint8_t);
  bool is_valid() const;
  void loadTable(const QString & iTiiFileName);

private:
  std::vector<BlackListElem> mBlackListVec;
  std::vector<CacheElem> mContentCacheVec;
  uint8_t mShift = 0;
  QString mTiiFileName;
  void * mpTiiLibHandler = nullptr;
  QLibrary * mphandle = nullptr;
  TpFn_init_tii mpFn_init_tii = nullptr;
  TpFn_close_tii mpFn_close_tii = nullptr;
  TpFn_loadTable mpFn_loadTable = nullptr;

  bool load_library();
  float _convert(const QString & s) const;
  uint16_t _get_E_id(const QString & s) const;
  uint8_t _get_main_id(const QString & s) const;
  uint8_t _get_sub_id(const QString & s) const;
  double _distance_2(float, float, float, float) const;
  int _read_columns(std::vector<QString> & oV, const char * b, int N) const;
  void _read_file(FILE *);
  char * _eread(char *, int, FILE *) const;
  bool _load_dyn_library_functions();
};

#endif
