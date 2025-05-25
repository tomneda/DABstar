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
#ifndef	__MOT_DIRECTORY__
#define	__MOT_DIRECTORY__

#include	"mot-object.h"
#include	<QString>
#include	<vector>
class	DabRadio;

class	MotDirectory {
public:
			MotDirectory	(DabRadio *,
                     u16,
                     i16,
                     i32,
                     i16,
                     u8 *);
			~MotDirectory();
	MotObject	*getHandle	(u16);
	void		setHandle	(MotObject *, u16);
	void		directorySegment (u16 transportId,
                                        u8 *segment,
                                        i16 segmentNumber,
                                        i32 segmentSize,
                                        bool    lastSegment);
	u16	get_transportId();
private:
	void		analyse_theDirectory();
	u16	transportId;

	DabRadio	*myRadioInterface;
	std::vector<u8>	dir_segments;
	bool		marked [512];
	i16		dir_segmentSize;
	i16		num_dirSegments;
	i16		dirSize;
	i16		numObjects;
	typedef struct {
	   bool		inUse;
	   u16	transportId;
	   MotObject	*motSlide;
	} motComponentType;
	std::vector<motComponentType>	motComponents;
};

#endif

