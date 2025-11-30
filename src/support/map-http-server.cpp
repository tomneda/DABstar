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

MapHttpServer::MapHttpServer(DabRadio * parent, const QString & mapPort, const QString & browserAddress, cf32 homeAddress, const QString & saveName, bool autoBrowser_off)
  : parent(parent)
  , mHttpAddress(browserAddress + ":" + mapPort)
  , mHttpPort(mapPort)
  , mAutoBrowserOff(autoBrowser_off)
  , mHomeLocation(homeAddress)
{
  mTcpServer = new QTcpServer(this);

  connect(mTcpServer, &QTcpServer::newConnection, this, &MapHttpServer::_slot_new_connection);
  // connect(this, &MapHttpServer::signal_terminating, parent, &DabRadio::_slot_http_terminate);

  mpCsvFP = fopen(saveName.toUtf8().data(), "w");

  if (mpCsvFP != nullptr)
  {
    fprintf(mpCsvFP, "Home location; %f; %f\n\n", real(homeAddress), imag(homeAddress));
    fprintf(mpCsvFP, "channel; latitude; longitude;transmitter;date and time; mainId; subId; distance; azimuth; power\n\n");
  }
  mAlreadyLoggedTransmitters.clear();
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
    emit signal_terminating();
    return;
  }

  if (!mAutoBrowserOff && !QDesktopServices::openUrl(QUrl(mHttpAddress)))
  {
    qCritical() << "Failed to open URL:" << mHttpAddress;
  }
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
    connect(socket, &QTcpSocket::readyRead, this, &MapHttpServer::_slot_ready_read);
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
  }
}

void MapHttpServer::_slot_ready_read()
{
  QTcpSocket * socket = qobject_cast<QTcpSocket*>(sender());
  if (!socket) return;

  // Read available data (up to 4096 as in original logic)
  const QByteArray buffer = socket->read(4096);
  if (buffer.isEmpty()) return;

  bool keepalive;

  const int httpVer = (buffer.contains("HTTP/1.1")) ? 11 : 10;

  if (httpVer == 11)
  {
    // HTTP 1.1 defaults to keep-alive, unless close is specified.
    keepalive = !buffer.contains("Connection: close");
  }
  else
  {
    // httpver == 10
    keepalive = buffer.contains("Connection: keep-alive");
  }

  /* Identify the URL. */
  int firstSpace = buffer.indexOf(' ');
  if (firstSpace == -1) return;
  int secondSpace = buffer.indexOf(' ', firstSpace + 1);
  if (secondSpace == -1) return;

  QByteArray url = buffer.mid(firstSpace + 1, secondSpace - firstSpace - 1);

  QString content;
  QString ctype;

  //	 "/" -> Our google map application.
  //	 "/data.json" -> Our ajax request to update planes. */
  if (url.contains("/data.json"))
  {
    content = _move_transmitter_list_to_json();
    if (!content.isEmpty())
    {
      ctype = "application/json;charset=utf-8";
    }
  }
  else
  {
    content = _gen_html_code(mHomeLocation);
    ctype = "text/html;charset=utf-8";
  }

  //	Create the header
  std::array<char, 2048> header;

  snprintf(header.data(),
           header.size(),
           "HTTP/1.1 200 OK\r\n"
           "Server: DABstar\r\n"
           "Content-Type: %s\r\n"
           "Connection: %s\r\n"
           "Content-Length: %d\r\n"
           "\r\n", ctype.toStdString().c_str(), keepalive ? "keep-alive" : "close", (i32)content.length());

  socket->write(header.data(), strlen(header.data()));
  socket->write(content.toStdString().c_str(), content.length());

  if (!keepalive)
  {
    socket->disconnectFromHost();
  }
}

