#
/*
 *    Copyright (C) 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
 *
 *    Qt-Dab is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation version 2 of the License.
 *
 *    Qt-Dab is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-Dab if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include	"Rsp1-handler.h"
#include	"sdrplay-handler-v3.h"

Rsp1_handler::Rsp1_handler(SdrPlayHandler_v3 *parent, sdrplay_api_DeviceT *chosenDevice,
						   i32 freq, bool agcMode, i32 lnaState, i32 GRdB, f64 ppm)
  : Rsp_device(parent, chosenDevice, freq, agcMode, lnaState, GRdB, ppm)
{
  i32 mLna_upperBound = lnaStates(freq) -1;
  if (lnaState > mLna_upperBound)
    this->mLnaState = mLna_upperBound;

  emit signal_set_lnabounds(0, mLna_upperBound);

  set_lna(this->mLnaState);
}


i32 Rsp1_handler::lnaStates (i32 freq)
{
  (void)freq;
  return 4;
}

bool Rsp1_handler::restart(i32 freq)
{
  sdrplay_api_ErrT err;

  mpChParams->tunerParams.rfFreq.rfHz = (f32)freq;
  err = mpParent->sdrplay_api_Update(mpChosenDevice->dev,
                                   mpChosenDevice->tuner,
                                       sdrplay_api_Update_Tuner_Frf,
                                       sdrplay_api_Update_Ext1_None);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "restart: error %s\n", mpParent->sdrplay_api_GetErrorString(err));
    return false;
  }

  this->mFreq	= freq;
  emit signal_set_lnabounds(0, lnaStates(freq) - 1);

  return true;
}

bool Rsp1_handler::set_lna(i32 lnaState)
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
