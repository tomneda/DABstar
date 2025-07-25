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

#include "spyserver-tcp-client.h"

/*
 *	Thanks to Youssef Touil (well know from Airspy and spyServer),
 *	the structure of this program part is an adapted C++ translation
 *	of a C# fragment that he is using
 */
#ifdef __MINGW32__
#pragma comment(lib, "ws2_32.lib")
#else
# include <arpa/inet.h>
# include <sys/resource.h>
# include <sys/select.h>
# include <sys/ioctl.h>
# include <netdb.h>
# define ioctlsocket ioctl
#endif
#if defined(__MINGW32__) || defined(__APPLE__)
#ifndef MSG_NOSIGNAL
        #define MSG_NOSIGNAL 0
#endif
#endif

SpyServerTcpClient::SpyServerTcpClient(const QString & addr, i32 port,
                           RingBuffer<u8> * inBuffer)
  : outBuffer(32768)
{
  i32 RetCode;
  this->tcpAddress = addr;
  this->tcpPort = port;
  this->inBuffer = inBuffer;
  connected = false;

#ifdef	__MINGW32__
	WSAData wsaData;
	WSAStartup (MAKEWORD (2, 2), &wsaData);
	fprintf (stderr, "Client: Winsock DLL is %s\n",
	                                 wsaData. szSystemStatus);
#endif
  SendingSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef	__MINGW32__
	if (SendingSocket == INVALID_SOCKET) {
	   fprintf (stderr, "Client: socket failed: Error code: %ld\n", WSAGetLastError());
	   WSACleanup ();
	   return;
	}
#endif
  i32 bufSize = 8 * 32768;
  setsockopt(SendingSocket, SOL_SOCKET, SO_RCVBUF,
             (char *)&bufSize, sizeof(bufSize));
  ServerAddr.sin_family = AF_INET;
  ServerAddr.sin_port = htons(this->tcpPort);
  ServerAddr.sin_addr.s_addr =
    inet_addr(tcpAddress.toLatin1().data());

  RetCode = ::connect(SendingSocket,
                      (struct sockaddr *)&ServerAddr, sizeof(ServerAddr));
  if (RetCode != 0)
  {
#ifdef	__MINGW32__
	   printf ("Client: connect() failed! Error code: %ld\n",
	                                             WSAGetLastError());
#endif
    close(SendingSocket);
#ifdef	__MINGW32__
	   WSACleanup();
#endif
    return;
  }
  else
  {
    fprintf(stderr, "Client: connect() is OK, got connected...\n");
    fprintf(stderr, "Client: Ready for sending and/or receiving data...\n");
  }
  start();	// this is the reader
  connected = true;
}

SpyServerTcpClient::~SpyServerTcpClient()
{
  if (isRunning())
  {
    running.store(false);
    while (isRunning())
      usleep(1000);
  }
}

bool SpyServerTcpClient::is_connected()
{
  return connected;
}

void SpyServerTcpClient::connect_conn() {}

void SpyServerTcpClient::close_conn()
{
  if (SendingSocket > 0)
  {
    close(SendingSocket);
  }
}

void SpyServerTcpClient::send_data(u8 * data, i32 length)
{
  outBuffer.put_data_into_ring_buffer(data, length);
}

char tempBuffer_8[1000000];

void SpyServerTcpClient::run()
{
  i32 received = 0;
  struct timeval m_timeInterval;
  fd_set m_readFds;

  running.store(true);
  while (running.load())
  {
    FD_ZERO(&m_readFds);
    FD_SET(SendingSocket, &m_readFds);
    m_timeInterval.tv_usec = 100;
    m_timeInterval.tv_sec = 0;
    i32 m_receivingStatus = select(SendingSocket + 1, &m_readFds,
                                   nullptr, nullptr, &m_timeInterval);

    if (m_receivingStatus < 0)
    {
      std::cerr << "ERROR" << std::endl;
    }
    if (m_receivingStatus == 0)
    {
      i32 amount = outBuffer.get_ring_buffer_read_available();
      if (amount > 0)
      {
        (void)outBuffer.get_data_from_ring_buffer(tempBuffer_8, amount);
        send(SendingSocket, (char *)tempBuffer_8, amount, MSG_NOSIGNAL);
      }
    }
    else
    {
      sockaddr t;
#ifndef	__MINGW32__
      u32 tt = 10;
#else
	      i32	tt = 10;
#endif
      unsigned long bytesAvailable = 0;
      i32 ret = ioctlsocket(SendingSocket, FIONREAD, &bytesAvailable);
      (void)ret;
      received = recvfrom(SendingSocket, (char *)tempBuffer_8,
                          bytesAvailable, 0, &t, &tt);
      inBuffer->put_data_into_ring_buffer(tempBuffer_8, received);
    }
  }
}
