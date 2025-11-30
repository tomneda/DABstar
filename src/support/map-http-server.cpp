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
#include <string>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDesktopServices>
#include <QUrl>
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
  connect(this, &MapHttpServer::signal_terminating, parent, &DabRadio::_slot_http_terminate);

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
  QByteArray buffer = socket->read(4096);
  if (buffer.isEmpty()) return;

  bool keepalive;
  int httpver = (buffer.contains("HTTP/1.1")) ? 11 : 10;
  if (httpver == 11)
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

  std::string content;
  std::string ctype;

  //	 "/" -> Our google map application.
  //	 "/data.json" -> Our ajax request to update planes. */
  if (url.contains("/data.json"))
  {
    content = _conv_transmitter_list_to_json();
    if (!content.empty())
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
  char hdr[2048];
  sprintf(hdr, "HTTP/1.1 200 OK\r\n"
          "Server: DABstar\r\n"
          "Content-Type: %s\r\n"
          "Connection: %s\r\n"
          "Content-Length: %d\r\n"
          "\r\n", ctype.c_str(), keepalive ? "keep-alive" : "close", (i32)content.length());

  socket->write(hdr, strlen(hdr));
  socket->write(content.c_str(), content.length());

  if (!keepalive)
  {
    socket->disconnectFromHost();
  }
}



#if !defined(__MINGW32__) && !defined(_WIN32)
void MapHttpServer::run()
{
  char buffer[4096];
  bool keepalive;
  char * url;
  i32 one = 1, ClientSocket = 0, ListenSocket;
  struct sockaddr_in svr_addr, cli_addr;
  std::string content;
  std::string ctype;

  running.store(true);
  socklen_t sin_len = sizeof(cli_addr);
  ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (ListenSocket < 0)
  {
    running.store(false);
    signal_terminating();
    return;
  }

  i32 flags = fcntl(ListenSocket, F_GETFL);
  fcntl(ListenSocket, F_SETFL, flags | O_NONBLOCK);
  setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(i32));
  svr_addr.sin_family = AF_INET;
  svr_addr.sin_addr.s_addr = INADDR_ANY;
  svr_addr.sin_port = htons(mHttpPort.toInt());

  if (::bind(ListenSocket, (struct sockaddr*)&svr_addr, sizeof(svr_addr)) == -1)
  {
    close(ListenSocket);
    running.store(false);
    signal_terminating();
    return;
  }

  //	Now, we are listening to port XXXX, ready to accept a
  //	socket for anyone who needs us
  ::listen(ListenSocket, 5);
  while (running.load())
  {
    ClientSocket = accept(ListenSocket, (struct sockaddr*)&cli_addr, &sin_len);
    if (ClientSocket == -1)
    {
      usleep(2000000);
      continue;
    }
    //
    //	someone needs us, let us see what (s)he wants
    while (running.load())
    {
      if (read(ClientSocket, buffer, 4096) < 0)
      {
        running.store(false);
        break;
      }

      // fprintf (stderr, "Buffer - %s\n", buffer);
      i32 httpver = (strstr(buffer, "HTTP/1.1") != nullptr) ? 11 : 10;
      if (httpver == 11)
      {
        //	HTTP 1.1 defaults to keep-alive, unless close is specified.
        keepalive = strstr(buffer, "Connection: close") == nullptr;
      }
      else
      { // httpver == 10
        keepalive = strstr(buffer, "Connection: keep-alive") != nullptr;
      }

      /*	Identify the URL. */
      char * p = strchr(buffer, ' ');
      if (p == nullptr)
      {
        break;
      }
      url = ++p; // Now this should point to the requested URL.
      p = strchr(p, ' ');
      if (p == nullptr)
      {
        break;
      }
      *p = '\0';

      //	Select the content to send, we have just two so far:
      //	 "/" -> Our google map application.
      //	 "/data.json" -> Our ajax request to update planes. */
      bool jsonUpdate = false;
      if (strstr(url, "/data.json"))
      {
        content = coordinatesToJson(mTransmitters);
        mTransmitters.clear();

        if (content != "")
        {
          ctype = "application/json;charset=utf-8";
          jsonUpdate = true;
        }
      }
      else
      {
        content = _gen_html_code(mHomeLocation);
        ctype = "text/html;charset=utf-8";
      }

      //	Create the header
      char hdr[2048];
      sprintf(hdr, "HTTP/1.1 200 OK\r\n"
              "Server: qt-dab\r\n"
              "Content-Type: %s\r\n"
              "Connection: %s\r\n"
              "Content-Length: %d\r\n"
              // "Access-Control-Allow-Origin: *\r\n"
              "\r\n", ctype.c_str(), keepalive ? "keep-alive" : "close", (i32)(strlen(content.c_str())));
      i32 hdrlen = strlen(hdr);
      //	      fprintf (stderr, "reply header %s \n", hdr);
      if (jsonUpdate)
      {
        // fprintf (stderr, "Json update requested\n");
        // fprintf (stderr, "%s\n", content. c_str ());
      }
      //	and send the reply
      if (write(ClientSocket, hdr, hdrlen) != hdrlen || write(ClientSocket, content.c_str(), content.size()) != (signed)content.size())
      {
        // fprintf (stderr, "WRITE PROBLEM\n");
        // break;
      }
    }
  }
  fprintf(stderr, "mapServer quits\n");
  close(ListenSocket);
  if (ClientSocket != 0) // hoping that ClientSocket of zero cannot be valid (to avoid "‘ClientSocket’ may be used uninitialized")
  {
    close(ClientSocket);
  }
  emit signal_terminating();
}

