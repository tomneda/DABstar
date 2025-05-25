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
#ifndef	__WAV_READER__
#define	__WAV_READER__

#include	<QThread>
#include	<sndfile.h>
#include	"dab-constants.h"
#include	"ringbuffer.h"
#include	<atomic>

class	WavFileHandler;

class	WavReader: public QThread {
Q_OBJECT
public:
	WavReader(WavFileHandler *, SNDFILE *, RingBuffer<cf32> *);
	~WavReader();
	void startReader();
	void stopReader();
	bool handle_continuousButton();

private:
	virtual void run();
	SNDFILE	*filePointer;
	RingBuffer<cf32> *theBuffer;
	u64 period;
	std::atomic<bool> running;
	std::atomic<bool> continuous;
	WavFileHandler *parent;
	i64	fileLength;
signals:
	void setProgress(int, f32);
};

#endif

