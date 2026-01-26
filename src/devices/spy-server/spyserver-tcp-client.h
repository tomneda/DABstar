/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2016 .. 2023
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
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
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#pragma once

#ifdef __MINGW32__
  #include <winsock2.h>
  #include <windows.h>
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
#endif
#if defined(__GNUC__) || defined(__MINGW32__)
  #include <unistd.h>
#endif
#include "ringbuffer.h"
#include <atomic>
#include <mutex>
#include <QThread>


class SpyServerTcpClient : public QThread
{
  Q_OBJECT

private:
  QString tcpAddress;
  i32 tcpPort;
  RingBuffer<u8> * inBuffer;
  RingBuffer<u8> outBuffer;
  std::mutex locker;
  bool connected;
  SOCKET SendingSocket;
  struct sockaddr_in ServerAddr;
  void run();
  std::atomic<bool> running;

public:
  SpyServerTcpClient(const QString & addr, i32 port,
               RingBuffer<u8> * inBuffer);
  ~SpyServerTcpClient();

  bool is_connected();
  void connect_conn();
  void close_conn();
  void send_data(u8 * data, i32 length);
};
