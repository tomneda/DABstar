#include  "RspII-handler.h"
#include  "sdrplay-handler-v3.h"

RspII_handler::RspII_handler(SdrPlayHandler_v3 * mpParent, sdrplay_api_DeviceT * chosenDevice, int sampleRate, int freq, bool agcMode, int lnaState, int GRdB, bool biasT)
  : Rsp_device(mpParent, chosenDevice, sampleRate, freq, agcMode, lnaState, GRdB, biasT)
{
  mLna_upperBound = 9;
  mDeviceModel = "RSP-II";
  mNrBits = 14;
  mAntennaSelect = true;
  emit signal_set_deviceName(mDeviceModel);
  emit signal_set_antennaSelect(1);
  emit signal_set_nrBits(mNrBits);
  if (freq < MHz (420))
  {
    mLna_upperBound = 9;
  }
  else
  {
    mLna_upperBound = 6;
  }
  if (lnaState > mLna_upperBound)
  {
    this->mLnaState = mLna_upperBound - 1;
  }
  set_lna(this->mLnaState);
  if (biasT)
  {
    set_biasT(true);
  }
}

RspII_handler::~RspII_handler()
{}


static const int RSPII_Table[3][10] = {{ 9, 0, 10, 15, 21, 24, 34, 39, 45, 64 },
                                       { 6, 0, 7,  10, 17, 22, 41, -1, -1, -1 },
                                       { 6, 0, 5,  21, 15, 15, 32, -1, -1, -1 }};

int16_t RspII_handler::bankFor_rspII(int freq)
{
  if (freq < MHz (420))
  {
    return 0;
  }
  if (freq < MHz (1000))
  {
    return 1;
  }
  return 2;
}

int RspII_handler::lnaStates(int frequency)
{
  int band = bankFor_rspII(frequency);
  return RSPII_Table[band][0];
}

int RspII_handler::get_lnaGain(int lnaState, int freq)
{
  int band = bankFor_rspII(freq);
  return RSPII_Table[band][lnaState + 1];
}

bool RspII_handler::restart(int freq)
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
  if (freq < MHz (420))
  {
    this->mLna_upperBound = 9;
  }
  else
  {
    this->mLna_upperBound = 6;
  }

  emit signal_set_lnabounds(0, mLna_upperBound);
  emit signal_show_lnaGain(get_lnaGain(mLnaState, freq));

  return true;
}

bool RspII_handler::set_agc(int setPoint, bool on)
{
  sdrplay_api_ErrT err;

  if (on)
  {
    mpChParams->ctrlParams.agc.setPoint_dBfs = -setPoint;
    mpChParams->ctrlParams.agc.enable = sdrplay_api_AGC_100HZ;
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

  this->mAgcMode = on;
  return true;
}

bool RspII_handler::set_GRdB(int GRdBValue)
{
  sdrplay_api_ErrT err;

  mpChParams->tunerParams.gain.gRdB = GRdBValue;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev, mpChosenDevice->tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "grdb: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }
  this->mGRdB = GRdBValue;
  return true;
}

bool RspII_handler::set_ppm(int ppmValue)
{
  sdrplay_api_ErrT err;

  mpDeviceParams->devParams->ppm = ppmValue;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev, mpChosenDevice->tuner, sdrplay_api_Update_Dev_Ppm, sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "lna: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }
  return true;
}

bool RspII_handler::set_lna(int lnaState)
{
  sdrplay_api_ErrT err;

  mpChParams->tunerParams.gain.LNAstate = lnaState;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev, mpChosenDevice->tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "grdb: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }
  this->mLnaState = lnaState;
  emit signal_show_lnaGain(get_lnaGain(lnaState, mFreq));
  return true;
}

bool RspII_handler::set_antenna(int antenna)
{
  sdrplay_api_Rsp2TunerParamsT * rsp2TunerParams;
  sdrplay_api_ErrT err;

  rsp2TunerParams = &(mpChParams->rsp2TunerParams);
  rsp2TunerParams->antennaSel = antenna == 'A' ? sdrplay_api_Rsp2_ANTENNA_A : sdrplay_api_Rsp2_ANTENNA_B;

  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev,
                                   mpChosenDevice->tuner,
                                   sdrplay_api_Update_Rsp2_AntennaControl,
                                   sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    return false;
  }

  return true;
}

bool RspII_handler::set_biasT(bool biasT_value)
{
  sdrplay_api_Rsp2TunerParamsT * rsp2TunerParams;
  sdrplay_api_ErrT err;

  rsp2TunerParams = &(mpChParams->rsp2TunerParams);
  rsp2TunerParams->biasTEnable = biasT_value;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev,
                                   mpChosenDevice->tuner,
                                   sdrplay_api_Update_Rsp2_BiasTControl,
                                   sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    return false;
  }

  return true;
}

