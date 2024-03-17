#ifndef  RSP_DEVICE_H
#define  RSP_DEVICE_H

#include  <QObject>
#include  <cstdint>
#include  <cstdio>
#include  <sdrplay_api.h>

class SdrPlayHandler_v3;

class Rsp_device : public QObject
{
Q_OBJECT
protected:
  sdrplay_api_DeviceT * mpChosenDevice;
  int mSampleRate;
  int mFreq;
  bool mAgcMode;
  int mLnaState;
  int mGRdB;
  sdrplay_api_RxChannelParamsT * mpChParams;
  sdrplay_api_DeviceParamsT * mpDeviceParams;
  SdrPlayHandler_v3 * mpParent;
  int mLna_upperBound;
  QString mDeviceModel;
  bool mAntennaSelect;
  int mNrBits;
  bool mBiasT;

public:
  Rsp_device(SdrPlayHandler_v3 * parent, sdrplay_api_DeviceT * chosenDevice, int sampleRate,
             int startFrequency, bool agcMode, int lnaState, int GRdB, bool biasT);
  ~Rsp_device() override;

  virtual int lnaStates(int frequency);
  virtual bool restart(int freq);
  virtual bool set_agc(int setPoint, bool on);
  virtual bool set_lna(int lnaState);
  virtual bool set_GRdB(int GRdBValue);
  virtual bool set_ppm(int ppm);
  virtual bool set_antenna(int antenna);
  virtual bool set_amPort(int amPort);
  virtual bool set_biasT(bool biasT);

signals:
  void signal_set_lnabounds(int, int);
  void signal_set_deviceName(const QString &);
  void signal_set_antennaSelect(int);
  void signal_set_nrBits(int);
  void signal_show_lnaGain(int);
};

#endif


