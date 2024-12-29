#include  "RspDuo-handler.h"
#include  "sdrplay-handler-v3.h"

RspDuo_handler::RspDuo_handler(SdrPlayHandler_v3 *mpParent, sdrplay_api_DeviceT *chosenDevice, int freq, bool agcMode, int lnaState, int GRdB, bool biasT, bool notch, double ppmValue)
  : Rsp_device(mpParent, chosenDevice, freq, agcMode, lnaState, GRdB, ppmValue)
{
  int mLna_upperBound = lnaStates(freq) - 1;
  if (lnaState > mLna_upperBound)
    this->mLnaState = mLna_upperBound;

  emit signal_set_antennaSelect(1);
  emit signal_set_lnabounds(0, mLna_upperBound);

  set_lna(this->mLnaState);

  if (biasT)
    set_biasT(true);
  if (notch)
    set_notch(true);
}

int RspDuo_handler::lnaStates(int freq)
{
  if (freq < MHz (60))
    return 7;
  if (freq < MHz (1000))
    return 10;
  return 9;
}

bool RspDuo_handler::restart(int freq)
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

bool RspDuo_handler::set_lna(int lnaState)
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

bool RspDuo_handler::set_amPort(int amPort)
{
  sdrplay_api_RspDuoTunerParamsT * rspDuoTunerParams;
  sdrplay_api_ErrT err;

  rspDuoTunerParams = &(mpChParams->rspDuoTunerParams);
  rspDuoTunerParams->tuner1AmPortSel = amPort == 1 ? sdrplay_api_RspDuo_AMPORT_1 : sdrplay_api_RspDuo_AMPORT_2;

  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev,
                                     mpChosenDevice->tuner,
                                     sdrplay_api_Update_RspDuo_AmPortSelect,
                                     sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "amPort: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }

  return true;
}

bool RspDuo_handler::set_antenna(int antenna)
{
  sdrplay_api_TunerSelectT tuner = antenna == 'A' ? sdrplay_api_Tuner_A : sdrplay_api_Tuner_B;

  if (tuner == mpChosenDevice->tuner)
    return true;

  sdrplay_api_ErrT err = mpParent->sdrplay_api_SwapRspDuoActiveTuner(mpChosenDevice->dev, &mpChosenDevice->tuner, sdrplay_api_RspDuo_AMPORT_1);
  if (err != sdrplay_api_Success)
  {
	fprintf (stderr, "Swapping tuner: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }
  return true;
}

bool RspDuo_handler::set_biasT(bool biasT_value)
{
  sdrplay_api_RspDuoTunerParamsT * rspDuoTunerParams;
  sdrplay_api_ErrT err;

  rspDuoTunerParams = &(mpChParams->rspDuoTunerParams);
  rspDuoTunerParams->biasTEnable = biasT_value;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev,
                                     mpChosenDevice->tuner,
                                     sdrplay_api_Update_RspDuo_BiasTControl,
                                     sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "setBiasT: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }
  return true;
}

bool RspDuo_handler::set_notch(bool on)
{
  sdrplay_api_RspDuoTunerParamsT * rspDuoTunerParams;
  sdrplay_api_ErrT err;

  rspDuoTunerParams = &(mpChParams->rspDuoTunerParams);
  rspDuoTunerParams->rfNotchEnable = on;
  rspDuoTunerParams->tuner1AmNotchEnable = on;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev, mpChosenDevice->tuner,
        (sdrplay_api_ReasonForUpdateT)(sdrplay_api_Update_RspDuo_RfNotchControl | sdrplay_api_Update_RspDuo_Tuner1AmNotchControl),
        sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "notch: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }
  return true;
}

