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

#include "spyserver-handler.h"
#include "device-handler.h"
#include "ringbuffer.h"
#include "ui_spyserver-client.h"
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <atomic>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QHostAddress>
#include <QFrame>
#include <QByteArray>
#include <cstdio>
#ifdef HAVE_LIQUID
  #include <liquid/liquid.h>
#endif


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


  struct SSetting
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
  };

  SSetting mSettings;

private slots:
  void _slot_handle_connect_button();
  void _slot_handle_gain(i32);
  void _slot_handle_autogain(i32);
  void _slot_handle_checkTimer();

public slots:
  void slot_data_ready();

private:
  QFrame mFrame;
  RingBuffer<cf32> mRingBuffer2{32 * 32768};
  RingBuffer<u8> mRingBuffer1{32 * 32768};
  std::vector<u8> mByteBuffer;
  std::vector<cf32> mResampBuffer;
  std::unique_ptr<SpyServerHandler> mSpyServerHandler;
  QHostAddress mServerAddress;
  i64 mBasePort = 0;
  std::atomic<bool> mIsRunning = false;
  std::atomic<bool> mIsConnected = false;
  std::vector<cf32> mConvBuffer;
  i16 mConvIndex = 0;
  i16 mConvBufferSize = 0;

#ifdef HAVE_LIQUID
  resamp_crcf mLiquidResampler = nullptr;
#else
  std::vector<i16> mMapTable_int;
  std::vector<f32> mMapTable_float;
  i32 selectedRate = 0;
#endif

  bool _setup_connection();
  bool _check_and_cleanup_ip_address();
};
