#
/*
 *    Copyright (C) 2014 .. 2019
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

#include	<QFileDialog>
#include	<QTime>
#include	"lime-handler.h"
#include	"xml-filewriter.h"
#include	"device-exceptions.h"

#define	FIFO_SIZE	32768
static
i16 localBuffer [4 * FIFO_SIZE];
lms_info_str_t limedevices [10];

	LimeHandler::LimeHandler (QSettings *s,
                            const QString & recorderVersion):
	                             myFrame (nullptr),
	                             _I_Buffer (4* 1024 * 1024),
	                             theFilter (5, 1560000 / 2, 2048000) {
	this	-> limeSettings		= s;
	this	-> recorderVersion	= recorderVersion;
	setupUi (&myFrame);
  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
	myFrame. show	();

	filtering	= false;

	limeSettings	-> beginGroup ("limeSettings");
	currentDepth	= limeSettings	-> value ("filterDepth", 5). toInt ();
	limeSettings	-> endGroup ();
	filterDepth	-> setValue (currentDepth);
	theFilter. resize (currentDepth);
#ifdef  __MINGW32__
        const char *libraryString = "LimeSuite.dll";
        Handle          = LoadLibrary ((wchar_t *)L"LimeSuite.dll");
#elif  __clang__
        const char *libraryString = "/opt/local/lib/libLimeSuite.dylib";
        Handle		= dlopen (libraryString, RTLD_NOW);
#else
        const char *libraryString = "libLimeSuite.so";
        Handle          = dlopen (libraryString, RTLD_NOW);
#endif

        if (Handle == nullptr) {
           throw (std_exception_string ("failed to open " +
	                             std::string (libraryString)));
        }

        libraryLoaded   = true;
	if (!load_limeFunctions()) {
#ifdef __MINGW32__
           FreeLibrary (Handle);
#else
           dlclose (Handle);
#endif
           throw (std_exception_string ("could not load all required lib functions"));
        }
//
//      From here we have a library available
	i32 ndevs	= LMS_GetDeviceList (limedevices);
	if (ndevs == 0) {	// no devices found
	   throw (std_exception_string ("No lime device found"));
	}

	for (i32 i = 0; i < ndevs; i ++)
	   fprintf (stderr, "device %s\n", limedevices [i]);

	i32 res		= LMS_Open (&theDevice, nullptr, nullptr);
	if (res < 0) {	// some error
	   throw (std_exception_string ("failed to open device"));
	}

	res		= LMS_Init (theDevice);
	if (res < 0) {	// some error
	   LMS_Close (&theDevice);
	   throw (std_exception_string ("failed to initialize device"));
	}

	res		= LMS_GetNumChannels (theDevice, LMS_CH_RX);
	if (res < 0) {	// some error
	   LMS_Close (&theDevice);
	   throw (std_exception_string ("could not set number of channels"));
	}

	fprintf (stderr, "device %s supports %d channels\n",
	                            limedevices [0], res);
	res		= LMS_EnableChannel (theDevice, LMS_CH_RX, 0, true);
	if (res < 0) {	// some error
	   LMS_Close (theDevice);
	   throw (std_exception_string ("could not enable channels"));
	}

	res	= LMS_SetSampleRate (theDevice, 2048000.0, 0);
	if (res < 0) {
	   LMS_Close (theDevice);
	   throw (std_exception_string ("could not set samplerate"));
	}

	float_type host_Hz, rf_Hz;
	res	= LMS_GetSampleRate (theDevice, LMS_CH_RX, 0,
	                                            &host_Hz, &rf_Hz);

	fprintf (stderr, "samplerate = %f %f\n", (f32)host_Hz, (f32)rf_Hz);
	
	res		= LMS_GetAntennaList (theDevice, LMS_CH_RX, 0, antennas);
	for (i32 i = 0; i < res; i ++)
	   antennaList	-> addItem (QString (antennas [i]));

	limeSettings -> beginGroup ("limeSettings");
	QString antenne	= limeSettings -> value ("antenna", "default"). toString();
	save_gainSettings	=
	          limeSettings -> value ("save_gainSettings", 1). toInt () != 0;
	limeSettings	-> endGroup();

	i32 k       = antennaList -> findText (antenne);
        if (k != -1) 
           antennaList -> setCurrentIndex (k);
	
	connect (antennaList, SIGNAL (activated (int)),
	         this, SLOT (setAntenna (int)));

//	default antenna setting
	res		= LMS_SetAntenna (theDevice, LMS_CH_RX, 0, 
	                           antennaList -> currentIndex());

//	default frequency
	res		= LMS_SetLOFrequency (theDevice, LMS_CH_RX,
	                                                 0, 220000000.0);
	if (res < 0) {
	   LMS_Close (theDevice);
	   throw (std_exception_string ("could not set LO frequency"));
	}

	res		= LMS_SetLPFBW (theDevice, LMS_CH_RX,
	                                               0, 1536000.0);
	if (res < 0) {
	   LMS_Close (theDevice);
	   throw (std_exception_string ("could not set bandwidth"));
	}

	LMS_SetGaindB (theDevice, LMS_CH_RX, 0, 50);

	LMS_Calibrate (theDevice, LMS_CH_RX, 0, 2500000.0, 0);
	
	
	limeSettings	-> beginGroup ("limeSettings");
	k	= limeSettings	-> value ("gain", 50). toInt();
	limeSettings	-> endGroup();
	gainSelector -> setValue (k);
	setGain (k);
	connect (gainSelector, SIGNAL (valueChanged (int)),
	         this, SLOT (setGain (int)));
	connect (dumpButton, SIGNAL (clicked ()),
	         this, SLOT (set_xmlDump ()));
	connect (this, SIGNAL (new_gainValue (int)),
	         gainSelector, SLOT (setValue (int)));
	connect (filterSelector, SIGNAL (stateChanged (int)),
	         this, SLOT (set_filter (int)));
	xmlDumper	= nullptr;
	dumping. store (false);
	running. store (false);
}

	LimeHandler::~LimeHandler() {
	stopReader ();
	running. store (false);
	while (isRunning())
	   usleep (100);
	myFrame. hide ();
	limeSettings	-> beginGroup ("limeSettings");
	limeSettings	-> setValue ("antenna", antennaList -> currentText());
	limeSettings	-> setValue ("gain", gainSelector -> value());
	limeSettings	-> setValue ("filterDepth", filterDepth -> value ());
	limeSettings	-> endGroup();
	LMS_Close (theDevice);
}

void	LimeHandler::setVFOFrequency	(i32 f) {
	LMS_SetLOFrequency (theDevice, LMS_CH_RX, 0, f);
}

i32	LimeHandler::getVFOFrequency() {
float_type freq;
	/*i32 res = */LMS_GetLOFrequency (theDevice, LMS_CH_RX, 0, &freq);
	return (i32)freq;
}

