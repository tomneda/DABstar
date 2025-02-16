/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2020
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

#ifndef  SDRPLAY_HANDLER_V3_H
#define  SDRPLAY_HANDLER_V3_H

#include  <QThread>
#include  <QFrame>
#include  <QSettings>
#include  <QSemaphore>
#include  <QLibrary>
#include  <atomic>
#include  <cstdio>
#include  <queue>
#include  "dab-constants.h"
#include  "ringbuffer.h"
#include  "device-handler.h"
#include  "ui_sdrplay-widget-v3.h"
#include  <sdrplay_api.h>

class Rsp_device;

class generalCommand;

class XmlFileWriter;

class SdrPlayHandler_v3 final : public QThread, public IDeviceHandler, public Ui_sdrplayWidget_v3
{
Q_OBJECT
public:
  SdrPlayHandler_v3(QSettings *, const QString &);
  ~SdrPlayHandler_v3() override;

  void setVFOFrequency(int32_t) override;
  int32_t getVFOFrequency() override;
  bool restartReader(int32_t) override;
  void stopReader() override;
  int32_t getSamples(cmplx *, int32_t) override;
  int32_t Samples() override;
  void resetBuffer() override;
  int16_t bitDepth() override;
  void show() override;
  void hide() override;
  bool isHidden() override;
  QString deviceName() override;
  bool isFileInput() override;
  void update_PowerOverload(sdrplay_api_EventParamsT * params);
  RingBuffer<cmplx16> * const p_I_Buffer;
  std::atomic<bool> receiverRuns;

  sdrplay_api_CallbackFnsT cbFns;
  sdrplay_api_Open_t sdrplay_api_Open;
  sdrplay_api_Close_t sdrplay_api_Close;
  sdrplay_api_ApiVersion_t sdrplay_api_ApiVersion;
  sdrplay_api_LockDeviceApi_t sdrplay_api_LockDeviceApi;
  sdrplay_api_UnlockDeviceApi_t sdrplay_api_UnlockDeviceApi;
  sdrplay_api_GetDevices_t sdrplay_api_GetDevices;
  sdrplay_api_SelectDevice_t sdrplay_api_SelectDevice;
  sdrplay_api_ReleaseDevice_t sdrplay_api_ReleaseDevice;
  sdrplay_api_GetErrorString_t sdrplay_api_GetErrorString;
  sdrplay_api_GetLastError_t sdrplay_api_GetLastError;
  sdrplay_api_DebugEnable_t sdrplay_api_DebugEnable;
  sdrplay_api_GetDeviceParams_t sdrplay_api_GetDeviceParams;
  sdrplay_api_Init_t sdrplay_api_Init;
  sdrplay_api_Uninit_t sdrplay_api_Uninit;
  sdrplay_api_Update_t sdrplay_api_Update;
  sdrplay_api_SwapRspDuoActiveTuner_t sdrplay_api_SwapRspDuoActiveTuner;
  sdrplay_api_DeviceT * chosenDevice;
  Rsp_device * theRsp = nullptr;

  std::atomic<bool> failFlag;
  std::atomic<bool> successFlag;
  std::atomic<bool> threadRuns;
  void run() override;
  bool messageHandler(generalCommand *);
  QString recorderVersion;
  int32_t vfoFrequency;
  int16_t hwVersion;
  QSettings * sdrplaySettings;
  bool agcMode;
  int lna_upperBound;
  float apiVersion;
  bool has_antennaSelect;
  QString deviceModel = "SDRplay";
  int GRdBValue;
  int lnaState;
  double ppmValue;
  bool biasT;
  bool notch;
  FILE * xmlDumper;
  XmlFileWriter * xmlWriter;
  int startupCnt = 0;
  bool setup_xmlDump();
  void close_xmlDump();
  std::atomic<bool> dumping;
  std::queue<generalCommand *> server_queue;
  QSemaphore serverjobs;
  bool loadFunctions();
  int errorCode;

private:
  QFrame myFrame;
  QLibrary * phandle;

  // tomneda: was at 14bit but it seems the whole 16bits are used in the callback function (for the RSPdx)
  static constexpr int16_t nrBits = 16;

  void set_deviceName(const QString &);
  void set_serial(const QString &);
  void set_apiVersion(float);

private slots:
  void set_ifgainReduction(int);
  void set_lnagainReduction(int);
  void set_agcControl(int);
  void set_ppmControl(double);
  void set_selectAntenna(const QString &);
  void set_biasT(int);
  void set_notch(int);
  void slot_overload_detected(bool);
  void slot_tuner_gain(double, int);
public slots:
  void set_lnabounds(int, int);
  void set_antennaSelect(int);
  void set_xmlDump();
  void show_lnaGain(int);
signals:
  void set_antennaSelect_signal(bool);
  void signal_overload_detected(bool);
  void signal_tuner_gain(double, int);
};

#endif

