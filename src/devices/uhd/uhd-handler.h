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


class UhdHandler final : public QObject, public deviceHandler, public Ui_uhdWidget
{
  Q_OBJECT
  friend class uhd_streamer;
public:
  explicit UhdHandler(QSettings * dabSettings);
  ~UhdHandler() override;

  void setVFOFrequency(int32_t freq) override;
  int32_t getVFOFrequency() override;
  bool restartReader(int32_t freq) override;
  void stopReader() override;
  int32_t getSamples(cmplx *, int32_t size) override;
  int32_t Samples() override;
  void resetBuffer() override;
  int16_t bitDepth() override;

private:
  QSettings * uhdSettings;
  QFrame myFrame{nullptr};
  uhd::usrp::multi_usrp::sptr m_usrp;
  uhd::rx_streamer::sptr m_rx_stream;
  RingBuffer<cmplx> * theBuffer = nullptr;
  uhd_streamer * m_workerHandle = nullptr;
  int32_t inputRate = 2048000;
  int32_t ringbufferSize = 1024;

  [[nodiscard]] int16_t _maxGain() const;

private slots:
  void _slot_set_external_gain(int) const;
  void _slot_set_f_correction(int) const;
  void _slot_set_khz_offset(int);
};

#endif

