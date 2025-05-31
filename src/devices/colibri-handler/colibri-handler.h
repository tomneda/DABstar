#
/*
 *    Copyright (C) 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the spectrumViewer
 *
 *    spectrumViewer is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation version 2 of the License.
 *
 *    spectrumViewer is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with spectrumViewer. if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef	__COLIBRI_HANDLER_H
#define	__COLIBRI_HANDLER_H
#include	<QThread>
#include	<QSettings>
#include	<QFileDialog>
#include	<QTime>
#include	<QDate>
#include	<QLabel>
#include	<QDebug>
#include	<QFileDialog>
#include	"ui_colibri-widget.h"
#include	"common.h"
#include	"LibLoader.h"
#include	"ringbuffer.h"
#include	"device-handler.h"

	class	colibriHandler: public deviceHandler, public Ui_colibriWidget {
Q_OBJECT
public:

			colibriHandler		(QSettings *,
	                                         bool);
			~colibriHandler		(void);
	bool		restartReader		(i32);
	void		stopReader		();
	void		setVFOFrequency		(i32);
	i32		getVFOFrequency		(void);
	i32		getSamples		(cf32 *, i32);
	i32		Samples			();
	void		resetBuffer		(void);
	i16		bitDepth		(void);
	void		hide			();
	void		show			();
	bool		isHidden		();
	QString		deviceName		();

	RingBuffer<cf32>	_I_Buffer;
	i16		convBufferSize;
	i16		convIndex;
	std::vector <cf32>   convBuffer;
	i16		mapTable_int   [2048];
	f32		mapTable_float [2048];
private:
	QFrame			myFrame;
	LibLoader		m_loader;
	QSettings		*colibriSettings;
	i32			sampleRate	(i32);
	i32			selectedRate;
	Descriptor		m_deskriptor;
	std::atomic<bool>	running;
	bool			iqSwitcher;
private slots:
	void			set_gainControl	(i32);
	void			handle_iqSwitcher	();
};
#endif
