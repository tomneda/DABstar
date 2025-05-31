#ifndef  RSPDUO_HANDLER_H
#define  RSPDUO_HANDLER_H

#include  "Rsp-device.h"

class SdrPlayHandler_v3;

class RspDuo_handler : public Rsp_device
{
public:
  RspDuo_handler(SdrPlayHandler_v3 * parent, sdrplay_api_DeviceT * chosenDevice,
                 i32 freq, bool agcMode, i32 lnaState, i32 GRdB, bool biasT, bool notch, f64 ppmValue);
  ~RspDuo_handler() override = default;

  i32 lnaStates(i32 frequency) override;
  bool restart(i32 freq) override;
  bool set_lna(i32 lnaState) override;
  bool set_antenna(i32 antenna) override;
  bool set_biasT(bool biasT) override;
  bool set_notch(bool) override;
  bool set_amPort(i32 amPort) override;
};

#endif



