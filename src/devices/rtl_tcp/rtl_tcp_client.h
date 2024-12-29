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
  void setVFOFrequency(int32_t) override;
  int32_t getVFOFrequency() override;
  bool restartReader(int32_t) override;
  void stopReader() override;
  int32_t getSamples(cmplx * V, int32_t size) override;
  int32_t Samples() override;
  void show() override;
  void hide() override;
  bool isHidden() override;
  int16_t bitDepth() override;
  QString deviceName() override;
  bool isFileInput() override;
  void resetBuffer() override;
  int32_t getRate();
  int32_t defaultFrequency();

private:
  QFrame myFrame;
  void sendVFO(int32_t);
  void sendRate(int32_t);
  void sendCommand(uint8_t, int32_t);
  QLineEdit * hostLineEdit;
  bool isvalidRate(int32_t);
  void setAgcMode(int);
  QSettings * remoteSettings;
  int32_t Bitrate;
  int32_t vfoFrequency;
  RingBuffer<cmplx> * _I_Buffer;
  bool connected;
  int16_t Gain;
  double Ppm;
  int16_t AgcMode;
  int16_t BiasT;
  int16_t Bandwidth;
  QString ipAddress;
  QTcpSocket toServer;
  qint64 basePort;
  int32_t vfoOffset = 0;
  bool dumping;
  FILE * dumpfilePointer;
  float mapTable[256];
  bool dongle_info_received = false;

private slots:
  void sendGain(int);
  void set_fCorrection(double);
  void readData();
  void wantConnect();
  void setDisconnect();
  void setBiasT(int);
  void setBandwidth(int);
  void setPort(int);
  void setAddress();
  void handle_hw_agc();
  void handle_sw_agc();
  void handle_manual();
};

#endif

