#ifndef  RSPDX_HANDLER_H
#define  RSPDX_HANDLER_H

#include  "Rsp-device.h"

class SdrPlayHandler_v3;

class RspDx_handler : public Rsp_device
{
public:
  RspDx_handler(SdrPlayHandler_v3 * parent, sdrplay_api_DeviceT * chosenDevice,
                int freq, bool agcMode, int lnaState, int GRdB, bool biasT, bool notch, f64 ppmValue);
  ~RspDx_handler() override = default;

  int lnaStates(int frequency) override;
  bool restart(int freq) override;
  bool set_lna(int lnaState) override;
  bool set_antenna(int antenna) override;
  bool set_amPort(int amPort) override;
  bool set_biasT(bool biasT) override;
  bool set_notch(bool) override;
};

#endif



