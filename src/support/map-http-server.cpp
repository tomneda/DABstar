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

#include "map-http-server.h"
#include "dabradio.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>
#include <QDataStream>
#include <QTimer>

MapHttpServer::MapHttpServer(DabRadio * ipDabRadio, const QString & iHttpPort, const QString & iHttpAddress, cf32 iHomeLocation, const QString & iCsvDumpName, bool iAutoBrowserOff)
  : mpDabRadio(ipDabRadio)
  , mHttpAddress(iHttpAddress + ":" + iHttpPort)
  , mHttpPort(iHttpPort)
  , mAutoBrowserOff(iAutoBrowserOff)
  , mHomeLocation(iHomeLocation)
{
  mTcpServer = new QTcpServer(this);

  mpAjaxRequestTimer = new QTimer(this);
  mpAjaxRequestTimer->setSingleShot(true);

  connect(mTcpServer, &QTcpServer::newConnection, this, &MapHttpServer::_slot_new_connection);
  connect(this, &MapHttpServer::signal_terminating, mpDabRadio, &DabRadio::slot_http_terminate);
  connect(mpAjaxRequestTimer, &QTimer::timeout, this, &MapHttpServer::_slot_ajax_request_timeout);

  mpCsvFP = fopen(iCsvDumpName.toUtf8().data(), "w");

  if (mpCsvFP != nullptr)
  {
    // fprintf(mpCsvFP, "Home location; %f; %f\n\n", real(iHomeLocation), imag(iHomeLocation));
    fprintf(mpCsvFP, "Channel; Latitude; Longitude; Transmitter; Altitude; Height; Date and time; MainId; SubId; Strength; Distance; Azimuth; Power\n");
  }

  start();
}

MapHttpServer::~MapHttpServer()
{
  stop();

  if (mpCsvFP != nullptr)
  {
    fclose(mpCsvFP);
  }
}

void MapHttpServer::start()
{
  if (!mTcpServer->listen(QHostAddress::Any, mHttpPort.toInt()))
  {
    qCritical() << "HttpHandler: Failed to listen on port" << mHttpPort;
    return;
  }

  if (!mAutoBrowserOff && !QDesktopServices::openUrl(QUrl(mHttpAddress)))
  {
    qCritical() << "Failed to open URL:" << mHttpAddress;
    return;
  }

  mpAjaxRequestTimer->start(12000); // give a bit more time after browser start
}

void MapHttpServer::stop()
{
  if (mTcpServer->isListening())
  {
    mTcpServer->close();
  }
}

void MapHttpServer::_slot_new_connection()
{
  while (mTcpServer->hasPendingConnections())
  {
    const QTcpSocket * socket = mTcpServer->nextPendingConnection();
    qDebug() << "New connection from" << socket->peerAddress();
    connect(socket, &QTcpSocket::readyRead, this, &MapHttpServer::_slot_ready_read);
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
  }
}

void MapHttpServer::_slot_ready_read()
{
  QTcpSocket * const socket = qobject_cast<QTcpSocket*>(sender());
  if (socket == nullptr) return;

  // Read available data (up to 4096 as in original logic)
  const QByteArray buffer = socket->read(4096);
  if (buffer.isEmpty()) return;

  const int httpVer = (buffer.contains("HTTP/1.1")) ? 11 : 10;
  const bool keepAlive = (httpVer == 11) ? !buffer.contains("Connection: close") : buffer.contains("Connection: keep-alive");

  /* Identify the URL. */
  const int firstSpace = buffer.indexOf(' ');
  if (firstSpace == -1) return;
  const int secondSpace = buffer.indexOf(' ', firstSpace + 1);
  if (secondSpace == -1) return;
  const QByteArray url = buffer.mid(firstSpace + 1, secondSpace - firstSpace - 1);

  QByteArray content;
  QString ctype;

  mpAjaxRequestTimer->start(2000 + 1000); // must be longer than the repetition request time from the web client (2000ms there)

  if (url.contains("/data.json")) // ajax request
  {
    content = _move_transmitter_list_to_json();

    if (!content.isEmpty())
    {
      ctype = "application/json;charset=utf-8";
    }
  }
  else
  {
    content = _gen_html_code();
    ctype = "text/html;charset=utf-8";
  }

  const QByteArray header = QString("HTTP/1.1 200 OK\r\n"
                                    "Server: DABstar\r\n"
                                    "Content-Type: %1\r\n"
                                    "Connection: %2\r\n"
                                    "Content-Length: %3\r\n"
                                    "\r\n"
                                   ).arg(ctype)
                                    .arg(keepAlive ? "keep-alive" : "close")
                                    .arg(content.size()).toLatin1();

  socket->write(header);
  socket->write(content);

  if (!keepAlive)
  {
    socket->disconnectFromHost();
    qDebug() << "Closing socket";
  }
}

void MapHttpServer::_slot_ajax_request_timeout()
{
  qDebug() << "Ajax request timeout";
  emit signal_terminating();
}