void	LimeHandler::setGain		(i32 g) {
float_type gg;
	LMS_SetGaindB (theDevice, LMS_CH_RX, 0, g);
	LMS_GetNormalizedGain (theDevice, LMS_CH_RX, 0, &gg);
	actualGain	-> display (gg);
}

void	LimeHandler::setAntenna		(i32 ind) {
	(void)LMS_SetAntenna (theDevice, LMS_CH_RX, 0, ind);
}

void	LimeHandler::set_filter		(i32 c) {
	filtering	= filterSelector -> isChecked ();
	fprintf (stderr, "filter set %s\n", filtering ? "on" : "off");
}

bool	LimeHandler::restartReader	(i32 freq) {
i32	res;

	if (isRunning())
	   return true;

	vfoFrequency	= freq;
	if (save_gainSettings) {
	   update_gainSettings	(freq / MHz (1));
	   setGain (gainSelector -> value ());
	}
	LMS_SetLOFrequency (theDevice, LMS_CH_RX, 0, freq);
	stream. isTx            = false;
        stream. channel         = 0;
        stream. fifoSize        = FIFO_SIZE;
        stream. throughputVsLatency     = 0.1;  // ???
        stream. dataFmt         = lms_stream_t::LMS_FMT_I12;    // 12 bit ints

	res     = LMS_SetupStream (theDevice, &stream);
        if (res < 0)
           return false;
        res     = LMS_StartStream (&stream);
        if (res < 0)
           return false;

	start ();
	return true;
}
	
void	LimeHandler::stopReader() {
	close_xmlDump ();
	if (!isRunning())
	   return;
	if (save_gainSettings)
	   record_gainSettings (vfoFrequency);

	running. store (false);
	while (isRunning())
	   usleep (200);
	(void)LMS_StopStream	(&stream);	
	(void)LMS_DestroyStream	(theDevice, &stream);
}

