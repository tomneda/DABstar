/*
 * Copyright (c) 2025 by Thomas Neder (https://github.com/tomneda)
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#pragma once

#include "dab-constants.h"
#include "tii-codes.h"
#include <QObject>
#include <vector>
#include <mutex>
#include <QString>

class DabRadio;
class QTcpServer;

class MapHttpServer : public QObject
{
  Q_OBJECT

public:
  MapHttpServer(DabRadio *, const QString & iHttpPort, const QString & iHttpAddress, cf32 address, const QString & iCsvDumpName, bool autoBrowse);
  ~MapHttpServer() override;

  void start();
  void stop();

  void add_transmitter_location_entry(u8 iType, const STiiDataEntry * ipTiiDataEntry, const QString & iDateTime, f32 iStrength, i32 iDistance, i32 iAzimuth, bool iNonEtsi);

private:
  struct SHttpData
  {
    u8 type;
    f32 latitude;
    f32 longitude;
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
    i32 distance;
    i32 azimuth;
    f32 power;
    f32 frequency;
    i32 altitude;
    i32 height;
    bool non_etsi;
  };

  const DabRadio * mpDabRadio;
  const QString mHttpAddress;
  const QString mHttpPort;
  const bool mAutoBrowserOff;
  const cf32 mHomeLocation;

  FILE * mpCsvFP = nullptr;
  QTcpServer * mTcpServer = nullptr;
  std::vector<SHttpData> mAlreadyLoggedTransmitters;
  std::vector<SHttpData> mTransmitters;
  mutable std::mutex mMutex; // guards the list of mTransmitters

  QByteArray _gen_html_code() const;
  QByteArray _move_transmitter_list_to_json(); // mTransmitters is empty after call

private slots:
  void _slot_new_connection();
  void _slot_ready_read();

signals:
  void signal_terminating();
};
