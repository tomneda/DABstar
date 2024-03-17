#include  "Rsp-device.h"

#include  <stdint.h>
#include  <sdrplay_api.h>
#include  "sdrplay-handler-v3.h"

Rsp_device::Rsp_device(SdrPlayHandler_v3 * parent, sdrplay_api_DeviceT * chosenDevice, int sampleRate, int startFreq, bool agcMode, int lnaState, int GRdB, bool biasT)
{
  sdrplay_api_ErrT err;
  mpParent = parent;
  mpChosenDevice = chosenDevice;
  mSampleRate = sampleRate;
  mFreq = startFreq;
  mAgcMode = agcMode;
  mLnaState = lnaState;
  mGRdB = GRdB;
  mBiasT = biasT;

  connect(this, &Rsp_device::signal_set_lnabounds, parent, &SdrPlayHandler_v3::set_lnabounds);
  connect(this, &Rsp_device::signal_set_deviceName, parent, &SdrPlayHandler_v3::set_deviceName);
  connect(this, &Rsp_device::signal_set_antennaSelect, parent, &SdrPlayHandler_v3::set_antennaSelect);
  connect(this, &Rsp_device::signal_show_lnaGain, parent, &SdrPlayHandler_v3::show_lnaGain);
  //connect(this, &Rsp_device::signal_set_nrBits, parent, &SdrPlayHandler_v3::set_nrBits); // it's to late to give data back to device caller

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

  mpDeviceParams->devParams->fsFreq.fsHz = sampleRate;
  mpChParams->tunerParams.bwType = sdrplay_api_BW_1_536;
  mpChParams->tunerParams.ifType = sdrplay_api_IF_Zero;

  //      these will change:
  mpChParams->tunerParams.rfFreq.rfHz = (float)startFreq;
  //
  //	It is known that all supported Rsp's can handle the values
  //	as given below. It is up to the particular Rsp to check
  //	correctness of the given lna and GRdB
  if (GRdB > 59)
  {
    mGRdB = 59;
  }
  if (GRdB < 20)
  {
    mGRdB = 20;
  }
  mpChParams->tunerParams.gain.gRdB = mGRdB;
  mpChParams->tunerParams.gain.LNAstate = 3;
  if (mAgcMode)
  {
    mpChParams->ctrlParams.agc.setPoint_dBfs = -30;
    mpChParams->ctrlParams.agc.enable = sdrplay_api_AGC_100HZ;
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
  (void)setPoint;
  (void)on;
  return false;
}

bool Rsp_device::set_lna(int lnaState)
{
  (void)lnaState;
  return false;
}

bool Rsp_device::set_GRdB(int GRdBValue)
{
  (void)GRdBValue;
  return false;
}

bool Rsp_device::set_ppm(int ppm)
{
  (void)ppm;
  return false;
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