i32	LimeHandler::getSamples	(cf32 *V, i32 size) {
std::complex<i16> temp [size];

        i32 amount      = _I_Buffer. get_data_from_ring_buffer (temp, size);
	if (filtering) {
	   if (filterDepth -> value () != currentDepth) {
	      currentDepth = filterDepth -> value ();
	      theFilter. resize (currentDepth);
	   }
           for (i32 i = 0; i < amount; i ++)
	      V [i] = theFilter. Pass (cf32 (
	                                         real (temp [i]) / 2048.0,
	                                         imag (temp [i]) / 2048.0));
	}
	else
           for (i32 i = 0; i < amount; i ++)
              V [i] = cf32 (real (temp [i]) / 2048.0,
                                           imag (temp [i]) / 2048.0);
        if (dumping. load ())
           xmlWriter -> add (temp, amount);
        return amount;
}

i32	LimeHandler::Samples() {
	return _I_Buffer. get_ring_buffer_read_available();
}

void	LimeHandler::resetBuffer() {
	_I_Buffer. flush_ring_buffer();
}

i16	LimeHandler::bitDepth() {
	return 12;
}

QString	LimeHandler::deviceName	() {
	return "limeSDR";
}

void	LimeHandler::showErrors		(i32 underrun, i32 overrun) {
	underrunDisplay	-> display (underrun);
	overrunDisplay	-> display (overrun);
}


void	LimeHandler::run() {
i32	res;
lms_stream_status_t streamStatus;
i32	underruns	= 0;
i32	overruns	= 0;
//i32	dropped		= 0;
i32	amountRead	= 0;

	running. store (true);
	while (running. load()) {
	   res = LMS_RecvStream (&stream, localBuffer,
	                                     FIFO_SIZE,  &meta, 1000);
	   if (res > 0) {
	      _I_Buffer. put_data_into_ring_buffer (localBuffer, res);
	      amountRead	+= res;
	      res	= LMS_GetStreamStatus (&stream, &streamStatus);
	      underruns	+= streamStatus. underrun;
	      overruns	+= streamStatus. overrun;
	   }
	   if (amountRead > 4 * 2048000) {
	      amountRead = 0;
	      showErrors (underruns, overruns);
	      underruns	= 0;
	      overruns	= 0;
	   }
	}
}

