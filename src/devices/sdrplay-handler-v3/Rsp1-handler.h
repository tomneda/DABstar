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
#ifndef  RSP1_HANDLER_H
#define  RSP1_HANDLER_H

#include	"Rsp-device.h"

class	sdrplayHandler_v3;

class	Rsp1_handler: public Rsp_device
{
public:
	Rsp1_handler(SdrPlayHandler_v3 *parent, sdrplay_api_DeviceT *chosenDevice,
	             int freq, bool agcMode, int lnaState, int GRdB, double ppm);
	~Rsp1_handler() override = default;

	int	lnaStates(int frequency) override;
	bool restart(int freq) override;
	bool set_lna(int lnaState) override;
};

#endif