QByteArray MapHttpServer::_gen_html_code() const
{
  const std::string latitude  = std::to_string(real(mHomeLocation));
  const std::string longitude = std::to_string(imag(mHomeLocation));

  QFile file(":res/map-viewer.html");

  if (!file.open(QFile::ReadOnly))
  {
    qCritical() << "Failed to open map file:" << file.fileName();
    return "";
  }

  QDataStream in(&file);
  QByteArray body(file.size() + 40 /*place for inserted location data*/, Qt::Uninitialized);

  QByteArray record_data(1, Qt::Uninitialized);
  i32 idx = 0;
  i32 paramPatchCnt = 0;

  while (!in.atEnd())
  {
    char cc;
    in.readRawData(&cc, 1);

    if (cc == '$')
    {
      if (paramPatchCnt == 0)
      {
        for (i32 i = 0; latitude.c_str()[i] != 0; i++)
        {
          if (latitude.c_str()[i] == ',')
            body[idx++] = '.';
          else
            body[idx++] = latitude.c_str()[i];
        }
        paramPatchCnt++;
      }
      else if (paramPatchCnt == 1)
      {
        for (i32 i = 0; longitude.c_str()[i] != 0; i++)
        {
          if (longitude.c_str()[i] == ',')
            body[idx++] = '.';
          else
            body[idx++] = longitude.c_str()[i];
        }
        paramPatchCnt++;
      }
      else
      {
        body[idx++] = cc;
      }
    }
    else
    {
      body[idx++] = cc;
    }
  }

  file.close();

  body[idx++] = 0;
  assert(idx <= body.size());
  body.resize(idx); // TODO: use insert

  return body;
}

QByteArray MapHttpServer::_move_transmitter_list_to_json()
{
  std::lock_guard lock(mMutex);

  if (mTransmitters.empty())
  {
    return {};
  }

  QJsonArray jsonArray;

  for (const auto & t : mTransmitters)
  {
    QJsonObject jsonObj;
    jsonObj["type"] = t.type;
    jsonObj["lat"] = t.latitude;
    jsonObj["lon"] = t.longitude;
    jsonObj["name"] = t.transmitterName;
    jsonObj["channel"] = t.channelName;
    jsonObj["mainId"] = t.mainId;
    jsonObj["subId"] = t.subId;
    jsonObj["strength"] = t.strength;
    jsonObj["dist"] = t.distance;
    jsonObj["azimuth"] = t.azimuth;
    jsonObj["power"] = t.power;
    jsonObj["altitude"] = t.altitude;
    jsonObj["height"] = t.height;
    jsonObj["dir"] = t.direction;
    jsonObj["pol"] = t.polarization;
    jsonObj["nonetsi"] = (t.non_etsi ? ", non-ETSI phases" : "");

    jsonArray.append(jsonObj);
  }

  mTransmitters.clear();

  const QJsonDocument doc(jsonArray);
  return doc.toJson(QJsonDocument::Compact);
}

void MapHttpServer::add_transmitter_location_entry(const u8 iType, const STiiDataEntry * const ipTiiDataEntry, const QString & iDateTime,
                                                   const f32 iStrength, f32 iDistance, f32 iAzimuth, const bool iNonEtsi)
{
  // we only want to set a MAP_RESET or MAP_CLOSE?
  if (iType != MAP_NORM_TRANS)
  {
    SHttpData t{};
    t.type = iType;
    mMutex.lock();
    mTransmitters.clear();
    mTransmitters.emplace_back(t);
    mMutex.unlock();
    return;
  }

  // check if entry was already added to transmitter list
  for (const auto & t : mTransmitters)
  {
    if (t.latitude  == ipTiiDataEntry->latitude &&
        t.longitude == ipTiiDataEntry->longitude &&
        t.transmitterName == ipTiiDataEntry->transmitterName)
    {
      return;
    }
  }

  SHttpData t;
  t.type = iType;
  t.ensemble = ipTiiDataEntry->ensemble;
  t.Eid = ipTiiDataEntry->Eid;
  t.latitude = ipTiiDataEntry->latitude;
  t.longitude = ipTiiDataEntry->longitude;
  t.transmitterName = ipTiiDataEntry->transmitterName;
  t.channelName = ipTiiDataEntry->channel;
  t.dateTime = iDateTime;
  t.mainId = ipTiiDataEntry->mainId;
  t.subId = ipTiiDataEntry->subId;
  t.strength = iStrength,
  t.distance = iDistance;
  t.azimuth = iAzimuth;
  t.power = ipTiiDataEntry->power;
  t.altitude = ipTiiDataEntry->altitude;
  t.height = ipTiiDataEntry->height;
  t.polarization = ipTiiDataEntry->polarization;
  t.frequency = ipTiiDataEntry->frequency;
  t.direction = (ipTiiDataEntry->direction.isEmpty() ? "undefined" : ipTiiDataEntry->direction);
  t.non_etsi = iNonEtsi;

  mMutex.lock();
  mTransmitters.emplace_back(t);
  mMutex.unlock();

  // Log transmitter data to CSV file but ignore already set transmitters
  for (auto & t : mAlreadyLoggedTransmitters)
  {
    if ((t.transmitterName == ipTiiDataEntry->transmitterName) &&
        (t.channelName == ipTiiDataEntry->channel))
    {
      return;
    }
  }

  if (mpCsvFP != nullptr)
  {
    fprintf(mpCsvFP,
            "%s; %f; %f; %s; %d; %d; %s; %d; %d; %.1f; %.1f; %.1f; %.3f\n",
            ipTiiDataEntry->channel.toUtf8().data(),
            ipTiiDataEntry->latitude,
            ipTiiDataEntry->longitude,
            ipTiiDataEntry->transmitterName.toUtf8().data(),
            ipTiiDataEntry->altitude,
            ipTiiDataEntry->height,
            t.dateTime.toUtf8().data(),
            ipTiiDataEntry->mainId,
            ipTiiDataEntry->subId,
            iStrength,
            iDistance,
            iAzimuth,
            ipTiiDataEntry->power);

    mAlreadyLoggedTransmitters.push_back(t);
  }
}