bool	LimeHandler::load_limeFunctions() {

	this	-> LMS_GetDeviceList = (pfn_LMS_GetDeviceList)
	                    GETPROCADDRESS (Handle, "LMS_GetDeviceList");
	if (this -> LMS_GetDeviceList == nullptr) {
	   fprintf (stderr, "could not find LMS_GetdeviceList\n");
	   return false;
	}
	this	-> LMS_Open = (pfn_LMS_Open)
	                    GETPROCADDRESS (Handle, "LMS_Open");
	if (this -> LMS_Open == nullptr) {
	   fprintf (stderr, "could not find LMS_Open\n");
	   return false;
	}
	this	-> LMS_Close = (pfn_LMS_Close)
	                    GETPROCADDRESS (Handle, "LMS_Close");
	if (this -> LMS_Close == nullptr) {
	   fprintf (stderr, "could not find LMS_Close\n");
	   return false;
	}
	this	-> LMS_Init = (pfn_LMS_Init)
	                    GETPROCADDRESS (Handle, "LMS_Init");
	if (this -> LMS_Init == nullptr) {
	   fprintf (stderr, "could not find LMS_Init\n");
	   return false;
	}
	this	-> LMS_GetNumChannels = (pfn_LMS_GetNumChannels)
	                    GETPROCADDRESS (Handle, "LMS_GetNumChannels");
	if (this -> LMS_GetNumChannels == nullptr) {
	   fprintf (stderr, "could not find LMS_GetNumChannels\n");
	   return false;
	}
	this	-> LMS_EnableChannel = (pfn_LMS_EnableChannel)
	                    GETPROCADDRESS (Handle, "LMS_EnableChannel");
	if (this -> LMS_EnableChannel == nullptr) {
	   fprintf (stderr, "could not find LMS_EnableChannel\n");
	   return false;
	}
	this	-> LMS_SetSampleRate = (pfn_LMS_SetSampleRate)
	                    GETPROCADDRESS (Handle, "LMS_SetSampleRate");
	if (this -> LMS_SetSampleRate == nullptr) {
	   fprintf (stderr, "could not find LMS_SetSampleRate\n");
	   return false;
	}
	this	-> LMS_GetSampleRate = (pfn_LMS_GetSampleRate)
	                    GETPROCADDRESS (Handle, "LMS_GetSampleRate");
	if (this -> LMS_GetSampleRate == nullptr) {
	   fprintf (stderr, "could not find LMS_GetSampleRate\n");
	   return false;
	}
	this	-> LMS_SetLOFrequency = (pfn_LMS_SetLOFrequency)
	                    GETPROCADDRESS (Handle, "LMS_SetLOFrequency");
	if (this -> LMS_SetLOFrequency == nullptr) {
	   fprintf (stderr, "could not find LMS_SetLOFrequency\n");
	   return false;
	}
	this	-> LMS_GetLOFrequency = (pfn_LMS_GetLOFrequency)
	                    GETPROCADDRESS (Handle, "LMS_GetLOFrequency");
	if (this -> LMS_GetLOFrequency == nullptr) {
	   fprintf (stderr, "could not find LMS_GetLOFrequency\n");
	   return false;
	}
	this	-> LMS_GetAntennaList = (pfn_LMS_GetAntennaList)
	                    GETPROCADDRESS (Handle, "LMS_GetAntennaList");
	if (this -> LMS_GetAntennaList == nullptr) {
	   fprintf (stderr, "could not find LMS_GetAntennaList\n");
	   return false;
	}
	this	-> LMS_SetAntenna = (pfn_LMS_SetAntenna)
	                    GETPROCADDRESS (Handle, "LMS_SetAntenna");
	if (this -> LMS_SetAntenna == nullptr) {
	   fprintf (stderr, "could not find LMS_SetAntenna\n");
	   return false;
	}
	this	-> LMS_GetAntenna = (pfn_LMS_GetAntenna)
	                    GETPROCADDRESS (Handle, "LMS_GetAntenna");
	if (this -> LMS_GetAntenna == nullptr) {
	   fprintf (stderr, "could not find LMS_GetAntenna\n");
	   return false;
	}
	this	-> LMS_GetAntennaBW = (pfn_LMS_GetAntennaBW)
	                    GETPROCADDRESS (Handle, "LMS_GetAntennaBW");
	if (this -> LMS_GetAntennaBW == nullptr) {
	   fprintf (stderr, "could not find LMS_GetAntennaBW\n");
	   return false;
	}
	this	-> LMS_SetNormalizedGain = (pfn_LMS_SetNormalizedGain)
	                    GETPROCADDRESS (Handle, "LMS_SetNormalizedGain");
	if (this -> LMS_SetNormalizedGain == nullptr) {
	   fprintf (stderr, "could not find LMS_SetNormalizedGain\n");
	   return false;
	}
	this	-> LMS_GetNormalizedGain = (pfn_LMS_GetNormalizedGain)
	                    GETPROCADDRESS (Handle, "LMS_GetNormalizedGain");
	if (this -> LMS_GetNormalizedGain == nullptr) {
	   fprintf (stderr, "could not find LMS_GetNormalizedGain\n");
	   return false;
	}
	this	-> LMS_SetGaindB = (pfn_LMS_SetGaindB)
	                    GETPROCADDRESS (Handle, "LMS_SetGaindB");
	if (this -> LMS_SetGaindB == nullptr) {
	   fprintf (stderr, "could not find LMS_SetGaindB\n");
	   return false;
	}
	this	-> LMS_GetGaindB = (pfn_LMS_GetGaindB)
	                    GETPROCADDRESS (Handle, "LMS_GetGaindB");
	if (this -> LMS_GetGaindB == nullptr) {
	   fprintf (stderr, "could not find LMS_GetGaindB\n");
	   return false;
	}
	this	-> LMS_SetLPFBW = (pfn_LMS_SetLPFBW)
	                    GETPROCADDRESS (Handle, "LMS_SetLPFBW");
	if (this -> LMS_SetLPFBW == nullptr) {
	   fprintf (stderr, "could not find LMS_SetLPFBW\n");
	   return false;
	}
	this	-> LMS_GetLPFBW = (pfn_LMS_GetLPFBW)
	                    GETPROCADDRESS (Handle, "LMS_GetLPFBW");
	if (this -> LMS_GetLPFBW == nullptr) {
	   fprintf (stderr, "could not find LMS_GetLPFBW\n");
	   return false;
	}
	this	-> LMS_Calibrate = (pfn_LMS_Calibrate)
	                    GETPROCADDRESS (Handle, "LMS_Calibrate");
	if (this -> LMS_Calibrate == nullptr) {
	   fprintf (stderr, "could not find LMS_Calibrate\n");
	   return false;
	}
	this	-> LMS_SetupStream = (pfn_LMS_SetupStream)
	                    GETPROCADDRESS (Handle, "LMS_SetupStream");
	if (this -> LMS_SetupStream == nullptr) {
	   fprintf (stderr, "could not find LMS_SetupStream\n");
	   return false;
	}
	this	-> LMS_DestroyStream = (pfn_LMS_DestroyStream)
	                    GETPROCADDRESS (Handle, "LMS_DestroyStream");
	if (this -> LMS_DestroyStream == nullptr) {
	   fprintf (stderr, "could not find LMS_DestroyStream\n");
	   return false;
	}
	this	-> LMS_StartStream = (pfn_LMS_StartStream)
	                    GETPROCADDRESS (Handle, "LMS_StartStream");
	if (this -> LMS_StartStream == nullptr) {
	   fprintf (stderr, "could not find LMS_StartStream\n");
	   return false;
	}
	this	-> LMS_StopStream = (pfn_LMS_StopStream)
	                    GETPROCADDRESS (Handle, "LMS_StopStream");
	if (this -> LMS_StopStream == nullptr) {
	   fprintf (stderr, "could not find LMS_StopStream\n");
	   return false;
	}
	this	-> LMS_RecvStream = (pfn_LMS_RecvStream)
	                    GETPROCADDRESS (Handle, "LMS_RecvStream");
	if (this -> LMS_RecvStream == nullptr) {
	   fprintf (stderr, "could not find LMS_RecvStream\n");
	   return false;
	}
	this	-> LMS_GetStreamStatus = (pfn_LMS_GetStreamStatus)
	                    GETPROCADDRESS (Handle, "LMS_GetStreamStatus");
	if (this -> LMS_GetStreamStatus == nullptr) {
	   fprintf (stderr, "could not find LMS_GetStreamStatus\n");
	   return false;
	}

	return true;
}

