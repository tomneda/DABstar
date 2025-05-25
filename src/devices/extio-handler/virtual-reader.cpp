#
/*
 *    Copyright (C) 2013 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
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
//
//	This is the - almost empty - default implementation
#include	"virtual-reader.h"

	virtualReader::virtualReader	(RingBuffer<cf32> *p,
	                                                       i32 rate) {
	theBuffer	= p;
	blockSize	= -1;
	setMapper (rate, 2048000);
}

	virtualReader::~virtualReader		(void) {
}

void	virtualReader::restartReader	(i32 s) {
	fprintf (stderr, "Restart met block %d\n", s);
	blockSize	= s;
}

void	virtualReader::stopReader	(void) {
}

void	virtualReader::processData	(f32 IQoffs, void *data, int cnt) {
	(void)IQoffs;
	(void)data;
	(void)cnt;
}

i16	virtualReader::bitDepth	(void) {
	return 12;
}

void	virtualReader::setMapper	(i32 inRate, i32 outRate) {
i32	i;

	this	-> inSize	= inRate / 1000;
	this	-> outSize	= outRate / 1000;
	inTable			= new cf32 [inSize];
	outTable		= new cf32 [outSize];
	mapTable		= new f32 [outSize];
	for (i = 0; i < outSize; i ++)
	   mapTable [i] = (f32) i * inRate / outRate;
	conv	= 0;
}

void	virtualReader::convertandStore (cf32 *s,
	                                             i32 amount) {
i32	i, j;

	for (i = 0; i < amount; i ++) {
	   inTable [conv++]	= s [i];
	   if (conv >= inSize) {	// full buffer, map
	      for (j = 0; j < outSize - 1; j ++) {
	         i16 base	= (int)(floor (mapTable [j]));
	         f32  frac	= mapTable [j] - base;
	         outTable [j]	=  inTable [base] * (1 - frac) +
	                         inTable [base + 1] * frac;
	      }
	      
//
//	let op, het laatste element was nog niet gebruikta
	      conv	= 1;
	      inTable [0] = inTable [inSize - 1];
	      theBuffer -> putDataIntoBuffer (outTable, outSize - 1);
	   }
	}
}

