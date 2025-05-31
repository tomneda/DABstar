#
/*
 *    Copyright (C) 2013 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
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
#ifndef	__ELAD_FILES__
#define	__ELAD_FILES__

#include	<QString>
#include	<QFrame>
#include	<sndfile.h>
#include	<atomic>
#include	"dab-constants.h"
#include	"device-handler.h"
#include	"ringbuffer.h"

#include	"ui_elad-files.h"
#include	"elad-reader.h"

#define	ELAD_RATE	3072000
#define	DAB_RATE	2048000
class	eladFiles: public deviceHandler, public Ui_eladreaderWidget {
Q_OBJECT
public:
			eladFiles	(QString);
	       		~eladFiles	();
	i32		getSamples	(cf32 *, i32);
	i32		Samples		();
	bool		restartReader	(i32);
	void		stopReader	();
	void		show		();	
	void		hide		();
	bool		isHidden	();
private:
	QFrame		myFrame;
	QString		fileName;
	RingBuffer<u8>	_I_Buffer;
	RingBuffer<cf32> _O_Buffer;
	i32		bufferSize;
	FILE		*filePointer;
	eladReader	*readerTask;
	std::atomic<bool>	running;

	i32             iqSize;
        cf32 convBuffer  [ELAD_RATE / 1000 + 1];
        i32             mapTable_int    [DAB_RATE / 1000];
        f32           mapTable_float  [DAB_RATE / 1000];
        i32             convIndex;
	std::atomic<bool> iqSwitch;

public slots:
	void		setProgress	(i32);
	void		handle_iqButton	();
};

#endif

