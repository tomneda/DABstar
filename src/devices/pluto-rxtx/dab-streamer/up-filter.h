#
/*
 *    Copyright (C) 2019
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
#ifndef	__UP_FILTER_H
#define	__UP_FILTER_H

#include	<complex>
#include	<vector>
#include	<math.h>


class	upFilter {
	std::vector<cf32> kernel;
	std::vector<cf32> buffer;
	i32		ip;
	i32		order;
	i32		bufferSize;
	i32		multiplier;
public:
	upFilter	(i32, i32, i32);
	~upFilter	();
void	Filter	(cf32, cf32 *);
};
#endif

