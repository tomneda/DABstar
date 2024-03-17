#ifndef  RSPDX_HANDLER_H
#define  RSPDX_HANDLER_H

#include  "Rsp-device.h"

class SdrPlayHandler_v3;

class RspDx_handler : public Rsp_device
{
public:
  RspDx_handler(SdrPlayHandler_v3 * parent, sdrplay_api_DeviceT * chosenDevice, int sampleRate,
                int freq, bool agcMode, int lnaState, int GRdB, bool biasT);
  ~RspDx_handler() override = default;

  int lnaStates(int frequency) override;
  bool restart(int freq) override;
  bool set_agc(int setPoint, bool on) override;
  bool set_GRdB(int GRdBValue) override;
  bool set_ppm(int ppm) override;
  bool set_lna(int lnaState) override;
  bool set_antenna(int antenna) override;
  bool set_amPort(int amPort) override;
  bool set_biasT(bool biasT) override;

private:
  int16_t bankFor_rspdx(int freq);
  int get_lnaGain(int, int);
};

#endif



