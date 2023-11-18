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
  volatile bool m_stop_signal_called;
};


class UhdHandler : public QObject, public deviceHandler, public Ui_uhdWidget
{
  Q_OBJECT

  friend class uhd_streamer;

public:
  explicit UhdHandler(QSettings * dabSettings);
  ~UhdHandler() override;
  void setVFOFrequency(int32_t freq) override;
  int32_t getVFOFrequency() override;

//  bool legalFrequency(int32_t) { return true; }
//  int32_t defaultFrequency() { return 100000000; }

  bool restartReader(int32_t freq) override;
  void stopReader() override;
  int32_t getSamples(cmplx *, int32_t size) override;
  int32_t Samples() override;
  //uint8_t myIdentity();
  void resetBuffer() override;
  virtual int16_t maxGain();
  int16_t bitDepth() override;

private:
  QSettings * uhdSettings;
  QFrame * myFrame;
  uhd::usrp::multi_usrp::sptr m_usrp;
  uhd::rx_streamer::sptr m_rx_stream;
  RingBuffer<cmplx> * theBuffer;
  uhd_streamer * m_workerHandle;
  int32_t inputRate;
  int32_t ringbufferSize;
  
private slots:
  void setExternalGain(int);
  void set_fCorrection(int);
  void set_KhzOffset(int);
};

#endif

