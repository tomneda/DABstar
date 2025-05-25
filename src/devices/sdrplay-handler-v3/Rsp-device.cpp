#include  "Rsp-device.h"

#include  <stdint.h>
#include  <sdrplay_api.h>
#include  "sdrplay-handler-v3.h"

Rsp_device::Rsp_device(SdrPlayHandler_v3 *parent, sdrplay_api_DeviceT *chosenDevice, int startFreq, bool agcMode, int lnaState, int GRdB, f64 ppmValue)
{
  int mGRdB = GRdB;
  sdrplay_api_ErrT err;
  mpParent = parent;
  mpChosenDevice = chosenDevice;
  mFreq = startFreq;
  mLnaState = lnaState;

  connect(this, &Rsp_device::signal_set_lnabounds, parent, &SdrPlayHandler_v3::set_lnabounds);
  connect(this, &Rsp_device::signal_set_antennaSelect, parent, &SdrPlayHandler_v3::set_antennaSelect);

  err = parent->sdrplay_api_GetDeviceParams(chosenDevice->dev, &mpDeviceParams);

  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "sdrplay_api_GetDeviceParams failed %s\n", parent->sdrplay_api_GetErrorString(err));
    throw (21);
  }

  if (mpDeviceParams == nullptr)
  {
    fprintf(stderr, "sdrplay_api_GetDeviceParams return null as par\n");
    throw (22);
  }

  mpChParams = mpDeviceParams->rxChannelA;

  mpDeviceParams->devParams->ppm = ppmValue;
  mpDeviceParams->devParams->fsFreq.fsHz = 2048000;
  mpChParams->tunerParams.bwType = sdrplay_api_BW_1_536;
  mpChParams->tunerParams.ifType = sdrplay_api_IF_Zero;

  //      these will change:
  mpChParams->tunerParams.rfFreq.rfHz = (f32)startFreq;
  //
  //	It is known that all supported Rsp's can handle the values
  //	as given below. It is up to the particular Rsp to check
  //	correctness of the given lna and GRdB
  if (GRdB > 59)
  {
    mGRdB = 59;
  }
  else if (GRdB < 20)
  {
    mGRdB = 20;
  }
  mpChParams->tunerParams.gain.gRdB = mGRdB;
  mpChParams->tunerParams.gain.LNAstate = 3;

  mpChParams->ctrlParams.agc.setPoint_dBfs = -17;
  mpChParams->ctrlParams.agc.attack_ms = 500;
  mpChParams->ctrlParams.agc.decay_ms = 500;
  mpChParams->ctrlParams.agc.decay_delay_ms = 200;
  mpChParams->ctrlParams.agc.decay_threshold_dB = 3;
  if (agcMode)
  {
    mpChParams->ctrlParams.agc.enable = sdrplay_api_AGC_CTRL_EN;
  }
  else
  {
    mpChParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
  }
  err = parent->sdrplay_api_Init(chosenDevice->dev, &parent->cbFns, parent);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "sdrplay_api_Init failed %s\n", parent->sdrplay_api_GetErrorString(err));
    throw (23);
  }
}

Rsp_device::~Rsp_device()
{}

int Rsp_device::lnaStates(int frequency)
{
  (void)frequency;
  return 0;
}

bool Rsp_device::restart(int freq)
{
  (void)freq;
  return false;
}

bool Rsp_device::set_agc(int setPoint, bool on)
{
  sdrplay_api_ErrT err;

  if (on)
  {
    mpChParams->ctrlParams.agc.setPoint_dBfs = setPoint;
	mpChParams->ctrlParams.agc.enable = sdrplay_api_AGC_CTRL_EN;
  }
  else
  {
    mpChParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
  }

  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev, mpChosenDevice->tuner, sdrplay_api_Update_Ctrl_Agc, sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "agc: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }
  return true;
}

bool Rsp_device::set_lna(int lnaState)
{
  (void)lnaState;
  return false;
}

bool Rsp_device::set_GRdB(int GRdBValue)
{
  sdrplay_api_ErrT err;

  mpChParams->tunerParams.gain.gRdB = GRdBValue;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev, mpChosenDevice->tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "grdb: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }
  return true;
}

bool Rsp_device::set_ppm(f64 ppmValue)
{
  sdrplay_api_ErrT err;

  mpDeviceParams->devParams->ppm = ppmValue;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev, mpChosenDevice->tuner, sdrplay_api_Update_Dev_Ppm, sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "ppm: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }
  return true;
}

bool Rsp_device::set_antenna(int antenna)
{
  (void)antenna;
  return false;
}

bool Rsp_device::set_amPort(int amPort)
{
  (void)amPort;
  return false;
}

bool Rsp_device::set_biasT(bool b)
{
  (void)b;
  return false;
}

bool Rsp_device::set_notch(bool b)
{
  (void)b;
  return false;
}

