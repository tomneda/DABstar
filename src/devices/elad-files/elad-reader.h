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
#ifndef	__ELAD_READER__
#define	__ELAD_READER__

#include	<QThread>
#include	<cstdio>
#include	"dab-constants.h"
#include	"ringbuffer.h"
#include	<atomic>
class	eladFiles;

class	eladReader:public QThread {
Q_OBJECT
public:
			eladReader	(eladFiles *,
	                                 FILE *,
	                                 RingBuffer<u8> *);
			~eladReader	();
	void		startReader	();
	void		stopReader	();
private:
virtual void		run		();
	FILE		*filePointer;
	RingBuffer<u8> *theBuffer;
	u64	period;
	std::atomic<bool>	running;
	i64		fileLength;
	eladFiles	*parent;
signals:
	void		setProgress	(i32);
};

#endif

