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

#ifndef __ELAD_WORKER__
#define	__ELAD_WORKER__

#include	<stdlib.h>
#include	<cstdio>
#include	<math.h>
#include	<string.h>
#include	<unistd.h>
#include	<atomic>
#include	<stdint.h>
#include	<QThread>
#include	"dab-constants.h"
#include	"ringbuffer.h"

class	eladLoader;
class	eladHandler;

class	eladWorker: public QThread {
Q_OBJECT
public:
			eladWorker	(i32,	// selected frequency
	                                 eladLoader *,
	                                 eladHandler *,
	                                 RingBuffer<u8> *,
	                                 bool *);
			~eladWorker		();
	void		stop			();
private:
	void			run		();
	eladLoader		*functions;	// 
	RingBuffer<u8>	*_O_Buffer;
	long i32		eladFrequency;
	std::atomic<bool>	running;
};
#endif

