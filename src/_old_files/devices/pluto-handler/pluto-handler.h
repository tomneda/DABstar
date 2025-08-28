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

#ifndef __PLUTO_HANDLER__
#define	__PLUTO_HANDLER__

#include	<QtNetwork>
#include        <QMessageBox>
#include        <QByteArray>
#include	<QThread>
#include	<QFrame>
#include	<QSettings>
#include	<atomic>
#include	<iio.h>
#include	"dab-constants.h"
#include	"ringbuffer.h"
#include	"device-handler.h"
#include	"ui_pluto-widget.h"

class	xml_fileWriter;

#ifndef	PLUTO_RATE
#define	PLUTO_RATE	2100000
#define	DAB_RATE	2048000
#define	DIVIDER		1000
#define	CONV_SIZE	(PLUTO_RATE / DIVIDER)
#endif


struct stream_cfg {
        i64 bw_hz;
        i64 fs_hz;
        i64 lo_hz;
        const char *rfport;
};

class	plutoHandler: public QThread, public deviceHandler, public Ui_plutoWidget {
Q_OBJECT
public:
			plutoHandler		(QSettings *, QString &);
            		~plutoHandler		();
	void		setVFOFrequency		(i32);
	i32		getVFOFrequency		();
	bool		restartReader		(i32);
	void		stopReader		();
	i32		getSamples		(cf32 *,
	                                                          i32);
	i32		Samples			();
	void		resetBuffer		();
	i16		bitDepth		();

	void		show			();
	void		hide			();
	bool		isHidden		();
	QString		deviceName		();
private:
	QFrame			myFrame;
	RingBuffer<cf32>	_I_Buffer;
	QSettings		*plutoSettings;
	QString			recorderVersion;
	FILE			*xmlDumper;
	xml_fileWriter		*xmlWriter;
	bool			setup_xmlDump	();
	void			close_xmlDump	();
	std::atomic<bool>	dumping;
	bool			filterOn;
	void			run		();
	i32			inputRate;
	i32			vfoFrequency;
	std::atomic<bool>	running;
	bool			debugFlag;
//      configuration items

	struct	iio_device	*rx;
	struct	iio_context	*ctx;
	struct	iio_channel	*rx0_i;
	struct	iio_channel	*rx0_q;
	struct	iio_buffer	*rxbuf;
	struct stream_cfg	rx_cfg;
	bool			connected;
	cf32	convBuffer	[CONV_SIZE + 1];
	i32			convIndex;
	i16			mapTable_int	[DAB_RATE / DIVIDER];
	f32			mapTable_float	[DAB_RATE / DIVIDER];

	void			record_gainSettings	(i32);
	void			update_gainSettings	(i32);
	bool			save_gainSettings;
//
signals:
	void		new_gainValue		(i32);
	void		new_agcValue		(bool);
private slots:
	void		set_gainControl		(i32);
	void		set_agcControl		(i32);
	void		toggle_debugButton	();
	void		set_filter		();
	void		set_xmlDump		();
};
#endif

