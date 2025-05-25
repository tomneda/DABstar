#
/*
 *    Copyright (C) 2014
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
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

#ifndef __ELAD_HANDLER__
#define	__ELAD_HANDLER__

#include	<QObject>
#include	<QFrame>
#include	<QFileDialog>
#include	<atomic>
#include	"dab-constants.h"
#include	"device-handler.h"
#include	"ringbuffer.h"
#include	"ui_elad-widget.h"
#include	<libusb-1.0/libusb.h>

#define	ELAD_RATE	3072000

class	QSettings;
class	eladWorker;
class	eladLoader;
//typedef	cf32(*makeSampleP)(u8 *);

class	eladHandler: public deviceHandler, public Ui_eladWidget {
Q_OBJECT
public:
		eladHandler		(QSettings *);
		~eladHandler		();
	i32	getVFOFrequency		();
	bool	legalFrequency		(i32);
	i32	defaultFrequency	();

	bool	restartReader		(i32);
	void	stopReader		();
	i32	getSamples		(cf32 *, i32);
	i32	Samples			();
	void	resetBuffer		();
	i32	getRate			();
	i16	bitDepth		();
private	slots:
	void	setGainReduction	();
	void	setFilter		();
	void	set_Offset		(int);
	void	set_NyquistWidth	(int);
	void	toggle_IQSwitch		();
	void	show_iqSwitch		(bool);
private:
	QSettings	*eladSettings;
	QFrame		myFrame;
	RingBuffer<u8>	_I_Buffer;
	RingBuffer<cf32>	_O_Buffer;
	bool		deviceOK;
	eladLoader	*theLoader;
	eladWorker	*theWorker;
	i32		externalFrequency;
	i32		eladFrequency;
	int		gainReduced;
	int		localFilter;
	int		Offset;
	int		Nyquist;
	std::atomic<bool> iqSwitch;
	int		iqSize;
	cf32 convBuffer	[ELAD_RATE / 1000 + 1];
	int		mapTable_int	[INPUT_RATE / 1000];
        f32		mapTable_float	[INPUT_RATE / 1000];
	int		convIndex;
};
#endif

