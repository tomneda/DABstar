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

  bool restartReader(i32) override;
  void stopReader() override;
  i32 getSamples(cf32 * V, i32 size) override;
  i32 Samples() override;
  i16 bitDepth() override;

  void setVFOFrequency(i32) override;
  i32 getVFOFrequency() override;
  void resetBuffer() override;
  void show() override;
  void hide() override;
  bool isHidden() override;
  QString deviceName() override;
  bool isFileInput() override;

  // void connect_on();
  i32 getRate();


  struct
  {
    i32 gain = 0;
    i32 basePort = 0;
    QString ipAddress;
    u32 sample_rate = 0;
    f32 resample_ratio = 0;
    i32 desired_decim_stage = 0;
    i32 resample_quality = 0;
    i32 batchSize = 0;
    i32 sample_bits = 0;
    bool auto_gain = false;
  } settings;

private slots:
  void _slot_handle_connect_button();
  void _slot_handle_gain(i32);
  void _slot_handle_autogain(i32);
  void _slot_handle_checkTimer();

public slots:
  void slot_data_ready();

private:
  QFrame myFrame;
  RingBuffer<cf32> _I_Buffer{32 * 32768};
  RingBuffer<u8> tmpBuffer{32 * 32768};
  std::unique_ptr<SpyServerHandler> theServer;
  i32 theRate;
  QHostAddress serverAddress;
  i64 basePort;
  std::atomic<bool> running = false;
  std::atomic<bool> connected = false;
  // std::atomic<bool> onConnect = false;
  // std::atomic<bool> timedOut = false;

  i16 convBufferSize;
  i16 convIndex = 0;
  std::vector<cf32> convBuffer;
  i16 mapTable_int[4 * 512];
  f32 mapTable_float[4 * 512];
  i32 selectedRate;

  bool _setup_connection();
  bool _check_and_cleanup_ip_address();
};
