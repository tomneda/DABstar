#
/*
 *    Copyright (C) 2020
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
#include	<cstdio>
#include	<unistd.h>
#include	<cstdlib>
#include	<fcntl.h>
#include	<sys/time.h>
#include	<ctime>
#include	<QString>
#include	"elad-files.h"

#define	__BUFFERSIZE__	32 * 32768

	eladFiles::eladFiles (QString f):
	                           myFrame (nullptr),
	                           _I_Buffer (4 * __BUFFERSIZE__),
	                           _O_Buffer (__BUFFERSIZE__) {
SF_INFO *sf_info;

	fileName	= f;
	setupUi (&myFrame);
  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
	myFrame. show	();

	filePointer	= fopen (f. toUtf8(). data(), "r+b");
	if (filePointer == nullptr) {
	   fprintf (stderr, "file %s no legitimate sound file\n", 
	                                f. toUtf8().data());
	   throw (24);
	}

        for (int i = 0; i < DAB_RATE / 1000; i ++) {
           f32 inVal  = f32 (ELAD_RATE / 1000);
           mapTable_int [i] =  int (floor (i * (inVal / 2048.0)));
           mapTable_float [i] = i * (inVal / 2048.0) - mapTable_int [i];
        }
        convIndex       = 0;
        fprintf (stderr, "mapTables initialized\n");

        iqSize          = 8;
	iqSwitch. store (false);
	nameofFile	-> setText (f);
	fileProgress	-> setValue (0);
	connect (iqSwitch_button, SIGNAL (clicked ()),
	         this, SLOT (handle_iqButton ()));
	running. store (false);
}
//
//	Note that running == true <==> readerTask has value assigned

	eladFiles::~eladFiles() {
	if (running. load()) {
	   readerTask	-> stopReader();
	   while (readerTask -> isRunning())
	      usleep (500);
	   delete readerTask;
	}
	if (filePointer != nullptr)
	   fclose (filePointer);
}

bool	eladFiles::restartReader	(i32 freq) {
	(void)freq;
	if (running. load())
           return true;
        readerTask      = new eladReader (this, filePointer, &_I_Buffer);
        running. store (true);
        return true;
}

void	eladFiles::stopReader() {
	if (running. load()) {
           readerTask   -> stopReader();
           while (readerTask -> isRunning())
              usleep (100);
	   delete readerTask;
        }
        running. store (false);
}

//cf32	makeSample_31bits (u8 *, bool);

typedef union {
	struct __attribute__((__packed__)) {
		f32	i;
		f32	q;
		} iqf;
	struct __attribute__((__packed__)) {
		i32	i;
		i32	q;
		} iq;
	struct __attribute__((__packed__)) {
		u8		i1;
		u8		i2;
		u8		i3;
		u8		i4;
		u8		q1;
		u8		q2;
		u8		q3;
		u8		q4;
		};
} iq_sample;

// ADC output unsigned 14 bit input to FPGA output signed 32 bit
// multiply output FPGA by SCALE_FACTOR to normalize 32bit signed values to ADC range signed 14 bit
#define SCALE_FACTOR_32to14    (0.000003814) //(8192/2147483648)
// ADC out unsigned 14 bit input to FPGA output signed 16 bit
#define SCALE_FACTOR_16to14    (0.250)       //(8192/32768)  

cf32	makeSample (u8 *buf, bool iqSwitch) {
//cf32	makeSample_31bits (u8 *buf, bool iqSwitch) {
int ii = 0; int qq = 0;
i16	i = 0;
u32	uii = 0, uqq = 0;

	u8 i0 = buf [i++];
	u8 i1 = buf [i++];
	u8 i2 = buf [i++];
	u8 i3 = buf [i++];

	u8 q0 = buf [i++];
	u8 q1 = buf [i++];
	u8 q2 = buf [i++];
	u8 q3 = buf [i++];


	uii = (i3 << 24) | (i2 << 16) | (i1 << 8) | i0;
	uqq = (q3 << 24) | (q2 << 16) | (q1 << 8) | q0;

	ii	= (int) uii;
	qq	= (int) uqq;
	if (iqSwitch)
	   return cf32 ((f32)qq * SCALE_FACTOR_32to14,
	                               (f32)ii * SCALE_FACTOR_32to14);
	else
	   return cf32 ((f32)ii * SCALE_FACTOR_32to14,
	                               (f32)qq * SCALE_FACTOR_32to14);
}

#define	SEGMENT_SIZE	(1024 * 8)
u8	lbuffer [SEGMENT_SIZE];
//	size is in I/Q pairs
//	Note: Samples computes the amount of samples that either
//	are already available or can be computed based on the
//	current content of the _I_Buffer
i32	eladFiles::getSamples	(cf32 *V, i32 size) {
i32	amount;
cf32 temp [2048];

	if (filePointer == nullptr)
	   return 0;

	while (Samples () < size)	// should not happen
	   usleep (500);

	while (_O_Buffer. GetRingBufferReadAvailable () < size) {
	   _I_Buffer. getDataFromBuffer (lbuffer, SEGMENT_SIZE);
	   for (int i = 0; i < SEGMENT_SIZE / iqSize; i ++) {
	      if (convIndex > ELAD_RATE / 1000) {
	         f32 sum = 0;
	         i16 j;
	         for (j = 0; j < 2048; j ++) {
	            i16  inpBase		= mapTable_int [j];
	            f32    inpRatio		= mapTable_float [j];
	            temp [j]  = convBuffer [inpBase + 1] * inpRatio +
	                        convBuffer [inpBase] * (1 - inpRatio);
	            sum += abs (temp [j]);
	         }
	
	         _O_Buffer. putDataIntoBuffer (temp, 2048);
//	shift the sample at the end to the beginning, it is needed
//	as the starting sample for the next time
	         convBuffer [0] = convBuffer [ELAD_RATE / 1000];
	         convIndex = 1;
              }
	   }
	}
	return _O_Buffer. getDataFromBuffer (V, size);
}

i32	eladFiles::Samples	(void) {
i64	bufferContent	= _I_Buffer. GetRingBufferReadAvailable ();
	return _O_Buffer. GetRingBufferReadAvailable () +
	       (int)(((i64)2048 * bufferContent / (i64)3072) / iqSize);
}

void    eladFiles::setProgress (int progress) {
	fileProgress	-> setValue (progress);
}

void	eladFiles::show		() {
	myFrame. show ();
}

void	eladFiles::hide		() {
	myFrame. hide	();
}

bool	eladFiles::isHidden	() {
	return myFrame. isHidden ();
}

void	eladFiles::handle_iqButton	() {
	iqSwitch. store (!iqSwitch. load ());
	if (iqSwitch)
	   iqSwitch_button -> setText ("Q/I");
	else
	   iqSwitch_button -> setText ("I/Q");
}

