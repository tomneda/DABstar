/*
 *    Copyright (C) 2014 .. 2017
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

#ifndef __SOAPY_HANDLER__
#define __SOAPY_HANDLER__

#include <QObject>
#include <QFrame>
#include <QSettings>
#include <atomic>
#include "device-handler.h"
#include "ringbuffer.h"
#include "ui_soapy-handler.h"
#include "soapy-worker.h"
#include <SoapySDR/Device.hpp>

class SoapyHandler : public QObject, public IDeviceHandler, public Ui_soapyWidget
{
  Q_OBJECT

public:
  SoapyHandler(QSettings *);
  ~SoapyHandler(void);

  bool restartReader(i32) override;
  void stopReader() override;
  void setVFOFrequency(i32) override;
  i32 getVFOFrequency(void) override;
  i16 bitDepth(void) override;
  void show() override;
  void hide() override;
  bool isHidden() override;
  QString deviceName() override { return "SoapySDR"; };
  void resetBuffer(void) override;
  i32 getSamples(cf32 *, i32) override;
  i32 Samples() override;
  bool isFileInput() override;

private:
  QFrame myFrame;
  SoapySDR::Device * device;
  SoapySDR::Stream * stream;
  QSettings * soapySettings;
  std::vector<std::string> gainsList;
  soapyWorker * worker;
  void createDevice(const QString d, const QString s);
  int findDesiredRange(const SoapySDR::RangeList &theRanges);
  //SoapyConverter theConverter;

private slots:
  void handle_spinBox_1(i32);
  void handle_spinBox_2(i32);
  void set_agcControl(i32);
  void handleAntenna(const QString &);
};
#endif
