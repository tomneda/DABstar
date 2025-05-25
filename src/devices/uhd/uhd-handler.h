/*
 *    Copyright (C) 2015
 *    Sebastian Held <sebastian.held@imst.de>
 *
 *    This file is part of Qt-DAB
 *    Many of the ideas as implemented in SDR-J are derived from
 *    other work, made available through the GNU general Public License.
 *    All copyrights of the original authors are recognized.
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
#ifndef  UHD_INPUT_H
#define  UHD_INPUT_H


#include  <QThread>
#include  <QSettings>
#include  <QFrame>
#include  <QObject>
#include  <uhd/usrp/multi_usrp.hpp>
#include  "ui_uhd-widget.h"
#include  "device-handler.h"
#include  "ringbuffer.h"

class UhdHandler;

class uhd_streamer : public QThread
{
public:
  explicit uhd_streamer(UhdHandler * d);
  void stop();

private:
  UhdHandler * m_theStick;
  void run() override;
  std::atomic<bool> m_stop_signal_called = false;
};


class UhdHandler final : public QObject, public IDeviceHandler, public Ui_uhdWidget
{
  Q_OBJECT
  friend class uhd_streamer;
public:
  explicit UhdHandler(QSettings * dabSettings);
  ~UhdHandler() override;

  void setVFOFrequency(i32 freq) override;
  i32 getVFOFrequency() override;
  bool restartReader(i32 freq) override;
  void stopReader() override;
  i32 getSamples(cf32 *, i32 size) override;
  i32 Samples() override;
  void resetBuffer() override;
  i16 bitDepth() override;
  void show() override;
  void hide() override;
  bool isHidden() override;
  QString deviceName() override;
  bool isFileInput() override;

private:
  static constexpr char SETTING_GROUP_NAME[] = "uhdSettings";
  QSettings * uhdSettings;
  QFrame myFrame{nullptr};
  uhd::usrp::multi_usrp::sptr m_usrp;
  uhd::rx_streamer::sptr m_rx_stream;
  RingBuffer<cf32> * theBuffer = nullptr;
  uhd_streamer * m_workerHandle = nullptr;
  i32 inputRate = 2048000;
  i32 ringbufferSize = 1024;
  i32 vfoOffset = 0;

  [[nodiscard]] i16 _maxGain() const;
  void _load_save_combobox_settings(QComboBox * ipCmb, const QString & iName, bool iSave);

private slots:
  void _slot_set_external_gain(int) const;
  void _slot_set_f_correction(int) const;
  void _slot_set_khz_offset(int);
  void _slot_handle_ant_selector(const QString &);
};

#endif

