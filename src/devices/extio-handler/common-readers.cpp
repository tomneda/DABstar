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
//	Note that the cardreader is quite different
//	and coded elsewhere
//
#include	"common-readers.h"
//
//	The reader for 16 bit i32 values
//
	reader_16::reader_16 (RingBuffer<cf32> *p,
	                      i32 base_16,
	                      i32 rate):virtualReader (p, rate) {
	this	-> base = base_16;
}

	reader_16::~reader_16 (void) {
}
//
//	apparently bytes are read in from low byte to high byte
void	reader_16::processData	(f32 IQoffs, void *data, i32 cnt) {
i32	i;
	auto * const IQData = make_vla (cf32, blockSize);
u8	*p	= (u8 *)data;
	(void)IQoffs;
	(void)cnt;

	for (i = 0; i < blockSize; i ++) {
	   u8 r0	= p [4 * i];
	   u8 r1	= p [4 * i + 1];
	   u8 i0	= p [4 * i + 2];
	   u8 i1	= p [4 * i + 3];
	   i16 re	= (r1 << 8) | r0;
	   i16 im	= (i1 << 8) | i0;
	   IQData [i]	= cf32 ((f32)re / base,
	                                       (f32)im / base);
	}
	
	convertandStore (IQData, blockSize);
}

i16 reader_16::bitDepth	(void) {
	return 16;
}
//
//	The reader for 24 bit integer values
//
	reader_24::reader_24 (RingBuffer<cf32> *p,
	                      i32 base_24, i32 rate):
	                                       virtualReader (p, rate) {
	this	-> base	= base_24;
}

	reader_24::~reader_24 (void) {
}

void	reader_24::processData	(f32 IQoffs, void *data, i32 cnt) {
i32	i;
	auto * const IQData = make_vla (cf32, blockSize);
u8	*p	= (u8 *)data;
	(void)IQoffs;
	(void)cnt;

	for (i = 0; i < blockSize; i ++) {
	   u8 r0	= p [6 * i];
	   u8 r1	= p [6 * i + 1];
	   u8 r2	= p [6 * i + 2];
	   u8 i0	= p [6 * i + 3];
	   u8 i1	= p [6 * i + 4];
	   u8 i2	= p [6 * i + 5];
	   i32 re	= i32 (u32 (r2 << 16 | r1 << 8 | r0));
	   i32 im	= i32 (u32 (i2 << 16 | i1 << 8 | i0));
	   IQData [i]	= cf32 ((f32)re / base,
	                                       (f32)im / base);
	}
	
	convertandStore (IQData, blockSize);
}

i16 reader_24::bitDepth	(void) {
	return 24;
}
//
//	The reader for 32 bit integer values
//
	reader_32::reader_32 (RingBuffer<cf32> *p,
	                      i32 base_32, i32 rate):
	                                         virtualReader (p, rate) {
	this	-> base = base_32;
}

	reader_32::~reader_32 (void) {
}

void	reader_32::processData	(f32 IQoffs, void *data, i32 cnt) {
i32	i;
	auto * const IQData = make_vla (cf32, blockSize);
u8	*p	= (u8 *)data;
	(void)IQoffs;
	(void)cnt;

	for (i = 0; i < blockSize; i ++) {
	   u8 r0	= p [8 * i];
	   u8 r1	= p [8 * i + 1];
	   u8 r2	= p [8 * i + 2];
	   u8 r3	= p [8 * i + 3];
	   u8 i0	= p [8 * i + 4];
	   u8 i1	= p [8 * i + 5];
	   u8 i2	= p [8 * i + 6];
	   u8 i3	= p [8 * i + 7];
	   i32 re	= i32 (u32 (r3 << 24 | r2 << 16 |
	                                             r1 << 8 | r0));
	   i32 im	= i32 (u32 (i3 << 24 | i2 << 16 |
	                                             i1 << 8 | i0));
	   IQData [i]	= cf32 ((f32)re / base,
	                                       (f32)im / base);
	}
	
	convertandStore (IQData, blockSize);
}

i16	reader_32::bitDepth	(void) {
	return 32;
}
//
//	The reader for 32 bit f32 values
//
	reader_float::reader_float (RingBuffer<cf32> *p,
	                                                     i32 rate):
	                                             virtualReader (p, rate) {
i16	i;
}

	reader_float::~reader_float (void) {
}
//
void	reader_float::processData	(f32 IQoffs, void *data, i32 cnt) {
i32	i, j;
	auto * const IQData = make_vla (cf32, blockSize);
f32	*p	= (f32 *)data;
	(void)IQoffs;
	(void)cnt;

	for (i = 0; i < blockSize; i ++) 
	   IQData [i]	= cf32 (p [2 * i], p [2 * i + 1]);

	convertandStore (IQData, blockSize);
}

i16 reader_float::bitDepth	(void) {
	return 24;
}