QString MapHttpServer::_gen_html_code(const cf32 iHomeLocator) const
{
  const std::string latitude = std::to_string(real(iHomeLocator));
  const std::string longitude = std::to_string(imag(iHomeLocator));
  i32 idx = 0;
  i32 paramPatchCnt = 0;
  QByteArray body;

  // read map file from resource file
  QFile file(":res/map-viewer.html");

  if (file.open(QFile::ReadOnly))
  {
    QByteArray record_data(1, 0);
    QDataStream in(&file);
    const i32 bodySize = file.size();
    body.resize(bodySize + 40 /*place for inserted location data*/);

    while (!in.atEnd())
    {
      in.readRawData(record_data.data(), 1);

      const i32 cc = (*record_data.constData());

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
          body[idx++] = (char)cc;
      }
      else
        body[idx++] = (char)cc;
    }
  }
  else
  {
    qCritical() << "Failed to open map file:" << file.fileName();
    return "";
  }

  body[idx++] = 0;

  const QString res = std::move(body);
  file.close();
  return res;
}

QString MapHttpServer::_move_transmitter_list_to_json()
{
  std::lock_guard<std::mutex> lock(mMutex);

  if (mTransmitters.empty())
  {
    return {};
  }

  QJsonArray jsonArray;

  for (const auto & t : mTransmitters)
  {
    QJsonObject jsonObj;
    jsonObj["type"] = t.type;
    jsonObj["lat"] = real(t.coords);
    jsonObj["lon"] = imag(t.coords);
    jsonObj["name"] = t.transmitterName;
    jsonObj["channel"] = t.channelName;
    jsonObj["mainId"] = t.mainId;
    jsonObj["subId"] = t.subId;
    jsonObj["strength"] = (int)(t.strength * 10);
    jsonObj["dist"] = t.distance;
    jsonObj["azimuth"] = t.azimuth;
    jsonObj["power"] = (int)(t.power * 100);
    jsonObj["altitude"] = t.altitude;
    jsonObj["height"] = t.height;
    jsonObj["dir"] = t.direction;
    jsonObj["pol"] = t.polarization;
    jsonObj["nonetsi"] = (t.non_etsi ? ", non-ETSI phases" : "");

    jsonArray.append(jsonObj);
  }

  mTransmitters.clear();

  const QJsonDocument doc(jsonArray);
  return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

void MapHttpServer::add_location_entry(const u8 type, const STiiDataEntry * const tr, const QString & dateTime,
                                       const f32 strength, const i32 distance, const i32 azimuth, const bool non_etsi)
{
  const cf32 target = cf32(tr->latitude, tr->longitude);

  for (u32 i = 0; i < mTransmitters.size(); i++)
  {
    if (mTransmitters[i].coords == target)
      return;
  }

  SHttpData t;
  t.type = type;
  t.ensemble = tr->ensemble;
  t.Eid = tr->Eid;
  t.coords = target;
  t.transmitterName = tr->transmitterName;
  t.channelName = tr->channel;
  t.dateTime = dateTime;
  t.mainId = tr->mainId;
  t.subId = tr->subId;
  t.strength = strength,
  t.distance = distance;
  t.azimuth = azimuth;
  t.power = tr->power;
  t.altitude = tr->altitude;
  t.height = tr->height;
  t.polarization = tr->polarization;
  t.frequency = tr->frequency;
  t.direction = tr->direction;
  t.non_etsi = non_etsi;
  mMutex.lock();
  mTransmitters.push_back(t);
  mMutex.unlock();

  // log transmitter data to CSV file but ignore already set transmitters
  for (u32 i = 0; i < mAlreadyLoggedTransmitters.size(); i++)
  {
    if ((mAlreadyLoggedTransmitters.at(i).transmitterName == tr->transmitterName) &&
        (mAlreadyLoggedTransmitters.at(i).channelName == tr->channel))
      return;
  }

  if (mpCsvFP != nullptr && type == MAP_NORM_TRANS)
  {
    fprintf(mpCsvFP,
            "%s; %f; %f; %s; %s; %d; %d; %f; %d; %d; %f; %d; %d\n",
            tr->channel.toUtf8().data(),
            real(target),
            imag(target),
            tr->transmitterName.toUtf8().data(),
            t.dateTime.toUtf8().data(),
            tr->mainId,
            tr->subId,
            strength,
            distance,
            azimuth,
            tr->power,
            tr->altitude,
            tr->height);
    mAlreadyLoggedTransmitters.push_back(t);
  }
}