void	LimeHandler::set_xmlDump () {
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

bool	LimeHandler::setup_xmlDump () {
QTime	theTime;
QDate	theDate;
QString saveDir = limeSettings -> value (sSettingSampleStorageDir,
                                           QDir::homePath ()). toString ();
        if ((saveDir != "") && (!saveDir. endsWith ("/")))
           saveDir += "/";

	QString channel		= limeSettings -> value ("channel", "xx").
	                                                      toString ();
	QString timeString      = theDate. currentDate (). toString () + "-" +
	                          theTime. currentTime (). toString ();
	for (i32 i = 0; i < timeString. length (); i ++)
	if (!isValid (timeString. at (i)))
	   timeString. replace (i, 1, '-');
        QString suggestedFileName =
                    saveDir + "limeSDR" + "-" + channel + "-" + timeString;
	QString fileName =
	           QFileDialog::getSaveFileName (nullptr,
	                                         tr ("Save file ..."),
	                                         suggestedFileName + ".uff",
	                                         tr ("Xml (*.uff)"));
        fileName        = QDir::toNativeSeparators (fileName);
        xmlDumper	= fopen (fileName. toUtf8(). data(), "w");
	if (xmlDumper == nullptr)
	   return false;
	
	xmlWriter	= new XmlFileWriter (xmlDumper,
                                  bitDepth	(),
                                  "int16",
                                  2048000,
                                  getVFOFrequency (),
                                  "lime",
                                  "1",
                                  recorderVersion);
	dumping. store (true);

	QString dumper	= QDir::fromNativeSeparators (fileName);
	i32 x		= dumper. lastIndexOf ("/");
	saveDir		= dumper. remove (x, dumper. size () - x);
	limeSettings -> setValue (sSettingSampleStorageDir, saveDir);
	return true;
}

void	LimeHandler::close_xmlDump () {
	if (xmlDumper == nullptr)	// this can happen !!
	   return;
	dumping. store (false);
	usleep (1000);
	xmlWriter	-> computeHeader ();
	delete xmlWriter;
	fclose (xmlDumper);
	xmlDumper	= nullptr;
}

void	LimeHandler::show	() {
	myFrame. show ();
}

void	LimeHandler::hide	() {
	myFrame. hide ();
}

bool	LimeHandler::isHidden	() {
	return myFrame. isHidden ();
}

void	LimeHandler::record_gainSettings	(i32 key) {
i32 gainValue	= gainSelector -> value ();
QString theValue	= QString::number (gainValue);

	limeSettings	-> beginGroup ("limeSettings");
        limeSettings	-> setValue (QString::number (key), theValue);
        limeSettings	-> endGroup ();
}

void	LimeHandler::update_gainSettings	(i32 key) {
i32	gainValue;

	limeSettings	-> beginGroup ("limeSettings");
        gainValue	= limeSettings -> value (QString::number (key), -1). toInt ();
        limeSettings	-> endGroup ();

	if (gainValue == -1)
	   return;

	gainSelector	-> blockSignals (true);
	new_gainValue (gainValue);
	while (gainSelector -> value () != gainValue)
	   usleep (1000);
	actualGain	-> display (gainValue);
	gainSelector	-> blockSignals (false);
}

bool LimeHandler::isFileInput()
{
  return false;
}

