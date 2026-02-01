/*
 *    Copyright (C) 2014 .. 2023
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
 *
 *    Many of the ideas as implemented in Qt-DAB are derived from
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
 *
 */
#pragma once

#include	<QObject>
#include	<atomic>
#include	<thread>
#include	"device-handler.h"
#include	"ringbuffer.h"
#include	"soapy-converter.h"
#include	"ui_soapy-handler.h"
#include	<SoapySDR/Device.h>

class SoapySdr_Thread;
class QSettings;


class SoapyHandler : public QObject, public IDeviceHandler, public Ui_soapyWidget
{
  Q_OBJECT

public:
  SoapyHandler(QSettings *);
  ~SoapyHandler();

  /*
  virtual bool restartReader(i32 freq) = 0;
  virtual void stopReader() = 0;
  virtual void setVFOFrequency(i32) = 0;
  virtual i32 getVFOFrequency() = 0;
  virtual i32 getSamples(cf32 *, i32) = 0;
  virtual i32 Samples() = 0;
  virtual void resetBuffer() = 0;
  virtual i16 bitDepth() = 0;
  virtual void hide() = 0;
  virtual void show() = 0;
  virtual bool isHidden() = 0;
  virtual QString deviceName() = 0;
  virtual bool isFileInput() = 0;
   */

  bool restartReader(int) override;
  void stopReader() override;
  void setVFOFrequency(i32) override { throw std::runtime_error("Soapy does not support VFO frequency setting"); }
  i32 getVFOFrequency() override { return m_freq; }
  i16 bitDepth() override { return 32; }
  void hide() override { myFrame.hide(); }
  void show() override { myFrame.show(); }
  bool isHidden() override { return myFrame.isHidden(); }
  QString deviceName() override { return "SoapySDR"; };
  void resetBuffer() override;
  int32_t getSamples(std::complex<float> * Buffer, int32_t Size) override;
  int32_t Samples() override;
  bool isFileInput() override;

  float getGain() const;
  int32_t getGainCount();

private:
  RingBuffer<std::complex<float>> m_sampleBuffer;
  SoapyConverter theConverter;
  SoapySDRStream * rxStream;
  QFrame myFrame;

  void setAntenna(const std::string & antenna);
  void decreaseGain();
  void increaseGain();

  void createDevice(const QString &);
  int m_freq = 0;
  std::string m_driver_args;
  std::string m_antenna;
  SoapySDRDevice * m_device = nullptr;
  std::atomic<bool> m_running;
  std::atomic<bool> deviceReady;
  bool m_sw_agc = false;
  bool hasAgc;
  std::vector<double> m_gains;

  std::thread m_thread;
  void workerthread(void);
  void process(SoapySDRStream * stream);

  int findDesiredRange(SoapySDRRange * theRanges, int length);

private slots:
  void setAgc(int);
  void setGain(int);
};
