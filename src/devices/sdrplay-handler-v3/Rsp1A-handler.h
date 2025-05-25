#ifndef  RSP1A_HANDLER_H
#define  RSP1A_HANDLER_H

#include  "Rsp-device.h"

class SdrPlayHandler_v3;

class Rsp1A_handler : public Rsp_device
{
public:
  Rsp1A_handler(SdrPlayHandler_v3 * parent, sdrplay_api_DeviceT * chosenDevice, int freq,
                bool agcMode, int lnaState, int GRdB, bool biasT, bool notch, f64 ppmValue);
  ~Rsp1A_handler() override = default;

  int lnaStates(int frequency) override;
  bool restart(int freq) override;
  bool set_lna(int lnaState) override;
  bool set_biasT(bool) override;
  bool set_notch(bool) override;
};

#endif



