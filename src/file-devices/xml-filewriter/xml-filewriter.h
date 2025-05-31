#
/*
 *    Copyright (C) 2014 .. 2019
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB (formerly SDR-J, JSDR).
 *    Many of the ideas as implemented in Qt-DAB are derived from
 *    other work, made available through the GNU general Public License.
 *    All copyrights of the original authors are acknowledged.
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

#ifndef	__XML_FILEWRITER__
#define	__XML_FILEWRITER__

#include "glob_data_types.h"
#include <QString>
#include <stdint.h>
#include <cstdio>
#include <complex>
#include <QtXml>

class Blocks	{
public:
			Blocks		() {}
			~Blocks		() {}
	i32		blockNumber;
	i32		nrElements;
	QString		typeofUnit;
	i32		frequency;
	QString		modType;
};

class XmlFileWriter {
public:
		XmlFileWriter	(FILE *,
                    i32,
                    QString,
                    i32,
                    i32,
                    QString,
                    QString,
                    QString);
	                         
			~XmlFileWriter		();
	void		add			(std::complex<i16> *, i32);
	void		add			(std::complex<u8> *, i32);
	void		add			(std::complex<i8> *, i32);
	void		computeHeader		();
private:
	i32		nrBits;
	QString		container;
	i32		sampleRate;
	i32		frequency;
	QString		deviceName;
	QString		deviceModel;
	QString		recorderVersion;
	QString		create_xmltree		();
	FILE		*xmlFile;
	QString		byteOrder;
	i32		nrElements;
};

#endif
