/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013 .. 2017
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

#ifndef  RTL_TCP_CLIENT_H
#define  RTL_TCP_CLIENT_H

#include  <QtNetwork>
#include  <QSettings>
#include  <QLabel>
#include  <QMessageBox>
#include  <QLineEdit>
#include  <QHostAddress>
#include  <QByteArray>
#include  <QTcpSocket>
#include  <QTimer>
#include  <QComboBox>
#include  <cstdio>
#include  "dab-constants.h"
#include  "device-handler.h"
#include  "ringbuffer.h"
#include  "ui_rtl_tcp-widget.h"

class RtlTcpClient final : public QObject, public IDeviceHandler, Ui_rtl_tcp_widget
{
Q_OBJECT
public:
  explicit RtlTcpClient(QSettings *);
  ~RtlTcpClient() override;
  void setVFOFrequency(i32) override;
  i32 getVFOFrequency() override;
  bool restartReader(i32) override;
  void stopReader() override;
  i32 getSamples(cf32 * V, i32 size) override;
  i32 Samples() override;
  void show() override;
  void hide() override;
  bool isHidden() override;
  i16 bitDepth() override;
  QString deviceName() override;
  bool isFileInput() override;
  void resetBuffer() override;
  i32 getRate();
  i32 defaultFrequency();

private:
  QFrame myFrame;
  void sendVFO(i32);
  void sendRate(i32);
  void sendCommand(u8, i32);
  QLineEdit * hostLineEdit;
  bool isvalidRate(i32);
  void setAgcMode(i32);
  QSettings * remoteSettings;
  i32 Bitrate;
  i32 vfoFrequency;
  RingBuffer<cf32> * _I_Buffer;
  bool connected;
  i16 Gain;
  f64 Ppm;
  i16 AgcMode;
  i16 BiasT;
  i16 Bandwidth;
  QString ipAddress;
  QTcpSocket toServer;
  qint64 basePort;
  i32 vfoOffset = 0;
  bool dumping;
  FILE * dumpfilePointer;
  f32 mapTable[256];
  bool dongle_info_received = false;

private slots:
  void sendGain(i32);
  void set_fCorrection(f64);
  void readData();
  void wantConnect();
  void setDisconnect();
  void setBiasT(i32);
  void setBandwidth(i32);
  void setPort(i32);
  void setAddress();
  void handle_hw_agc();
  void handle_sw_agc();
  void handle_manual();
};

#endif

