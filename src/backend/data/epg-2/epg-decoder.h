/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013 .. 2020
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
 *    along with Qt-TAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef	EPG_DECODER2_H
#define	EPG_DECODER2_H

#include "glob_data_types.h"
#include <QObject>
#include <cstdio>
#include <QString>

class progDesc
{
public:
  QString ident;
  QString shortName;
  QString mediumName;
  QString longName;
  QString shortDescription;
  QString longDescription;
  int startTime;
  int stopTime;

  progDesc()
  {
    startTime = -1;
    stopTime = -1;
  }

  void clean()
  {
    startTime = -1;
    longName = "";
    mediumName = "";
    shortName = "";
    ident = "";
    shortDescription = "";
    longDescription = "";
  }
};

class EpgDecoder : public QObject
{
Q_OBJECT

public:
  EpgDecoder();
  ~EpgDecoder() override = default;

  int process_epg(u8 * v, int e_length, u32 SId, int subType, u32 theDay);

private:
  u32 SId;
  QString stringTable[20];
  int subType;
  u32 julianDate;
  int getBit(u8 * v, int bitnr);
  u32 getBits(u8 * v, int bitnr, int length);
  int process_programGroups(u8 * v, int index);
  int process_programGroup(u8 * v, int index);
  int process_schedule(u8 * v, int index);
  int process_program(u8 * v, int index, progDesc *);
  int process_scope(u8 * v, int index, progDesc *);
  int process_serviceScope(u8 * v, int index);
  int process_mediaDescription(u8 * v, int index, progDesc *);
  int process_ensemble(u8 * v, int index);
  int process_service(u8 * v, int index);
  int process_location(u8 * v, int index, progDesc *);
  int process_bearer(u8 * v, int index);
  int process_geoLocation(u8 * v, int index);
  int process_programmeEvent(u8 * v, int index);
  int process_onDemand(u8 * v, int index);
  int process_genre(u8 * v, int index, progDesc *);
  int process_keyWords(u8 * v, int index);
  int process_link(u8 * v, int index);
  int process_shortName(u8 * v, int index, progDesc *);
  int process_mediumName(u8 * v, int index, progDesc *);
  int process_longName(u8 * v, int index, progDesc *);
  int process_shortDescription(u8 * v, int index, progDesc *);
  int process_longDescription(u8 * v, int index, progDesc *);
  int process_multiMedia(u8 * v, int index);
  int process_radiodns(u8 * v, int index);
  int process_time(u8 * v, int index, int *);
  int process_relativeTime(u8 * v, int index);
  int process_memberOf(u8 * v, int index);
  int process_presentationTime(u8 * v, int index);
  int process_acquisitionTime(u8 * v, int index);

  int process_country(u8 * v, int index);
  int process_point(u8 * v, int index);
  int process_polygon(u8 * v, int index);
  //
  int process_412(u8 * v, int index);
  int process_440(u8 * v, int index);
  int process_46(u8 * v, int index);
  int process_471(u8 * v, int index);
  int process_472(u8 * v, int index);
  int process_473(u8 * v, int index);
  int process_474(u8 * v, int index, int *);
  int process_475(u8 * v, int index);
  int process_476(u8 * v, int index);
  int process_481(u8 * v, int index);
  int process_482(u8 * v, int index);
  int process_483(u8 * v, int index);
  int process_484(u8 * v, int index);
  int process_485(u8 * v, int index);
  int process_4171(u8 * v, int index);

  int process_tokenTable(u8 * v, int index);
  int process_token(u8 * v, int index);
  int process_defaultLanguage(u8 * v, int index);
  int process_obsolete(u8 * v, int index);

  void record(progDesc *);
  QString getCData(u8 * v, int index, int elength);

signals:
  void signal_set_epg_data(int, int, const QString &, const QString &);
};

#endif

