#
/*
 *    Copyright (C) 2015 .. 2017
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
 *
 *	MOT handling is a crime, here we have a single class responsible
 *	for handling a MOT directory
 */
#include	"mot-dir.h"

	MotDirectory::MotDirectory (DabRadio *mr,
                              u16	transportId,
                              i16	segmentSize,
                              i32	dirSize,
                              i16	objects,
                              u8	*segment) {
i16	i;

	   this	-> myRadioInterface	= mr;
	   for (i = 0; i < 512; i ++)
	      marked [i] = false;
	   num_dirSegments	= -1;
	   this	-> transportId	= transportId;
	   this	-> dirSize	= dirSize;
	   this	-> numObjects	= objects;
	   this	-> dir_segmentSize	= segmentSize;
//	   fprintf (stdout, "transportId %d, dirSize %d, numObjects %d, segmentSize %d\n",
//	                             transportId, dirSize, objects, segmentSize);
	   dir_segments. resize (dirSize);
	   motComponents. resize (objects);
	   for (i = 0; i < objects; i ++) {
	      motComponents [i]. inUse = false;
	      motComponents [i]. motSlide	= nullptr;
	   }
	   memcpy (&dir_segments [0], segment, segmentSize);
	   marked [0] = true;
	   if (segmentSize >= dirSize)
	      analyse_theDirectory();
	   
}

	MotDirectory::~MotDirectory () {
int	i;

	for (i = 0; i < numObjects; i ++) 
	   if (motComponents [i]. inUse &&
	                   (motComponents [i]. motSlide != nullptr)) 
	      delete motComponents [i]. motSlide;
}

MotObject	*MotDirectory::getHandle (u16 transportId) {
int i;
	for (i = 0; i < numObjects; i ++)
	   if ((motComponents [i]. inUse) &&
	             (motComponents [i]. transportId == transportId))
	      return motComponents [i]. motSlide;

	return nullptr;
}

void	MotDirectory::setHandle (MotObject *h, u16 transportId) {
int	i;

	for (i = 0; i < numObjects; i ++)
	   if (!motComponents [i]. inUse) {
	      motComponents [i]. inUse		= true;
	      motComponents [i]. transportId	= transportId;
	      motComponents [i]. motSlide	= h;
	      return;
	   }
}
//
//	unfortunately, directory segments do not need to come in
//	in order
void	MotDirectory::directorySegment (u16 transportId,
                                      u8	*segment,
                                      i16	segmentNumber,
                                      i32	segmentSize,
                                      bool	lastSegment) {
i16	i;

//	fprintf (stdout, "adding dir segment %d\n", segmentNumber);
	if (this -> transportId != transportId)
	   return;
	if (this -> marked [segmentNumber])
	   return;
	if (lastSegment)
	   this -> num_dirSegments = segmentNumber + 1;
	this	-> marked [segmentNumber] = true;
	u8	*address = &dir_segments [segmentNumber * dir_segmentSize];
	memcpy (address, segment, segmentSize);
//
//	we are "complete" if we know the number of segments and
//	all segments are "in"
	if (this -> num_dirSegments != -1) {
	   for (i = 0; i < this -> num_dirSegments; i ++)
	      if (!this -> marked [i])
	         return;
//	   yes we have all data to build up the directory
	   analyse_theDirectory();
	}
}
//
//	This is the tough one, we collected the bits, and now
//	we need to extract the "motObject"s from it

void	MotDirectory::analyse_theDirectory () {
u32	currentBase	= 11;	// in bytes
//u8	*data			= dir_segments;
u16 extensionLength	= (dir_segments [currentBase] << 8) |
	                                         dir_segments [currentBase + 1];

	currentBase += 2 + extensionLength;
//	fprintf (stdout, "directory with transportId %d\n", transportId);
	for (int i = 0; i < numObjects; i ++) {
	   u16 transportId	= (dir_segments [currentBase] << 8) |
	                                    dir_segments [currentBase + 1];
	   if (transportId == 0)	// just a dummy
	      break;

//	   fprintf (stdout, "motObject with transportId %d and \n", transportId);
	   u8 *segment	= &dir_segments [currentBase + 2];
	   MotObject *handle	= new MotObject (myRadioInterface,
                                         true,
                                         transportId,
                                         segment,
                                         -1,
                                         false);

	   
	   currentBase		+= 2 + handle -> get_header_size();
	   setHandle (handle, transportId);
	}
}

u16	MotDirectory::get_transportId() {
	return transportId;
}

