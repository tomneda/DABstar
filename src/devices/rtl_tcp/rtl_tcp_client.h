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

#pragma once

#include "dab_constants.h"
#include "device_handler_if.h"
#include "device_notifier_if.h"
#include "ringbuffer.h"
#include "ui_rtl_tcp_widget.h"
#include <memory>
#include <QTcpSocket>

class XmlFileWriter;
class QSettings;

class RtlTcpClient final : public IDeviceNotifier, public IDeviceHandler, private Ui_rtl_tcp_widget
{
  Q_OBJECT
public:
  explicit RtlTcpClient(QSettings *, const QString & iRecorderVersion);
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
  QWidget * get_widget() override { return &mFrame; }
  QString deviceName() override;
  void resetBuffer() override;
  bool hasDump() override;
  bool startDumping() override;
  void stopDumping() override;

private:
  enum class EAgcMode { HW = 0, OFF = 1, SW = 2 };

  QFrame mFrame;
  QSettings * const mpSettings;
  std::array<f32, 256> mMapTable;
  std::unique_ptr<RingBuffer<cf32>> mpBuffer;
  i32 mBitRate;
  i32 mVfoFrequency;
  bool mIsConnected = false;
  i16 mGain;
  f64 mPpm;
  EAgcMode mAgcMode;
  i16 mBiasT;
  i16 mBandwidthKhz;
  QString mServerAddress;
  i32 mPort;
  QTcpSocket mTcpSocket;
  bool mDongleInfoReceived = false;
  const QString mRecorderVersion;
  QString mTunerText;
  FILE * mpXmlDumper = nullptr;
  std::unique_ptr<XmlFileWriter> mpXmlWriter;
  std::atomic<bool> mIsXmlDumping;

  // Sample flow supervision: with rtl_tcp the samples are fetched in the GUI thread, so any longer
  // blocking of the GUI lets the socket data pile up and the input ring buffer can overflow. The
  // resulting gaps corrupt the OFDM/FIC decoding, so they must not stay unnoticed.
  u64 mTotalSampleCnt = 0;
  u64 mDroppedSampleCnt = 0;
  u64 mDroppedSampleCntLastBurst = 0;
  bool mOverflowActive = false;

  void _send_vfo(i32);
  void _send_rate(i32);
  void _send_command(u8, i32);
  bool _setup_xml_dump();
  bool _setup_connection();
  bool _check_and_cleanup_ip_address();
  void _set_agc_mode(EAgcMode iAgcMode);
  void _show_connection_state(bool iIsConnected);
  void _show_error_state(const QString & iText);

private slots:
  void _slot_socket_error(QAbstractSocket::SocketError iSocketError);
  void _slot_socket_disconnected();
  void _slot_handle_connect_button();
  void _slot_handle_gain(i32);
  void _slot_handle_ppm(f64);
  void _slot_handle_biast(i32);
  void _slot_handle_bandwidth(i32);
  void _slot_read_data();
};


