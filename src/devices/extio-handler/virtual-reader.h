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
//
//	For the different formats for input, we have
//	different readers, with one "mother" reader.
//	Note that the cardreader is quite different here
#ifndef	__VIRTUAL_READER__
#define	__VIRTUAL_READER__

#include	<stdint.h>
#include	<cstdio>
#include	"ringbuffer.h"
#include	"dab-constants.h"
//
//	The virtualReader is the mother of the readers.
//	The cardReader is slighty different, however
//	made fitting the framework
class	virtualReader {
protected:
RingBuffer<cf32>	*theBuffer;
i32	blockSize;
public:
		virtualReader	(RingBuffer<cf32> *p,
	                                                  i32 rate);
virtual		~virtualReader	(void);
virtual void	restartReader	(i32 s);
virtual void	stopReader	(void);
virtual void	processData	(f32 IQoffs, void *data, i32 cnt);
virtual	i16	bitDepth	(void);
protected:
	i32	base;
	void	convertandStore		(cf32 *, i32);
private:
	void	setMapper	(i32, i32);
	f32	*mapTable;
	i16	conv;
	i16	inSize;
	i16	outSize;
	cf32	*inTable;
	cf32	*outTable;
	
};

#endif

