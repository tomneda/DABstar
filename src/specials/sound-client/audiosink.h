#
/*
 *    Copyright (C)  2009, 2010, 2011
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB and friends
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __AUDIO_SINK__
#define	__AUDIO_SINK__
#include	"sound-constants.h"
#include	<QString>
#include	<portaudio.h>
#include	<cstdio>
#include	<QComboBox>
#include	"ringbuffer.h"

#define		LOWLATENCY	0100
#define		HIGHLATENCY	0200
#define		VERYHIGHLATENCY	0300

class	audioSink  {
public:
			audioSink		(int32_t);
			~audioSink		(void);
	int16_t		numberofDevices		(void);
	QString		outputChannelwithRate	(int16_t, int32_t);
	void		stop			(void);
	void		restart			(void);
	int32_t		putSample		(cmplx);
	int32_t		putSamples		(cmplx *, int32_t);
	int16_t		invalidDevice		(void);
	bool		isValidDevice		(int16_t);

	bool		selectDefaultDevice	(void);
	bool		selectDevice		(int16_t);
private:
	bool		OutputrateIsSupported	(int16_t, int32_t);
	int32_t		CardRate;
	bool		portAudio;
	bool		writerRunning;
	int16_t		numofDevices;
	int		paCallbackReturn;
	int16_t		bufSize;
	PaStream	*ostream;
	RingBuffer<float>	*_O_Buffer;
	PaStreamParameters	outputParameters;
protected:
static	int		paCallback_o	(const void	*input,
	                                 void		*output,
	                                 unsigned long	framesperBuffer,
	                                 const PaStreamCallbackTimeInfo *timeInfo,
					 PaStreamCallbackFlags statusFlags,
	                                 void		*userData);
};

#endif

