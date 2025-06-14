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

#include	<QObject>
#include	<QSettings>
#include	<QTimer>
#include	<atomic>
#include	<QLabel>
#include	<QMessageBox>
#include	<QLineEdit>
#include	<QHostAddress>
#include	<QFrame>
#include	<QByteArray>
#include	<cstdio>
#include	"dab-constants.h"
#include	"device-handler.h"
#include	"ringbuffer.h"
#include	"ui_spyserver-client.h"

#include	"spyserver-handler.h"


class SpyServerClient : public QObject, public IDeviceHandler, private Ui_spyServer_widget_8
{
  Q_OBJECT

public:
  SpyServerClient(QSettings *);
  ~SpyServerClient() override;

  bool restartReader(int32_t) override;
  void stopReader() override;
  int32_t getSamples(std::complex<float> * V, int32_t size) override;
  int32_t Samples() override;
  int16_t bitDepth() override;

  void setVFOFrequency(int32_t) override;
  int32_t getVFOFrequency() override;
  void resetBuffer() override;
  void show() override;
  void hide() override;
  bool isHidden() override;
  QString deviceName() override;
  bool isFileInput() override;

  // void connect_on();
  int32_t getRate();


  struct
  {
    int gain = 0;
    int basePort = 0;
    QString ipAddress;
    uint32_t sample_rate = 0;
    float resample_ratio = 0;
    int desired_decim_stage = 0;
    int resample_quality = 0;
    int batchSize = 0;
    int sample_bits = 0;
    bool auto_gain = false;
  } settings;

private slots:
  void _slot_set_connection();
  void _slot_connect();
  void _slot_handle_gain(int);
  void _slot_handle_autogain(int);
  void _slot_handle_checkTimer();

public slots:
  void slot_data_ready();

private:
  QFrame myFrame;
  RingBuffer<std::complex<float>> _I_Buffer{32 * 32768};
  RingBuffer<uint8_t> tmpBuffer{32 * 32768};
  // QTimer checkTimer;
  std::unique_ptr<SpyServerHandler> theServer;
  // QLineEdit * hostLineEdit;
  bool isvalidRate(int32_t);
  // QSettings * spyServer_settings;
  int32_t theRate;
  QHostAddress serverAddress;
  qint64 basePort;
  std::atomic<bool> running = false;
  std::atomic<bool> connected = false;
  // std::atomic<bool> onConnect = false;
  // std::atomic<bool> timedOut = false;

  int16_t convBufferSize;
  int16_t convIndex = 0;
  std::vector<std::complex<float>> convBuffer;
  int16_t mapTable_int[4 * 512];
  float mapTable_float[4 * 512];
  int selectedRate;

  bool _check_and_cleanup_ip_address();
};
