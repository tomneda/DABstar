/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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

#ifndef SDRPLAY_HANDLER_H
#define SDRPLAY_HANDLER_H

#include  <QObject>
#include  <QPoint>
#include  <QFrame>
#include  <QSettings>
#include  <atomic>
#include  "dab-constants.h"
#include  "ringbuffer.h"
#include  "device-handler.h"
#include  "ui_sdrplay-widget-v2.h"
#include  "mirsdrapi-rsp.h"
#include  <QLibrary>

typedef void (* mir_sdr_StreamCallback_t)(i16 * xi, i16 * xq, u32 firstSampleNum, i32 grChanged, i32 rfChanged, i32 fsChanged, u32 numSamples, u32 reset, u32 hwRemoved, void * cbContext);
typedef void (* mir_sdr_GainChangeCallback_t)(u32 gRdB, u32 lnaGRdB, void * cbContext);

#ifdef _WIN32
#define	GETPROCADDRESS	GetProcAddress
#else
#define  GETPROCADDRESS  dlsym
#endif

class XmlFileWriter;

// Dll and ".so" function prototypes
typedef mir_sdr_ErrT (* pfn_mir_sdr_StreamInit)(i32 * gRdB, f64 fsMHz, f64 rfMHz, mir_sdr_Bw_MHzT bwType, mir_sdr_If_kHzT ifType, i32 LNAEnable, i32 * gRdBsystem, i32 useGrAltMode, i32 * samplesPerPacket, mir_sdr_StreamCallback_t StreamCbFn, mir_sdr_GainChangeCallback_t GainChangeCbFn, void * cbContext);
typedef mir_sdr_ErrT (* pfn_mir_sdr_Reinit)(i32 * gRdB, f64 fsMHz, f64 rfMHz, mir_sdr_Bw_MHzT bwType, mir_sdr_If_kHzT ifType, mir_sdr_LoModeT, i32, i32 *, i32, i32 *, mir_sdr_ReasonForReinitT);
typedef mir_sdr_ErrT (* pfn_mir_sdr_StreamUninit)();
typedef mir_sdr_ErrT (* pfn_mir_sdr_SetRf)(f64 drfHz, i32 abs, i32 syncUpdate);
typedef mir_sdr_ErrT (* pfn_mir_sdr_SetFs)(f64 dfsHz, i32 abs, i32 syncUpdate, i32 reCal);
typedef mir_sdr_ErrT (* pfn_mir_sdr_SetGr)(i32 gRdB, i32 abs, i32 syncUpdate);
typedef mir_sdr_ErrT (* pfn_mir_sdr_RSP_SetGr)(i32 gRdB, i32 lnaState, i32 abs, i32 syncUpdate);
typedef mir_sdr_ErrT (* pfn_mir_sdr_SetGrParams)(i32 minimumGr, i32 lnaGrThreshold);
typedef mir_sdr_ErrT (* pfn_mir_sdr_SetDcMode)(i32 dcCal, i32 speedUp);
typedef mir_sdr_ErrT (* pfn_mir_sdr_SetDcTrackTime)(i32 trackTime);
typedef mir_sdr_ErrT (* pfn_mir_sdr_SetSyncUpdateSampleNum)(u32 sampleNum);
typedef mir_sdr_ErrT (* pfn_mir_sdr_SetSyncUpdatePeriod)(u32 period);
typedef mir_sdr_ErrT (* pfn_mir_sdr_ApiVersion)(f32 * version);
typedef mir_sdr_ErrT (* pfn_mir_sdr_ResetUpdateFlags)(i32 resetGainUpdate, i32 resetRfUpdate, i32 resetFsUpdate);
typedef mir_sdr_ErrT (* pfn_mir_sdr_AgcControl)(u32, i32, i32, u32, u32, i32, i32);
typedef mir_sdr_ErrT (* pfn_mir_sdr_DCoffsetIQimbalanceControl)(u32, u32);
typedef mir_sdr_ErrT (* pfn_mir_sdr_SetPpm)(f64);
typedef mir_sdr_ErrT (* pfn_mir_sdr_DebugEnable)(u32);
typedef mir_sdr_ErrT (* pfn_mir_sdr_GetDevices)(mir_sdr_DeviceT *, u32 *, u32);
typedef mir_sdr_ErrT (* pfn_mir_sdr_GetCurrentGain)(mir_sdr_GainValuesT *);
typedef mir_sdr_ErrT (* pfn_mir_sdr_GetHwVersion)(u8 *);
typedef mir_sdr_ErrT (* pfn_mir_sdr_RSPII_AntennaControl)(mir_sdr_RSPII_AntennaSelectT);
typedef mir_sdr_ErrT (* pfn_mir_sdr_rspDuo_TunerSel)(mir_sdr_rspDuo_TunerSelT);
typedef mir_sdr_ErrT (* pfn_mir_sdr_SetDeviceIdx)(u32);
typedef mir_sdr_ErrT (* pfn_mir_sdr_ReleaseDeviceIdx)(u32);
typedef mir_sdr_ErrT (* pfn_mir_sdr_RSPII_RfNotchEnable)(u32);

typedef mir_sdr_ErrT (* pfn_mir_sdr_RSPII_BiasTControl)(u32 enable);
typedef mir_sdr_ErrT (* pfn_mir_sdr_rsp1a_BiasT)(i32 enable);
typedef mir_sdr_ErrT (* pfn_mir_sdr_rspDuo_BiasT)(i32 enable);

