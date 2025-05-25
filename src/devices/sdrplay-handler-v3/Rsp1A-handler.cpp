#include  "Rsp1A-handler.h"
#include  "sdrplay-handler-v3.h"

Rsp1A_handler::Rsp1A_handler(SdrPlayHandler_v3 *parent, sdrplay_api_DeviceT *chosenDevice, int freq, bool agcMode, int lnaState, int GRdB, bool biasT, bool notch, f64 ppmValue)
  : Rsp_device(parent, chosenDevice, freq, agcMode, lnaState, GRdB, ppmValue)
{
  int mLna_upperBound = lnaStates(freq) - 1;
  if (lnaState > mLna_upperBound)
    this->mLnaState = mLna_upperBound;

  emit signal_set_lnabounds(0, mLna_upperBound);

  set_lna(this->mLnaState);

  if (biasT)
    set_biasT(true);
  if (notch)
    set_notch(true);
}

int Rsp1A_handler::lnaStates(int freq)
{
  if (freq < MHz (60))
    return 7;
  if (freq < MHz (1000))
    return 10;
  return 9;
}

bool Rsp1A_handler::restart(int freq)
{
  sdrplay_api_ErrT err;

  mpChParams->tunerParams.rfFreq.rfHz = (f32)freq;
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

bool Rsp1A_handler::set_lna(int lnaState)
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

bool Rsp1A_handler::set_biasT(bool biasT_value)
{
  sdrplay_api_Rsp1aTunerParamsT * rsp1aTunerParams;
  sdrplay_api_ErrT err;

  rsp1aTunerParams = &(mpChParams->rsp1aTunerParams);
  rsp1aTunerParams->biasTEnable = biasT_value;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev,
                                     mpChosenDevice->tuner,
                                     sdrplay_api_Update_Rsp1a_BiasTControl,
                                     sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "setBiasT: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }
  return true;
}

bool Rsp1A_handler::set_notch(bool on)
{
  sdrplay_api_ErrT err;

  mpDeviceParams->devParams->rsp1aParams.rfNotchEnable = on;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev,
                                     mpChosenDevice->tuner,
                                     sdrplay_api_Update_Rsp1a_RfNotchControl,
                                     sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "notch: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }
  return true;
}
