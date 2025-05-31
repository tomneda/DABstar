#ifndef  RSP1A_HANDLER_H
#define  RSP1A_HANDLER_H

#include  "Rsp-device.h"

class SdrPlayHandler_v3;

class Rsp1A_handler : public Rsp_device
{
public:
  Rsp1A_handler(SdrPlayHandler_v3 * parent, sdrplay_api_DeviceT * chosenDevice, i32 freq,
                bool agcMode, i32 lnaState, i32 GRdB, bool biasT, bool notch, f64 ppmValue);
  ~Rsp1A_handler() override = default;

  i32 lnaStates(i32 frequency) override;
  bool restart(i32 freq) override;
  bool set_lna(i32 lnaState) override;
  bool set_biasT(bool) override;
  bool set_notch(bool) override;
};

#endif