///////////////////////////////////////////////////////////////////////////
class SdrPlayHandler_v2 final : public QObject, public IDeviceHandler, public Ui_sdrplayWidget
{
Q_OBJECT
public:
  SdrPlayHandler_v2(QSettings * s, const QString & recorderVersion);
  ~SdrPlayHandler_v2() override;

  void setVFOFrequency(i32) override;
  i32 getVFOFrequency() override;
  bool restartReader(i32) override;
  void stopReader() override;
  i32 getSamples(cf32 *, i32) override;
  i32 Samples() override;
  void resetBuffer() override;
  i16 bitDepth() override;
  QString deviceName() override;
  void show() override;
  void hide() override;
  bool isHidden() override;
  bool isFileInput() override;


  void adjustFreq(i32);
  i32 defaultFrequency();
  QPoint get_coords();
  void moveTo(QPoint);
  //
  //	The buffer should be visible by the callback function
  RingBuffer<std::complex<i16>> _I_Buffer;
  f32 denominator;

private:
  QFrame myFrame;
  pfn_mir_sdr_StreamInit my_mir_sdr_StreamInit;
  pfn_mir_sdr_Reinit my_mir_sdr_Reinit;
  pfn_mir_sdr_StreamUninit my_mir_sdr_StreamUninit;
  pfn_mir_sdr_SetRf my_mir_sdr_SetRf;
  pfn_mir_sdr_SetFs my_mir_sdr_SetFs;
  pfn_mir_sdr_SetGr my_mir_sdr_SetGr;
  pfn_mir_sdr_RSP_SetGr my_mir_sdr_RSP_SetGr;
  pfn_mir_sdr_SetGrParams my_mir_sdr_SetGrParams;
  pfn_mir_sdr_SetDcMode my_mir_sdr_SetDcMode;
  pfn_mir_sdr_SetDcTrackTime my_mir_sdr_SetDcTrackTime;
  pfn_mir_sdr_SetSyncUpdateSampleNum my_mir_sdr_SetSyncUpdateSampleNum;
  pfn_mir_sdr_SetSyncUpdatePeriod my_mir_sdr_SetSyncUpdatePeriod;
  pfn_mir_sdr_ApiVersion my_mir_sdr_ApiVersion;
  pfn_mir_sdr_ResetUpdateFlags my_mir_sdr_ResetUpdateFlags;
  pfn_mir_sdr_AgcControl my_mir_sdr_AgcControl;
  pfn_mir_sdr_DCoffsetIQimbalanceControl my_mir_sdr_DCoffsetIQimbalanceControl;
  pfn_mir_sdr_SetPpm my_mir_sdr_SetPpm;
  pfn_mir_sdr_DebugEnable my_mir_sdr_DebugEnable;
  pfn_mir_sdr_GetDevices my_mir_sdr_GetDevices;
  pfn_mir_sdr_GetCurrentGain my_mir_sdr_GetCurrentGain;
  pfn_mir_sdr_GetHwVersion my_mir_sdr_GetHwVersion;
  pfn_mir_sdr_RSPII_AntennaControl my_mir_sdr_RSPII_AntennaControl;
  pfn_mir_sdr_rspDuo_TunerSel my_mir_sdr_rspDuo_TunerSel;
  pfn_mir_sdr_SetDeviceIdx my_mir_sdr_SetDeviceIdx;
  pfn_mir_sdr_ReleaseDeviceIdx my_mir_sdr_ReleaseDeviceIdx;
  pfn_mir_sdr_RSPII_RfNotchEnable my_mir_sdr_RSPII_RfNotchEnable;

  pfn_mir_sdr_RSPII_BiasTControl my_mir_sdr_RSPII_BiasTControl;
  pfn_mir_sdr_rsp1a_BiasT my_mir_sdr_rsp1a_BiasT;
  pfn_mir_sdr_rspDuo_BiasT my_mir_sdr_rspDuo_BiasT;

  QString recorderVersion;
  QString deviceModel;
  bool fetchLibrary();
  void releaseLibrary();
  QString errorCodes(mir_sdr_ErrT);
  i16 hwVersion;
  u32 numofDevs;
  i16 deviceIndex;
  bool loadFunctions();
  QSettings * sdrplaySettings;
  i32 inputRate;
  i32 vfoFrequency;
  bool libraryLoaded;
  std::atomic<bool> running;
  HINSTANCE Handle;
  bool agcMode;
  i16 nrBits;

  i32 lnaMax;
  FILE * xmlDumper;
  XmlFileWriter * xmlWriter;
  bool setup_xmlDump();
  void close_xmlDump();
  std::atomic<bool> dumping;
  //	experimental
  void record_gainSettings(i32);
  void update_gainSettings(i32);
  bool save_gainSettings;
signals:
  void new_GRdBValue(i32);
  void new_lnaValue(i32);
  void new_agcSetting(bool);
private slots:
  void set_ifgainReduction(i32);
  void set_lnagainReduction(i32);
  void set_agcControl(i32);
  void set_debugControl(i32);
  void set_ppmControl(i32);
  void set_antennaSelect(const QString &);
  void set_tunerSelect(const QString &);
  void set_xmlDump();
  void voidSignal(i32);
  void biasT_selectorHandler(i32);
};

#endif

