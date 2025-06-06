#
/*
 *    Copyright (C) 2014 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation recorder 2 of the License.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include	<QThread>
#include	<QSettings>
#include	<QTime>
#include	<QDate>
#include	<QLabel>
#include	<QFileDialog>
#include	"sdrplay-handler-v2.h"
#include	"sdrplayselect.h"
#include	"xml-filewriter.h"
#include	"device-exceptions.h"
static
i32     RSP1_Table [] = {0, 24, 19, 43};

static
i32     RSP1A_Table [] = {0, 6, 12, 18, 20, 26, 32, 38, 57, 62};

static
i32     RSP2_Table [] = {0, 10, 15, 21, 24, 34, 39, 45, 64};

//static
//i32	RSPduo_Table [] = {0, 6, 12, 18, 20, 26, 32, 38, 57, 62};

static
i32	get_lnaGRdB (i32 hwVersion, i32 lnaState) {
	switch (hwVersion) {
	   case 1:
	      return RSP1_Table [lnaState];

	   case 2:
	      return RSP2_Table [lnaState];

	   default: // both for DUO and RSP1a
	      return RSP1A_Table [lnaState];
	}
}
//
//	here we start
	SdrPlayHandler_v2::SdrPlayHandler_v2  (QSettings *s,
                                         const QString & recorderVersion):
	                                           _I_Buffer (4 * 1024 * 1024),
	                                           myFrame (nullptr) {
mir_sdr_ErrT	err;
f32	ver;
mir_sdr_DeviceT devDesc [4];
//mir_sdr_GainValuesT gainDesc;
//sdrplaySelect	*sdrplaySelector;

	sdrplaySettings			= s;
	this	-> recorderVersion	= recorderVersion;
	sdrplaySettings	-> beginGroup ("sdrplaySettings");
	i32 x	= sdrplaySettings -> value ("position-x", 100). toInt ();
	i32 y	= sdrplaySettings -> value ("position-y", 100). toInt ();
	sdrplaySettings	-> endGroup ();
	setupUi (&myFrame);
	myFrame. move (QPoint (x, y));
	myFrame. show ();
	antennaSelector		-> hide();
	tunerSelector		-> hide();
	this	-> inputRate	= kHz (2048);

	bool success	= fetchLibrary ();
	if (!success) 
	   throw (std_exception_string ("mir_sdr_api could not be found"));
	success = loadFunctions();
	if (!success) {
	   releaseLibrary ();
	   throw (std_exception_string ("function in mir_sdr_api not found"));
	}

	err		= my_mir_sdr_ApiVersion (&ver);
	if (ver < 2.13) {
	   throw (std_exception_string ("Library version too old"));
	}

	api_version	-> display (ver);
	vfoFrequency	= kHz (220000);		// default

//	See if there are settings from previous incarnations
//	and config stuff

	connect (this, SIGNAL (new_GRdBValue (int)),
	         GRdBSelector, SLOT (setValue (int)));
	connect (this, SIGNAL (new_agcSetting (bool)),
	         agcControl, SLOT (setChecked (bool)));

	sdrplaySettings		-> beginGroup ("sdrplaySettings");
	i32 val		=
	            sdrplaySettings -> value ("sdrplay-ifgrdb", 20). toInt();
	if (20 <= val && val <= 59)
	   new_GRdBValue (val);


	ppmControl		-> setValue (
	            sdrplaySettings -> value ("sdrplay-ppm", 0). toInt());
	bool	debugFlag	=
	            sdrplaySettings -> value ("sdrplay-debug", 0). toInt();
	if (!debugFlag)
	   debugControl -> hide();
	bool agcMode		=
	       sdrplaySettings -> value ("sdrplay-agcMode", 0). toInt() != 0;
	if (agcMode) {
	   new_agcSetting	(true);
	   GRdBSelector         -> hide();
	   gainsliderLabel      -> hide();
	}
	save_gainSettings	=
	       sdrplaySettings	-> value ("save_gainSettings", 1). toInt () != 0;
	sdrplaySettings	-> endGroup();

	err = my_mir_sdr_GetDevices (devDesc, &numofDevs, u32 (4));
	if (err != mir_sdr_Success) {
	   fprintf (stderr, "error at GetDevices %s \n",
	                   errorCodes (err). toLatin1(). data());

	   throw (std_exception_string (errorCodes (err). toStdString ()));
	}

	if (numofDevs == 0) {
	   releaseLibrary ();
	   throw (std_exception_string ("No devices found"));
	}

	if (numofDevs > 1) {
           sdrplaySelect sdrplaySelector;
           for (deviceIndex = 0; deviceIndex < (i32)numofDevs; deviceIndex ++) {
#ifndef	__MINGW32__
              sdrplaySelector.
                   addtoList (devDesc [deviceIndex]. DevNm);
#else
              sdrplaySelector.
                   addtoList (devDesc [deviceIndex]. SerNo);
#endif
           }
           deviceIndex = sdrplaySelector. QDialog::exec();
        }
	else
	   deviceIndex = 0;

	serialNumber -> setText (devDesc [deviceIndex]. SerNo);
	hwVersion = devDesc [deviceIndex]. hwVer;
	fprintf (stderr, "hwVer = %d\n", hwVersion);
	fprintf (stderr, "devicename = %s\n", devDesc [deviceIndex]. DevNm);

	err = my_mir_sdr_SetDeviceIdx (deviceIndex);
	if (err != mir_sdr_Success) {
	   fprintf (stderr, "error at SetDeviceIdx %s \n",
	                   errorCodes (err). toLatin1(). data());
//	   my_mir_sdr_ReleaseDeviceIdx (deviceIndex);
	   releaseLibrary ();
	   throw (std_exception_string ("error in setting device parameters"));
	}
//
//	we know we are only in the frequency range 175 .. 230 Mhz,
//	so we can rely on a single table for the lna reductions.
	switch (hwVersion) {
	   case 1:		// old RSP
	      lnaGainSetting	-> setRange (0, 3);
	      deviceModel	= "RSP-I";
	      nrBits		= 12;
	      denominator	= 2048;
	      lnaMax		= 3;
	      break;
	   case 2:
	      lnaGainSetting	-> setRange (0, 8);
	      deviceModel	= "RSP-II";
	      nrBits		= 14;
	      denominator	= 8192;
	      antennaSelector -> show();
	      my_mir_sdr_RSPII_RfNotchEnable (1);
	      if (err != mir_sdr_Success) 
	         fprintf (stderr, "error %d in setting rfNotch\n", err);
	      err = my_mir_sdr_RSPII_AntennaControl (mir_sdr_RSPII_ANTENNA_A);
	      if (err != mir_sdr_Success) 
	         fprintf (stderr, "error %d in setting antenna\n", err);
	      connect (antennaSelector, SIGNAL (textActivated (const QString &)),
	            this, SLOT (set_antennaSelect (const QString &)));
	      lnaMax		= 8;
	      break;
	   case 3:	
	      lnaGainSetting	-> setRange (0, 9);
	      deviceModel	= "RSP-DUO";
	      nrBits		= 14;
	      denominator	= 8192;
	      tunerSelector	-> show();
	      err	= my_mir_sdr_rspDuo_TunerSel (mir_sdr_rspDuo_Tuner_1);
	      if (err != mir_sdr_Success) 
	         fprintf (stderr, "error %d in setting of rspDuo\n", err);
	      connect (tunerSelector, SIGNAL (textActivated (const QString &)),
	               this, SLOT (set_tunerSelect (const QString &)));
	      lnaMax		= 9;
	      break;
	   default:
	      lnaGainSetting	-> setRange (0, 9);
	      deviceModel	= "RSP-Ia";
	      nrBits		= 14;
	      denominator	= 8192;
	      lnaMax		= 9;
	      break;
	}

	val	= sdrplaySettings -> value ("sdrplay-lnastate", 0). toInt();
	if (val < 0)
	   val = 0;
	if (val > lnaMax)
	   val = lnaMax;
//	fprintf (stderr, "val = %d, max = %d\n", val, lnaMax);
	lnaGainSetting	-> setValue (val);
	connect (this, SIGNAL (new_lnaValue (int)),
                 lnaGainSetting, SLOT (setValue (int)));

	val	= sdrplaySettings -> value ("biasT_selector", 0). toInt ();
	if (val != 0) {
	   biasT_selector -> setChecked (true);
	   biasT_selectorHandler (1);
	}
	deviceLabel	-> setText (deviceModel);
//	and be prepared for future changes in the settings
	connect (debugControl, SIGNAL (stateChanged (int)),
	         this, SLOT (set_debugControl (int)));
	connect (ppmControl, SIGNAL (valueChanged (int)),
	         this, SLOT (set_ppmControl (int)));
	connect (dumpButton, SIGNAL (clicked ()),
	         this, SLOT (set_xmlDump ()));
	connect (biasT_selector, SIGNAL (stateChanged (int)),
	         this, SLOT (biasT_selectorHandler (int)));
	lnaGRdBDisplay		-> display (get_lnaGRdB (hwVersion,
	                                         lnaGainSetting -> value()));
	xmlDumper	= nullptr;
	dumping. store (false);
	running. store (false);
}

	SdrPlayHandler_v2::~SdrPlayHandler_v2() {
	stopReader();
	myFrame. hide ();
	sdrplaySettings	-> beginGroup ("sdrplaySettings");
	sdrplaySettings	-> setValue ("position-x", myFrame. pos (). x ());
	sdrplaySettings	-> setValue ("position-y", myFrame. pos (). y ());
	sdrplaySettings -> setValue ("sdrplay-ppm", ppmControl -> value());
	sdrplaySettings -> setValue ("sdrplay-ifgrdb",
	                                    GRdBSelector -> value());
	sdrplaySettings -> setValue ("sdrplay-lnastate",
	                                    lnaGainSetting -> value());
	sdrplaySettings	-> setValue ("sdrplay-agcMode",
	                                  agcControl -> isChecked() ? 1 : 0);
	sdrplaySettings	-> endGroup();
	sdrplaySettings	-> sync();

	if (numofDevs > 0)
	   my_mir_sdr_ReleaseDeviceIdx (deviceIndex);
	releaseLibrary ();
}

i32	SdrPlayHandler_v2::defaultFrequency() {
	return MHz (220);
}

i32	SdrPlayHandler_v2::getVFOFrequency() {
	return vfoFrequency;
}

void	SdrPlayHandler_v2::set_ifgainReduction	(i32 newGain) {
mir_sdr_ErrT	err;
//i32	GRdB		= GRdBSelector	-> value();
i32	lnaState	= lnaGainSetting -> value();

	if (!running. load ())
	   return;

	err =  my_mir_sdr_RSP_SetGr (newGain, lnaState, 1, 0);
	if (err != mir_sdr_Success)
	   fprintf (stderr, "toch weer een fout\n");

	lnaGRdBDisplay	-> display (get_lnaGRdB (hwVersion, lnaState));
}

void	SdrPlayHandler_v2::set_lnagainReduction (i32 lnaState) {
//mir_sdr_ErrT err;


	if (!running. load ())
	   return;

	if (!agcControl -> isChecked()) {
	   (void)my_mir_sdr_RSP_SetGr (GRdBSelector -> value (),
	                                                 lnaState, 1, 0);
	   lnaGRdBDisplay	-> display (get_lnaGRdB (hwVersion, lnaState));
	   return;
	}
//
//	apparently, agcControl is checked
	(void)my_mir_sdr_AgcControl (mir_sdr_AGC_5HZ,
	                             -30, 0, 0, 0, 0, lnaState);
	lnaGRdBDisplay	-> display (get_lnaGRdB (hwVersion, lnaState));
}

//
static
void myStreamCallback (i16		*xi,
	               i16		*xq,
	               u32		firstSampleNum,
	               i32		grChanged,
	               i32		rfChanged,
	               i32		fsChanged,
	               u32		numSamples,
	               u32		reset,
	               u32		hwRemoved,
	               void		*cbContext) {
i16	i;
SdrPlayHandler_v2	*p	= static_cast<SdrPlayHandler_v2 *> (cbContext);
std::complex<i16> localBuf [numSamples];

	if (hwRemoved)
	   fprintf (stderr, "Hardware removed\n");
	if (reset || hwRemoved)
	   return;
	for (i = 0; i <  (i32)numSamples; i ++)
//	   localBuf [i] = std::complex<i16> (xq [i], xi [i]);
	   localBuf [i] = std::complex<i16> (xi [i], xq [i]);
	i32 n = p -> _I_Buffer. get_ring_buffer_write_available ();
	if (n >= (i32)numSamples)
	   p -> _I_Buffer. put_data_into_ring_buffer (localBuf, numSamples);
	else {
	   p -> _I_Buffer. skip_data_in_ring_buffer (2048000 / 2);
	}
	(void)	firstSampleNum;
	(void)	grChanged;
	(void)	rfChanged;
	(void)	fsChanged;
}

void	myGainChangeCallback (u32	GRdB,
	                      u32	lnaGRdB,
	                      void	*cbContext) {
//sdrplayHandler	*p	= static_cast<sdrplayHandler *> (cbContext);
	(void)GRdB;
	(void)lnaGRdB;
	(void)cbContext;
//	p -> lnaGRdBDisplay	-> display ((i32)lnaGRdB);
}

void	SdrPlayHandler_v2::adjustFreq	(i32 offset) {
//	vfoFrequency	+= offset;
//	my_mir_sdr_SetRf ((f64)(vfoFrequency), 1, 0);
}
	
bool	SdrPlayHandler_v2::restartReader	(i32 freq) {
i32	gRdBSystem;
i32	samplesPerPacket;
mir_sdr_ErrT	err;
i32	GRdB		= GRdBSelector	-> value ();
i32	lnaState	= lnaGainSetting	-> value ();
i32	agc		= agcControl	-> isChecked () ? 1 : 0;

	vfoFrequency	= freq;
	if (running. load())
	   return true;

	if (save_gainSettings) {
	   update_gainSettings (freq / MHz (1));
	   set_ifgainReduction	(GRdBSelector -> value ());
	   GRdB		= GRdBSelector		-> value ();
	   set_lnagainReduction (lnaGainSetting -> value ());
	   lnaState	= lnaGainSetting	-> value ();
	   lnaGRdBDisplay	-> display (get_lnaGRdB (hwVersion, lnaState));
	   set_agcControl (agcControl -> isChecked ());
	   agc		= agcControl	-> isChecked () ? 1 : 0;
	}
	err	= my_mir_sdr_StreamInit (&GRdB,
	                                 f64 (inputRate) / MHz (1),
	                                 f64 (vfoFrequency) / MHz (1),
	                                 mir_sdr_BW_1_536,
	                                 mir_sdr_IF_Zero,
	                                 lnaState,
	                                 &gRdBSystem,
	                                 mir_sdr_USE_RSP_SET_GR,
	                                 &samplesPerPacket,
	                                 (mir_sdr_StreamCallback_t)myStreamCallback,
	                                 (mir_sdr_GainChangeCallback_t)myGainChangeCallback,
	                                 this);
	if (err != mir_sdr_Success) {
	   fprintf (stderr, "streamInit error = %s\n",
	                errorCodes (err). toLatin1(). data());
	   return false;
	}
	if (err != mir_sdr_Success) 
	   fprintf (stderr, "setting gain failed (plaats 2)\n");
	err	= my_mir_sdr_SetPpm (f64 (ppmControl -> value()));
	if (err != mir_sdr_Success) 
	   fprintf (stderr, "error = %s\n",
	                errorCodes (err). toLatin1(). data());

	if (agc == 1) {
	   my_mir_sdr_AgcControl (mir_sdr_AGC_5HZ,
	                          -30,
	                          0, 0, 0, 0, lnaGainSetting -> value());
	   GRdBSelector		-> hide ();
	   gainsliderLabel	-> hide ();
	}
	else {
	   GRdBSelector		-> show ();
	   gainsliderLabel	-> show ();
	}
	

	err		= my_mir_sdr_SetDcMode (4, 1);
	if (err != mir_sdr_Success)
	   fprintf (stderr, "error = %s\n",
	                        errorCodes (err). toLatin1(). data());
	err		= my_mir_sdr_SetDcTrackTime (63);
	if (err != mir_sdr_Success)
	   fprintf (stderr, "error = %s\n",
	                errorCodes (err). toLatin1(). data());
	err		= my_mir_sdr_DCoffsetIQimbalanceControl (1, 0);
	if (err != mir_sdr_Success)
           fprintf (stderr, "error = %s\n",
                        errorCodes (err). toLatin1(). data());

	connect (agcControl, SIGNAL (stateChanged (int)),
	         this, SLOT (set_agcControl (int)));
	connect (GRdBSelector, SIGNAL (valueChanged (int)),
	         this, SLOT (set_ifgainReduction (int)));
	connect (lnaGainSetting, SIGNAL (valueChanged (int)),
	         this, SLOT (set_lnagainReduction (int)));
	running. store (true);
	return true;
}

void	SdrPlayHandler_v2::voidSignal	(i32 s) {
	fprintf (stderr, "signal gehad\n");
}


void	SdrPlayHandler_v2::stopReader() {
mir_sdr_ErrT err;

	if (!running. load())
	   return;
	
	disconnect (GRdBSelector, SIGNAL (valueChanged (int)),
	            this, SLOT (set_ifgainReduction (int)));
	disconnect (this, SLOT (set_ifgainReduction (int)));
	disconnect (lnaGainSetting, SIGNAL (valueChanged (int)),
	            this, SLOT (set_lnagainReduction (int)));
	disconnect (agcControl, SIGNAL (stateChanged (int)),
	            this, SLOT (set_agcControl (int)));
	if (save_gainSettings)
	   record_gainSettings	(vfoFrequency / MHz (1));

	close_xmlDump ();		// just to be sure
	err	= my_mir_sdr_StreamUninit();
	if (err != mir_sdr_Success)
	   fprintf (stderr, "error = %s\n",
	                errorCodes (err). toLatin1(). data());
	_I_Buffer. flush_ring_buffer();
	running. store (false);
}

//
//	The brave old getSamples. For the sdrplay, we get
//	size still in I/Q pairs
i32	SdrPlayHandler_v2::getSamples (cf32 *V, i32 size) {
std::complex<i16> temp [size];
i32 i;
	i32 amount	= _I_Buffer. get_data_from_ring_buffer (temp, size);
	for (i = 0; i < amount; i ++) 
	   V [i] = cf32 (real (temp [i]) / (f32) denominator,
	                                imag (temp [i]) / (f32) denominator);
	if (dumping. load ()) 
	   xmlWriter -> add (temp, amount);
	return amount;
}

i32	SdrPlayHandler_v2::Samples () {
	return _I_Buffer. get_ring_buffer_read_available();
}

void	SdrPlayHandler_v2::resetBuffer () {
	_I_Buffer. flush_ring_buffer();
}

i16	SdrPlayHandler_v2::bitDepth () {
	return nrBits;
}

QString	SdrPlayHandler_v2::deviceName	() {
	return deviceModel;
}

void	SdrPlayHandler_v2::set_agcControl (i32 dummy) {
bool agcMode	= agcControl -> isChecked();
	(void)dummy;
	my_mir_sdr_AgcControl (agcMode ? mir_sdr_AGC_5HZ :
	                                 mir_sdr_AGC_DISABLE,
	                       -30,
	                       0, 0, 0, 0, lnaGainSetting -> value());
	if (!agcMode) {
	   GRdBSelector		-> show();
	   gainsliderLabel	-> show();	// old name actually

	   mir_sdr_ErrT err =  my_mir_sdr_RSP_SetGr (GRdBSelector -> value (),
	                                             lnaGainSetting -> value (),
	                                             1, 0);
	   if (err != mir_sdr_Success) 
	      fprintf (stderr, "fout by agcControl\n");
	}
	else {
	   GRdBSelector		-> hide();
	   gainsliderLabel	-> hide();
	}
}

void	SdrPlayHandler_v2::set_debugControl (i32 debugMode) {
	(void)debugMode;
	my_mir_sdr_DebugEnable (debugControl -> isChecked() ? 1 : 0);
}

void	SdrPlayHandler_v2::set_ppmControl (i32 ppm) {
	if (running. load()) {
	   my_mir_sdr_SetPpm	((f32)ppm);
	   my_mir_sdr_SetRf	((f32)vfoFrequency, 1, 0);
	}
}

void	SdrPlayHandler_v2::set_antennaSelect (const QString &s) {
mir_sdr_ErrT err;

	if (hwVersion != 2)	// should not happen
	   return;

	if (s == "Antenna A")
	   err = my_mir_sdr_RSPII_AntennaControl (mir_sdr_RSPII_ANTENNA_A);
	else
	   err = my_mir_sdr_RSPII_AntennaControl (mir_sdr_RSPII_ANTENNA_B);
	(void)err;
}

void	SdrPlayHandler_v2::set_tunerSelect (const QString &s) {
mir_sdr_ErrT err;

	if (hwVersion != 3)	// should not happen
	   return;
	if (s == "Tuner 1") 
	   err	= my_mir_sdr_rspDuo_TunerSel (mir_sdr_rspDuo_Tuner_1);
	else
	   err	= my_mir_sdr_rspDuo_TunerSel (mir_sdr_rspDuo_Tuner_2);

	if (err != mir_sdr_Success) 
	   fprintf (stderr, "error %d in selecting  rspDuo\n", err);
}

bool	SdrPlayHandler_v2::loadFunctions () {
	my_mir_sdr_StreamInit	=
	             (pfn_mir_sdr_StreamInit)
	                        GETPROCADDRESS (Handle, "mir_sdr_StreamInit");
	if (my_mir_sdr_StreamInit == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_StreamInit\n");
	   return false;
	}

	my_mir_sdr_StreamUninit	=
	             (pfn_mir_sdr_StreamUninit)
	                       GETPROCADDRESS (Handle, "mir_sdr_StreamUninit");
	if (my_mir_sdr_StreamUninit == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_StreamUninit\n");
	   return false;
	}

	my_mir_sdr_SetRf	=
	             (pfn_mir_sdr_SetRf)
	                       GETPROCADDRESS (Handle, "mir_sdr_SetRf");
	if (my_mir_sdr_SetRf == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_SetRf\n");
	   return false;
	}

	my_mir_sdr_SetFs	=
	             (pfn_mir_sdr_SetFs)
	                       GETPROCADDRESS (Handle, "mir_sdr_SetFs");
	if (my_mir_sdr_SetFs == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_SetFs\n");
	   return false;
	}

	my_mir_sdr_SetGr	=
	             (pfn_mir_sdr_SetGr)
	                       GETPROCADDRESS (Handle, "mir_sdr_SetGr");
	if (my_mir_sdr_SetGr == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_SetGr\n");
	   return false;
	}

	my_mir_sdr_RSP_SetGr	=
	             (pfn_mir_sdr_RSP_SetGr)
	                       GETPROCADDRESS (Handle, "mir_sdr_RSP_SetGr");
	if (my_mir_sdr_RSP_SetGr == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_RSP_SetGr\n");
	   return false;
	}

	my_mir_sdr_SetGrParams	=
	             (pfn_mir_sdr_SetGrParams)
	                    GETPROCADDRESS (Handle, "mir_sdr_SetGrParams");
	if (my_mir_sdr_SetGrParams == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_SetGrParams\n");
	   return false;
	}

	my_mir_sdr_SetDcMode	=
	             (pfn_mir_sdr_SetDcMode)
	                    GETPROCADDRESS (Handle, "mir_sdr_SetDcMode");
	if (my_mir_sdr_SetDcMode == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_SetDcMode\n");
	   return false;
	}

	my_mir_sdr_SetDcTrackTime	=
	             (pfn_mir_sdr_SetDcTrackTime)
	                    GETPROCADDRESS (Handle, "mir_sdr_SetDcTrackTime");
	if (my_mir_sdr_SetDcTrackTime == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_SetDcTrackTime\n");
	   return false;
	}

	my_mir_sdr_SetSyncUpdateSampleNum =
	             (pfn_mir_sdr_SetSyncUpdateSampleNum)
	               GETPROCADDRESS (Handle, "mir_sdr_SetSyncUpdateSampleNum");
	if (my_mir_sdr_SetSyncUpdateSampleNum == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_SetSyncUpdateSampleNum\n");
	   return false;
	}

	my_mir_sdr_SetSyncUpdatePeriod	=
	             (pfn_mir_sdr_SetSyncUpdatePeriod)
	                    GETPROCADDRESS (Handle, "mir_sdr_SetSyncUpdatePeriod");
	if (my_mir_sdr_SetSyncUpdatePeriod == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_SetSyncUpdatePeriod\n");
	   return false;
	}

	my_mir_sdr_ApiVersion	=
	             (pfn_mir_sdr_ApiVersion)
	                    GETPROCADDRESS (Handle, "mir_sdr_ApiVersion");
	if (my_mir_sdr_ApiVersion == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_ApiVersion\n");
	   return false;
	}

	my_mir_sdr_AgcControl	=
	             (pfn_mir_sdr_AgcControl)
	                      GETPROCADDRESS (Handle, "mir_sdr_AgcControl");
	if (my_mir_sdr_AgcControl == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_AgcControl\n");
	   return false;
	}

	my_mir_sdr_Reinit	=
	             (pfn_mir_sdr_Reinit)
	                     GETPROCADDRESS (Handle, "mir_sdr_Reinit");
	if (my_mir_sdr_Reinit == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_Reinit\n");
	   return false;
	}

	my_mir_sdr_SetPpm	=
	             (pfn_mir_sdr_SetPpm)
	                     GETPROCADDRESS (Handle, "mir_sdr_SetPpm");
	if (my_mir_sdr_SetPpm == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_SetPpm\n");
	   return false;
	}

	my_mir_sdr_DebugEnable	=
	             (pfn_mir_sdr_DebugEnable)
	                     GETPROCADDRESS (Handle, "mir_sdr_DebugEnable");
	if (my_mir_sdr_DebugEnable == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_DebugEnable\n");
	   return false;
	}

	my_mir_sdr_rspDuo_TunerSel =
	             (pfn_mir_sdr_rspDuo_TunerSel)
	                     GETPROCADDRESS (Handle, "mir_sdr_rspDuo_TunerSel");
	if (my_mir_sdr_rspDuo_TunerSel == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_rspDuo_TunerSel\n");
	   return false;
	}

	my_mir_sdr_DCoffsetIQimbalanceControl	=
	          (pfn_mir_sdr_DCoffsetIQimbalanceControl)
	                GETPROCADDRESS (Handle, "mir_sdr_DCoffsetIQimbalanceControl");
	if (my_mir_sdr_DCoffsetIQimbalanceControl == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_DCoffsetIQimbalanceControl\n");
	   return false;
	}

	my_mir_sdr_ResetUpdateFlags	=
	          (pfn_mir_sdr_ResetUpdateFlags)
	                GETPROCADDRESS (Handle, "mir_sdr_ResetUpdateFlags");
	if (my_mir_sdr_ResetUpdateFlags == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_ResetUpdateFlags\n");
	   return false;
	}

	my_mir_sdr_GetDevices		=
	          (pfn_mir_sdr_GetDevices)
	                GETPROCADDRESS (Handle, "mir_sdr_GetDevices");
	if (my_mir_sdr_GetDevices == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_GetDevices");
	   return false;
	}

	my_mir_sdr_GetCurrentGain	=
	          (pfn_mir_sdr_GetCurrentGain)
	                GETPROCADDRESS (Handle, "mir_sdr_GetCurrentGain");
	if (my_mir_sdr_GetCurrentGain == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_GetCurrentGain");
	   return false;
	}

	my_mir_sdr_GetHwVersion	=
	          (pfn_mir_sdr_GetHwVersion)
	                GETPROCADDRESS (Handle, "mir_sdr_GetHwVersion");
	if (my_mir_sdr_GetHwVersion == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_GetHwVersion");
	   return false;
	}

	my_mir_sdr_RSPII_AntennaControl	=
	          (pfn_mir_sdr_RSPII_AntennaControl)
	                GETPROCADDRESS (Handle, "mir_sdr_RSPII_AntennaControl");
	if (my_mir_sdr_RSPII_AntennaControl == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_RSPII_AntennaControl");
	   return false;
	}

	my_mir_sdr_SetDeviceIdx	=
	          (pfn_mir_sdr_SetDeviceIdx)
	                GETPROCADDRESS (Handle, "mir_sdr_SetDeviceIdx");
	if (my_mir_sdr_SetDeviceIdx == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_SetDeviceIdx");
	   return false;
	}

	my_mir_sdr_ReleaseDeviceIdx	=
	          (pfn_mir_sdr_ReleaseDeviceIdx)
	                GETPROCADDRESS (Handle, "mir_sdr_ReleaseDeviceIdx");
	if (my_mir_sdr_ReleaseDeviceIdx == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_ReleaseDeviceIdx");
	   return false;
	}

	my_mir_sdr_RSPII_RfNotchEnable	=
	          (pfn_mir_sdr_RSPII_RfNotchEnable)
	                GETPROCADDRESS (Handle, "mir_sdr_RSPII_RfNotchEnable");
	if (my_mir_sdr_RSPII_RfNotchEnable == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_RSPII_RfNotchEnable\n");
	   return false;
	}

	my_mir_sdr_RSPII_BiasTControl =
	          (pfn_mir_sdr_RSPII_BiasTControl)
	                GETPROCADDRESS (Handle,  "mir_sdr_RSPII_BiasTControl");
	if (my_mir_sdr_RSPII_BiasTControl == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_RSPII_BiasTControl\n");
	   return false;
	}

	my_mir_sdr_rsp1a_BiasT =
	          (pfn_mir_sdr_rsp1a_BiasT)
	               GETPROCADDRESS (Handle, "mir_sdr_rsp1a_BiasT");
	if (my_mir_sdr_rsp1a_BiasT == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_rsp1a_BiasT\n");
	   return false;
	}

	my_mir_sdr_rspDuo_BiasT =
	          (pfn_mir_sdr_rspDuo_BiasT)
	               GETPROCADDRESS (Handle, "mir_sdr_rspDuo_BiasT");
	if (my_mir_sdr_rspDuo_BiasT == nullptr) {
	   fprintf (stderr, "Could not find mir_sdr_rspDuo_BiasT\n");
	   return false;
	}

	return true;
}

bool	SdrPlayHandler_v2::fetchLibrary	() {
#ifdef  __MINGW32__
HKEY APIkey;
wchar_t APIkeyValue [256];
ULONG APIkeyValue_length = 255;

        wchar_t *libname = (wchar_t *)L"mir_sdr_api.dll";
        Handle  = LoadLibrary (libname);
	if (Handle == NULL) {
	   if (RegOpenKey (HKEY_LOCAL_MACHINE,
	                   TEXT("Software\\MiricsSDR\\API"),
	                   &APIkey) != ERROR_SUCCESS) {
              fprintf (stderr,
	               "failed to locate API registry entry, error = %d\n",
	               (i32)GetLastError ());
	      return false;
	   }

	   RegQueryValueEx (APIkey,
	                    (wchar_t *)L"Install_Dir",
	                    NULL,
	                    NULL,
	                    (LPBYTE)&APIkeyValue,
	                    (LPDWORD)&APIkeyValue_length);
//	Ok, make explicit it is in the 64 bits section
	   wchar_t *x =
#ifdef	__BITS64__
	        wcscat (APIkeyValue, (wchar_t *)L"\\x64\\mir_sdr_api.dll");
#else
	        wcscat (APIkeyValue, (wchar_t *)L"\\x86\\mir_sdr_api.dll");
#endif
	   RegCloseKey (APIkey);

	   Handle	= LoadLibrary (x);
	   if (Handle == NULL) {
	      return false;
	   }
	}
#else
	Handle		= dlopen ("libusb-1.0.so", RTLD_NOW | RTLD_GLOBAL);

	Handle		= dlopen ("libmirsdrapi-rsp.so", RTLD_NOW);
	if (Handle == NULL)
	   Handle	= dlopen ("libmir_sdr.so", RTLD_NOW);

	if (Handle == NULL) {
	   fprintf (stderr, "error report %s\n", dlerror ());
	   return false;
	}
#endif
	libraryLoaded	= true;
	return true;
}

void	SdrPlayHandler_v2::releaseLibrary	() {
#ifdef __MINGW32__
        FreeLibrary (Handle);
#else
        dlclose (Handle);
#endif
}

void	SdrPlayHandler_v2::show	() {
	myFrame. show ();
}

void	SdrPlayHandler_v2::hide	() {
	myFrame. hide ();
}

bool	SdrPlayHandler_v2::isHidden	() {
	return myFrame. isHidden ();
}


QString	SdrPlayHandler_v2::errorCodes (mir_sdr_ErrT err) {
	switch (err) {
	   case mir_sdr_Success:
	      return "success";
	   case mir_sdr_Fail:
	      return "Fail";
	   case mir_sdr_InvalidParam:
	      return "invalidParam";
	   case mir_sdr_OutOfRange:
	      return "OutOfRange";
	   case mir_sdr_GainUpdateError:
	      return "GainUpdateError";
	   case mir_sdr_RfUpdateError:
	      return "RfUpdateError";
	   case mir_sdr_FsUpdateError:
	      return "FsUpdateError";
	   case mir_sdr_HwError:
	      return "HwError";
	   case mir_sdr_AliasingError:
	      return "AliasingError";
	   case mir_sdr_AlreadyInitialised:
	      return "AlreadyInitialised";
	   case mir_sdr_NotInitialised:
	      return "NotInitialised";
	   case mir_sdr_NotEnabled:
	      return "NotEnabled";
	   case mir_sdr_HwVerError:
	      return "HwVerError";
	   case mir_sdr_OutOfMemError:
	      return "OutOfMemError";
	   case mir_sdr_HwRemoved:
	      return "HwRemoved";
	   default:
	      return "???";
	}
}

void	SdrPlayHandler_v2::set_xmlDump () {
	if (xmlDumper == nullptr) {
	  if (setup_xmlDump ())
	      dumpButton	-> setText ("writing");
	}
	else {
	   close_xmlDump ();
	   dumpButton	-> setText ("Dump");
	}
}

static inline
bool	isValid (QChar c) {
	return c. isLetterOrNumber () || (c == '-');
}

bool	SdrPlayHandler_v2::setup_xmlDump () {
QTime	theTime;
QDate	theDate;
QString	saveDir	= sdrplaySettings -> value (sSettingSampleStorageDir,
	                                   QDir::homePath ()). toString ();
	if ((saveDir != "") && (!saveDir. endsWith ("/")))
	   saveDir += "/";

	QString channel		= sdrplaySettings -> value ("channel", "xx").
	                                                       toString ();
	QString timeString      = theDate. currentDate (). toString () + "-" +
	                           theTime. currentTime(). toString ();
	for (i32 i = 0; i < timeString. length (); i ++)
           if (!isValid (timeString. at (i)))
              timeString. replace (i, 1, '-');
	
        QString suggestedFileName =
                    saveDir + deviceModel + "-" + channel + "-" + timeString;

	QString fileName = QFileDialog::getSaveFileName (nullptr,
	                                         tr ("Save file ..."),
	                                         suggestedFileName + ".uff",
	                                         tr ("Xml (*.uff)"));
        fileName        = QDir::toNativeSeparators (fileName);
        xmlDumper	= fopen (fileName. toUtf8(). data(), "w");
	if (xmlDumper == nullptr)
	   return false;
	
	xmlWriter	= new XmlFileWriter (xmlDumper,
                                  nrBits,
                                  "int16",
                                  2048000,
                                  getVFOFrequency (),
                                  "SDRplay",
                                  deviceModel,
                                  recorderVersion);
	dumping. store (true);

	QString dumper	= QDir::fromNativeSeparators (fileName);
	i32 x		= dumper. lastIndexOf ("/");
        saveDir		= dumper. remove (x, dumper. size () - x);
	sdrplaySettings	-> setValue (sSettingSampleStorageDir, saveDir);
	return true;
}

void	SdrPlayHandler_v2::close_xmlDump () {
	if (xmlDumper == nullptr)	// this can happen !!
	   return;
	dumping. store (false);
	usleep (1000);
	xmlWriter	-> computeHeader ();
	delete xmlWriter;
	fclose (xmlDumper);
	xmlDumper	= nullptr;
}

/////////////////////////////////////////////////////////////////////////
//Experimental
/////////////////////////////////////////////////////////////////////////
//
//	the frequency (the MHz component) is used as key
//
void	SdrPlayHandler_v2::record_gainSettings (i32 freq) {
i32	GRdB		= GRdBSelector		-> value ();
i32	lnaState	= lnaGainSetting	-> value ();
i32	agc		= agcControl		-> isChecked () == 1;
QString theValue	= QString::number (GRdB) + ":";

	theValue. append (QString::number (lnaState));
	theValue. append (":");
	theValue. append (QString::number (agc));

	sdrplaySettings         -> beginGroup ("sdrplaySettings");
	sdrplaySettings		-> setValue (QString::number (freq), theValue);
	sdrplaySettings		-> endGroup ();
}

void	SdrPlayHandler_v2::update_gainSettings (i32 freq) {
i32	GRdB;
i32	lnaState;
i32	agc;
QString	theValue	= "";

	sdrplaySettings	-> beginGroup ("sdrplaySettings");
	theValue	= sdrplaySettings -> value (QString::number (freq), ""). toString ();
	sdrplaySettings	-> endGroup ();

	if (theValue == QString (""))
	   return;		// or set some defaults here

	QStringList result	= theValue. split (":");
	if (result. size () < 3) 	// should not happen
	   return;

	GRdB		= result. at (0). toInt ();
	lnaState	= result. at (1). toInt ();
	agc		= result. at (2). toInt ();

	if (lnaState > lnaMax)
	   lnaState = lnaMax;
	GRdBSelector	-> blockSignals (true);
	new_GRdBValue (GRdB);
	while (GRdBSelector -> value () != GRdB)
	   usleep (1000);
	set_ifgainReduction	(GRdBSelector -> value ());
	GRdBSelector	-> blockSignals (false);

	lnaGainSetting	-> blockSignals (true);
	new_lnaValue (lnaState);
	while (lnaGainSetting -> value () != lnaState)
	   usleep (1000);
	lnaGainSetting	-> blockSignals (false);

	agcControl	-> blockSignals (true);
	new_agcSetting (agc == 1);
	while (agcControl -> isChecked () != (agc == 1))
	   usleep (1000);
	agcControl	-> blockSignals (false);
}

void	SdrPlayHandler_v2::biasT_selectorHandler (i32 k) {
bool setting = biasT_selector -> isChecked ();
	sdrplaySettings -> setValue ("biasT_selector", setting ? 1 : 0);
	switch (hwVersion) {
	   case 1:		// old RSP
	      return;		// no support for biasT
	   case 2:		// RSP 2
	      my_mir_sdr_RSPII_BiasTControl (setting? 1 : 0);
	      return;
	   case 3:		// RSP duo
	      my_mir_sdr_rspDuo_BiasT (setting ? 1 : 0);
	      return;
	   default:		// RSP1a
	      my_mir_sdr_rsp1a_BiasT (setting ? 1 : 0);
	      return;
	}
}

QPoint	SdrPlayHandler_v2::get_coords	() {
	return myFrame. mapToGlobal (QPoint (0, 0));
}

void	SdrPlayHandler_v2::moveTo		(QPoint p) {
	myFrame. move (p);
}

void SdrPlayHandler_v2::setVFOFrequency(i32)
{
  // TODO: implement this (needed for 'DC avoidance algorithm')
}

bool SdrPlayHandler_v2::isFileInput()
{
  return false;
}

