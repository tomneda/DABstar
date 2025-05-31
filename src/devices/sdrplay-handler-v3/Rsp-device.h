#ifndef  RSP_DEVICE_H
#define  RSP_DEVICE_H

#include "glob_data_types.h"
#include <QObject>
#include <cstdio>
#include <sdrplay_api.h>

class SdrPlayHandler_v3;

class Rsp_device : public QObject
{
Q_OBJECT
protected:
  sdrplay_api_DeviceT * mpChosenDevice;
  i32 mFreq;
  i32 mLnaState;
  sdrplay_api_RxChannelParamsT * mpChParams;
  sdrplay_api_DeviceParamsT * mpDeviceParams;
  SdrPlayHandler_v3 * mpParent;

public:
  Rsp_device(SdrPlayHandler_v3 * parent, sdrplay_api_DeviceT * chosenDevice,
             i32 startFrequency, bool agcMode, i32 lnaState, i32 GRdB, f64 ppmValue);
  ~Rsp_device() override;

  virtual i32 lnaStates(i32 frequency);
  virtual bool restart(i32 freq);
  bool set_agc(i32 setPoint, bool on);
  virtual bool set_lna(i32 lnaState);
  bool set_GRdB(i32 GRdBValue);
  bool set_ppm(f64 ppm);
  virtual bool set_antenna(i32 antenna);
  virtual bool set_amPort(i32 amPort);
  virtual bool set_biasT(bool biasT);
  virtual bool set_notch(bool notch);

signals:
  void signal_set_lnabounds(i32, i32);
  void signal_set_antennaSelect(i32);
  void signal_show_lnaGain(i32);
};

#endif


