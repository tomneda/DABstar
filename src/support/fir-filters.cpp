#
/*
 *    Copyright (C) 2010, 2011, 2012
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
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

#include	"fir-filters.h"

//	FIR LowPass

	LowPassFIR::LowPassFIR (i16 firsize,
	                        i32 Fc, i32 fs){
f32	sum	= 0.0;
auto * const temp = make_vla(f32, firsize);

	this -> frequency	= (f32)Fc / fs;
	this -> filterSize	= firsize;
	this -> ip		= 0;
	filterKernel.	resize (filterSize);
	Buffer.		resize (filterSize);

	for (i32 i = 0; i < filterSize; i ++) {
	   filterKernel [i]	= 0;
	   Buffer [i]		= cf32 (0, 0);
	}

	for (i32 i = 0; i < filterSize; i ++) {
	   if (i == filterSize / 2)
	      temp [i] = 2 * M_PI * frequency;
	   else 
	      temp [i] =
	         sin (2 * M_PI * frequency * (i - filterSize/2))/ (i - filterSize/2);
//
//	Blackman window
	   temp [i]  *= (0.42 -
		    0.5 * cos (2 * M_PI * (f32)i / filterSize) +
		    0.08 * cos (4 * M_PI * (f32)i / filterSize));

	   sum += temp [i];
	}

	for (i32 i = 0; i < filterSize; i ++)
	   filterKernel [i] = temp [i] / sum;
}

	LowPassFIR::~LowPassFIR () {
}

i32	LowPassFIR::theSize	() {
	return Buffer. size ();
}

void	LowPassFIR::resize (i32 newSize) {
f32	*temp 	= (f32 *)alloca (newSize * sizeof (f32));
f32	sum = 0;

	filterSize	= newSize;
	filterKernel. resize (filterSize);
	Buffer. resize (filterSize);
	ip		= 0;

	for (i32 i = 0; i < filterSize; i ++) {
	   filterKernel [i]	= 0;
	}

	for (i32 i = 0; i < filterSize; i ++) {
	   if (i == filterSize / 2)
	      temp [i] = 2 * M_PI * frequency;
	   else 
	      temp [i] =
	         sin (2 * M_PI * frequency * (i - filterSize/2))/ (i - filterSize/2);
//
//	Blackman window
	   temp [i]  *= (0.42 -
		    0.5 * cos (2 * M_PI * (f32)i / filterSize) +
		    0.08 * cos (4 * M_PI * (f32)i / filterSize));

	   sum += temp [i];
	}

	for (i32 i = 0; i < filterSize; i ++)
	   filterKernel [i] = temp [i] / sum;
}

//	we process the samples backwards rather than reversing
//	the kernel
cf32	LowPassFIR::Pass (cf32 z) {
i16	i;
cf32	tmp	= 0;

	Buffer [ip]	= z;
	for (i = 0; i < filterSize; i ++) {
	   i16 index = ip - i;
	   if (index < 0)
	      index += filterSize;
	   tmp		+= Buffer [index] * filterKernel [i];
	}

	ip = (ip + 1) % filterSize;
	return tmp;
}

f32	LowPassFIR::Pass (f32 v) {
	return real (Pass (cf32 (v, 0)));
}


