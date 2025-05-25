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
  int mFreq;
  int mLnaState;
  sdrplay_api_RxChannelParamsT * mpChParams;
  sdrplay_api_DeviceParamsT * mpDeviceParams;
  SdrPlayHandler_v3 * mpParent;

public:
  Rsp_device(SdrPlayHandler_v3 * parent, sdrplay_api_DeviceT * chosenDevice,
             int startFrequency, bool agcMode, int lnaState, int GRdB, f64 ppmValue);
  ~Rsp_device() override;

  virtual int lnaStates(int frequency);
  virtual bool restart(int freq);
  bool set_agc(int setPoint, bool on);
  virtual bool set_lna(int lnaState);
  bool set_GRdB(int GRdBValue);
  bool set_ppm(f64 ppm);
  virtual bool set_antenna(int antenna);
  virtual bool set_amPort(int amPort);
  virtual bool set_biasT(bool biasT);
  virtual bool set_notch(bool notch);

signals:
  void signal_set_lnabounds(int, int);
  void signal_set_antennaSelect(int);
  void signal_show_lnaGain(int);
};

#endif


