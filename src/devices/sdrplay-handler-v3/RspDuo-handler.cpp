#include  "RspDuo-handler.h"
#include  "sdrplay-handler-v3.h"

RspDuo_handler::RspDuo_handler(SdrPlayHandler_v3 * mpParent, sdrplay_api_DeviceT * chosenDevice, int sampleRate, int freq, bool agcMode, int lnaState, int GRdB, bool biasT)
  : Rsp_device(mpParent, chosenDevice, sampleRate, freq, agcMode, lnaState, GRdB, biasT)
{
  mLna_upperBound = 10;
  mDeviceModel = "RSP-Duo";
  mNrBits = 14;
  mAntennaSelect = true;

  emit signal_set_deviceName(mDeviceModel);
  emit signal_set_antennaSelect(1);
  emit signal_set_lnabounds(0, mLna_upperBound);
  emit signal_set_nrBits(mNrBits);
  emit signal_show_lnaGain(get_lnaGain(lnaState, freq));

  if (biasT)
  {
    RspDuo_handler::set_biasT(true);
  }
}

static const int RSPDuo_Table[4][11] = {{ 7,  0, 6, 12, 18, 37, 42, 61, -1, -1, -1 },
                                        { 10, 0, 6, 12, 18, 20, 26, 32, 38, 57, 62 },
                                        { 10, 0, 7, 13, 19, 20, 27, 33, 39, 45, 64 },
                                        { 9,  0, 6, 12, 20, 26, 32, 38, 43, 62, -1 }};

int16_t RspDuo_handler::bankFor_rspDuo(int freq)
{
  if (freq < MHz (60))
  {
    return 0;
  }
  if (freq < MHz (420))
  {
    return 1;
  }
  if (freq < MHz (1000))
  {
    return 2;
  }
  return 3;
}

int RspDuo_handler::lnaStates(int frequency)
{
  int band = bankFor_rspDuo(frequency);
  return RSPDuo_Table[band][0];
}

int RspDuo_handler::get_lnaGain(int lnaState, int freq)
{
  int band = bankFor_rspDuo(freq);
  return RSPDuo_Table[band][lnaState + 1];
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
  if (freq < MHz (60))
  {
    this->mLna_upperBound = RSPDUO_NUM_LNA_STATES_AM;
  }
  else if (freq < MHz (1000))
  {
    this->mLna_upperBound = RSPDUO_NUM_LNA_STATES;
  }
  else
  {
    this->mLna_upperBound = RSPDUO_NUM_LNA_STATES_LBAND;
  }
  emit signal_set_lnabounds(0, mLna_upperBound);
  emit signal_show_lnaGain(get_lnaGain(mLnaState, freq));
  return true;
}

bool RspDuo_handler::set_agc(int setPoint, bool on)
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

bool RspDuo_handler::set_GRdB(int GRdBValue)
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

bool RspDuo_handler::set_ppm(int ppmValue)
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

bool RspDuo_handler::set_lna(int lnaState)
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

bool RspDuo_handler::set_antenna(int antenna)
{
  sdrplay_api_RspDuoTunerParamsT * rspDuoTunerParams;
  sdrplay_api_ErrT err;

  rspDuoTunerParams = &(mpChParams->rspDuoTunerParams);
  rspDuoTunerParams->tuner1AmPortSel = antenna == 'A' ? sdrplay_api_RspDuo_AMPORT_1 : sdrplay_api_RspDuo_AMPORT_2;


  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev,
                                   mpChosenDevice->tuner,
                                   sdrplay_api_Update_RspDuo_AmPortSelect,
                                   sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
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

