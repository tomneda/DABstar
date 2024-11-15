#include  "RspDx-handler.h"
#include  "sdrplay-handler-v3.h"

RspDx_handler::RspDx_handler(SdrPlayHandler_v3 *parent, sdrplay_api_DeviceT *chosenDevice, int freq, bool agcMode, int lnaState, int GRdB, bool biasT, bool notch, double ppmValue)
  : Rsp_device(parent, chosenDevice, freq, agcMode, lnaState, GRdB, ppmValue)
{
  int mLna_upperBound = lnaStates(freq) - 1;
  if (lnaState > mLna_upperBound)
    this->mLnaState = mLna_upperBound;

  emit signal_set_lnabounds(0, mLna_upperBound);
  emit signal_set_antennaSelect(2);

  set_lna(this->mLnaState);

  if (biasT)
    set_biasT(true);
  if (notch)
    set_notch(true);
}

int RspDx_handler::lnaStates(int freq)
{
  if (freq < MHz (60))
    return 19;
  if (freq < MHz (250))
    return 27;
  if (freq < MHz (420))
    return 28;
  if (freq < MHz (1000))
    return 21;
  return 19;
}

bool RspDx_handler::restart(int freq)
{
  sdrplay_api_ErrT err;

  mpChParams->tunerParams.rfFreq.rfHz = (float)freq;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev, mpChosenDevice->tuner, sdrplay_api_Update_Tuner_Frf, sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "restart: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }

  this->mFreq = freq;
  emit signal_set_lnabounds(0, lnaStates(freq) - 1);

  return true;
}

bool RspDx_handler::set_lna(int lnaState)
{
  sdrplay_api_ErrT err;

  mpChParams->tunerParams.gain.LNAstate = lnaState;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev, mpChosenDevice->tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "lna: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }
  this->mLnaState = lnaState;
  return true;
}

bool RspDx_handler::set_antenna(int antenna)
{
  sdrplay_api_ErrT err;

  mpDeviceParams->devParams->rspDxParams.antennaSel = antenna == 'A' ? sdrplay_api_RspDx_ANTENNA_A : antenna == 'B'
                                                                                                   ? sdrplay_api_RspDx_ANTENNA_B
                                                                                                   : sdrplay_api_RspDx_ANTENNA_C;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev,
                                     mpChosenDevice->tuner,
                                     sdrplay_api_Update_None,
                                     sdrplay_api_Update_RspDx_AntennaControl);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "antenna: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }

  return true;
}

bool RspDx_handler::set_amPort(int amPort)
{
  sdrplay_api_ErrT err;

  mpDeviceParams->devParams->rspDxParams.hdrEnable = amPort;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev, mpChosenDevice->tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_HdrEnable);
  if (err != sdrplay_api_Success)
  {
    return false;
  }

  return true;
}

bool RspDx_handler::set_biasT(bool biasT_value)
{
  sdrplay_api_DevParamsT * xxx;
  sdrplay_api_RspDxParamsT * rspDxParams;
  sdrplay_api_ErrT err;

  xxx = mpDeviceParams->devParams;
  rspDxParams = &(xxx->rspDxParams);
  rspDxParams->biasTEnable = biasT_value;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev, mpChosenDevice->tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_BiasTControl);

  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "setBiasT: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }

  return true;
}

bool RspDx_handler::set_notch(bool on)
{
  sdrplay_api_DevParamsT * xxx;
  sdrplay_api_RspDxParamsT * rspDxParams;
  sdrplay_api_ErrT err;

  xxx = mpDeviceParams->devParams;
  rspDxParams = &(xxx->rspDxParams);
  rspDxParams->rfNotchEnable = on;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev, mpChosenDevice->tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_RfNotchControl);

  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "notch: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }

  return true;
}