#else

//
//	windows version
//
#if 0
void MapHttpServer::run()
{
  char buffer[4096];
  bool keepalive;
  char * url;
  std::string content;
  std::string ctype;
  WSADATA wsa;
  i32 iResult;
  SOCKET ListenSocket = INVALID_SOCKET;
  SOCKET ClientSocket = INVALID_SOCKET;
  struct addrinfo * result = nullptr;
  struct addrinfo hints;

  if (WSAStartup(MAKEWORD (2, 2), &wsa) != 0)
  {
    signal_terminating();
    return;
  }

  ZeroMemory (&hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  //	Resolve the server address and port
  iResult = getaddrinfo(nullptr, mHttpPort.toLatin1().data(), &hints, &result);
  if (iResult != 0)
  {
    WSACleanup();
    signal_terminating();
    return;
  }

  // Create a SOCKET for connecting to server
  ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (ListenSocket == INVALID_SOCKET)
  {
    freeaddrinfo(result);
    WSACleanup();
    signal_terminating();
    return;
  }
  unsigned long mode = 1;
  ioctlsocket(ListenSocket, FIONBIO, &mode);

  // Setup the TCP listening socket
  iResult = ::bind(ListenSocket, result->ai_addr, (i32)result->ai_addrlen);
  if (iResult == SOCKET_ERROR)
  {
    freeaddrinfo(result);
    closesocket(ListenSocket);
    WSACleanup();
    signal_terminating();
    return;
  }

  freeaddrinfo(result);

  running.store(true);

  listen(ListenSocket, 5);
  while (running.load())
  {
    ClientSocket = accept(ListenSocket, nullptr, nullptr);
    if (ClientSocket == INVALID_SOCKET)
    {
      usleep(2000000);
      continue;
    }

    while (running.load())
    {
      i32 xx;
L1:
      if ((xx = recv(ClientSocket, buffer, 4096, 0)) < 0)
      {
        // shutdown the connection since we're done
        iResult = shutdown(ClientSocket, SD_SEND);
        if (iResult == SOCKET_ERROR)
        {
          closesocket(ClientSocket);
          closesocket(ListenSocket);
          WSACleanup();
          running.store(false);
          signal_terminating();
          return;
        }
        break;
      }
      if (xx == 0)
      {
        if (!running.load())
        {
          closesocket(ClientSocket);
          closesocket(ListenSocket);
          WSACleanup();
          signal_terminating();
          return;
        }
        Sleep(1);
        goto L1;
      }

      i32 httpver = (strstr(buffer, "HTTP/1.1") != nullptr) ? 11 : 10;
      if (httpver == 11)
      {
        //	HTTP 1.1 defaults to keep-alive, unless close is specified.
        keepalive = strstr(buffer, "Connection: close") == nullptr;
      }
      else
      { // httpver == 10
        keepalive = strstr(buffer, "Connection: keep-alive") != nullptr;
      }

      /* Identify the URL. */
      char * p = strchr(buffer, ' ');
      if (p == nullptr)
      {
        break;
      }
      url = ++p; // Now this should point to the requested URL.
      p = strchr(p, ' ');
      if (p == nullptr)
      {
        break;
      }
      *p = '\0';

      //	Select the content to send, we have just two so far:
      //	 "/" -> Our google map application.
      //	 "/data.json" -> Our ajax request to update transmitters. */
      //bool jsonUpdate = false;
      if (strstr(url, "/data.json"))
      {
        content = coordinatesToJson(mTransmitters);
        mTransmitters.clear();
        if (content != "")
        {
          ctype = "application/json;charset=utf-8";
          //jsonUpdate = true;
          // fprintf (stderr, "%s will be sent\n", content. c_str ());
        }
      }
      else
      {
        content = _gen_html_code(mHomeLocation);
        ctype = "text/html;charset=utf-8";
      }
      //	Create the header
      char hdr[2048];
      sprintf(hdr, "HTTP/1.1 200 OK\r\n"
                   "Server: SDRunoPlugin_1090\r\n"
                   "Content-Type: %s\r\n"
                   "Connection: %s\r\n"
                   "Content-Length: %d\r\n"
                   // "Access-Control-Allow-Origin: *\r\n"
                   "\r\n", ctype.c_str(), keepalive ? "keep-alive" : "close", (i32)(strlen(content.c_str())));
      i32 hdrlen = strlen(hdr);
      //	      if (jsonUpdate) {
      //	         parent -> show_text (std::string ("Json update requested\n"));
      //	         parent -> show_text (content);
      //	      }
      //	and send the reply
      if ((send(ClientSocket, hdr, hdrlen, 0) == SOCKET_ERROR) || (send(ClientSocket, content.c_str(), content.size(), 0) == SOCKET_ERROR))
      {
        fprintf(stderr, "WRITE PROBLEM\n");
        break;
      }
    }
  }
  // cleanup
  closesocket(ClientSocket);
  closesocket(ListenSocket);
  WSACleanup();
  running.store(false);
  emit  signal_terminating();
}
#endif

#endif

std::string MapHttpServer::_gen_html_code(const cf32 iHomeLocator) const
{
  std::string res;
  i32 bodySize;
  char * body;
  const std::string latitude = std::to_string(real(iHomeLocator));
  const std::string longitude = std::to_string(imag(iHomeLocator));
  i32 teller = 0;
  i32 params = 0;

  // read map file from resource file
  QFile file(":res/qt-map.html");
  if (file.open(QFile::ReadOnly))
  {
    QByteArray record_data(1, 0);
    QDataStream in(&file);
    bodySize = file.size();
    body = (char*)malloc(bodySize + 40);
    while (!in.atEnd())
    {
      in.readRawData(record_data.data(), 1);
      const i32 cc = (*record_data.constData());
      if (cc == '$')
      {
        if (params == 0)
        {
          for (i32 i = 0; latitude.c_str()[i] != 0; i++)
            if (latitude.c_str()[i] == ',')
              body[teller++] = '.';
            else
              body[teller++] = latitude.c_str()[i];
          params++;
        }
        else if (params == 1)
        {
          for (i32 i = 0; longitude.c_str()[i] != 0; i++)
            if (longitude.c_str()[i] == ',')
              body[teller++] = '.';
            else
              body[teller++] = longitude.c_str()[i];
          params++;
        }
        else
          body[teller++] = (char)cc;
      }
      else
        body[teller++] = (char)cc;
    }
  }
  else
  {
    fprintf(stderr, "cannot open file\n");
    return "";
  }

  body[teller++] = 0;
  res = std::string(body);
  // fprintf(stderr, "The map :\n%s\n", res. c_str ());
  file.close();
  free(body);
  return res;
}

std::string dotNumber(f32 f)
{
  char temp[256];
  std::string s = std::to_string(f);
  for (u32 i = 0; i < s.size(); i++)
  {
    if (s.c_str()[i] == ',')
    {
      temp[i] = '.';
    }
    else
    {
      temp[i] = s.c_str()[i];
    }
  }
  temp[s.size()] = 0;
  return std::string(temp);
}

//
std::string MapHttpServer::_conv_transmitter_list_to_json()
{
  std::lock_guard<std::mutex> lock(mMutex);
  if (mTransmitters.empty())
    return "";

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

  QJsonDocument doc(jsonArray);
  return doc.toJson(QJsonDocument::Indented).toStdString();
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

  if ((mpCsvFP != nullptr) && ((type != MAP_RESET) && (type != MAP_FRAME)))
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
