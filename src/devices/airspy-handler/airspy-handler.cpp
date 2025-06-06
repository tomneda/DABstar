
/**
 *  IW0HDV Extio
 *
 *  Copyright 2015 by Andrea Montefusco IW0HDV
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 * @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
 *
 *	recoding, taking parts and extending for the airspyHandler interface
 *	for the Qt-DAB program
 *	jan van Katwijk
 *	Lazy Chair Computing
 */

#include	<QPoint>
#include	<QFileDialog>
#include	<QTime>
#include	<QDate>
#include	"airspy-handler.h"
#include	"airspyselect.h"
#include	"xml-filewriter.h"
#include	"device-exceptions.h"

static
const	i32	EXTIO_NS = 8192;
static
const	i32	EXTIO_BASE_TYPE_SIZE = sizeof (f32);

AirspyHandler::AirspyHandler(QSettings *s, QString recorderVersion):
                             myFrame(nullptr),
                             _I_Buffer(4 * 1024 * 1024)
{
	i32	result, i;
	i32	distance = 1000000;
	std::vector <u32> sampleRates;
	u32 samplerateCount;

	this->airspySettings = s;
	this->recorderVersion = recorderVersion;

	airspySettings->beginGroup("airspySettings");
	i32	x = airspySettings->value("position-x", 100).toInt();
	i32	y = airspySettings->value("position-y", 100).toInt();
	airspySettings->endGroup ();
	setupUi (&myFrame);
	myFrame.move (QPoint (x, y));
	myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
	myFrame.show		();
//
//	Since we have different tabs, with different sliders for
//	gain setting, restoring the settings is a tedious task
	airspySettings->beginGroup("airspySettings");
	i32 tab	= airspySettings->value("tabSettings", 2).toInt();
	airspySettings->endGroup();
    tabWidget->setCurrentIndex(tab);
	restore_gainSliders  (200, tab);

	theFilter		= nullptr;
	device			= nullptr;
	serialNumber		= 0;

#ifdef	__MINGW32__
	const char *libraryString = "airspy.dll";
#else
	const char *libraryString = "libairspy";
#endif
	phandle = new QLibrary(libraryString);
	phandle->load();

	if (!phandle->isLoaded())
	{
		throw (std_exception_string ("failed to open " + std::string (libraryString)));
	}

	if (!load_airspyFunctions())
	{
	   fprintf(stderr, "problem in loading functions\n");
	   throw(std_exception_string("one or more library functions could not be loaded"));
	}

	strcpy (serial,"");
	result = this->my_airspy_init();
	if (result != AIRSPY_SUCCESS)
	{
	   throw(std_exception_string(my_airspy_error_name((airspy_error)result)));
	}

	u64 deviceList[4];
	i32	deviceIndex;
	i32 numofDevs = my_airspy_list_devices (deviceList, 4);
	fprintf (stderr, "we have %d devices\n", numofDevs);
	if (numofDevs == 0)
	{
	   fprintf(stderr, "No devices found\n");
	   throw(std_exception_string("No airspy device was detected"));
	}

	if (numofDevs > 1)
	{
        airspySelect deviceSelector;
        for (deviceIndex = 0; deviceIndex < (i32)numofDevs; deviceIndex ++)
        {
            deviceSelector.addtoList(QString::number(deviceList[deviceIndex]));
        }
        deviceIndex = deviceSelector.QDialog::exec();
    }
	else
	   deviceIndex = 0;

	result = my_airspy_open(&device, deviceList[deviceIndex]);
	if (result != AIRSPY_SUCCESS)
	{
	   throw(std_exception_string(my_airspy_error_name((airspy_error)result)));
	}

	(void)my_airspy_set_sample_type(device, AIRSPY_SAMPLE_INT16_IQ);
	(void)my_airspy_get_samplerates(device, &samplerateCount, 0);
	fprintf(stderr, "%d samplerates are supported\n", samplerateCount);
	sampleRates.resize(samplerateCount);
	my_airspy_get_samplerates(device, sampleRates.data(), samplerateCount);

	selectedRate = 0;
	for (i = 0; i < (i32)samplerateCount; i ++)
	{
	    fprintf(stderr, "%d \n", sampleRates [i]);
	    if (abs((i32)sampleRates[i] - 2048000) < distance)
	    {
	        distance = abs((i32)sampleRates [i] - 2048000);
	        selectedRate = sampleRates [i];
	    }
	}

	if (selectedRate == 0)
	{
	   throw(std_exception_string("Cannot handle the samplerates"));
	}
	else
	   fprintf(stderr, "selected samplerate = %d\n", selectedRate);

	airspySettings->beginGroup("airspySettings");
    currentDepth = airspySettings->value("filterDepth", 5).toInt();
    airspySettings->endGroup();

    filterDepth->setValue(currentDepth);
	theFilter = new LowPassFIR(currentDepth, 1560000 / 2, selectedRate);
	filtering = false;
	result = my_airspy_set_samplerate (device, selectedRate);
	if (result != AIRSPY_SUCCESS)
	{
       throw(std_exception_string(my_airspy_error_name ((enum airspy_error)result)));
	}


//	The sizes of the mapTables follow from the input and output rate
//	(selectedRate / 1000) vs (2048000 / 1000)
//	so we end up with buffers with 1 msec content
	convBufferSize		= selectedRate / 1000;
	for (i = 0; i < 2048; i ++)
	{
	   f32 inVal	= f32 (selectedRate / 1000);
	   mapTable_int [i]	= i32 (floor (i * (inVal / 2048.0)));
	   mapTable_float [i]	= i * (inVal / 2048.0) - mapTable_int [i];
	}
	convIndex	= 0;
	convBuffer.resize (convBufferSize + 1);
//
	restore_gainSettings (tab);
	connect (linearitySlider, SIGNAL (valueChanged (int)),
	         this, SLOT (set_linearity (int)));
	connect (sensitivitySlider, SIGNAL (valueChanged (int)),
	         this, SLOT (set_sensitivity (int)));
	connect (lnaSlider, SIGNAL (valueChanged (int)),
	         this, SLOT (set_lna_gain (int)));
	connect (vgaSlider, SIGNAL (valueChanged (int)),
	         this, SLOT (set_vga_gain (int)));
	connect (mixerSlider, SIGNAL (valueChanged (int)),
	         this, SLOT (set_mixer_gain (int)));
	connect (lnaButton, SIGNAL (stateChanged (int)),
	         this, SLOT (set_lna_agc (int)));
	connect (mixerButton, SIGNAL (stateChanged (int)),
	         this, SLOT (set_mixer_agc (int)));
	connect (biasButton, SIGNAL (stateChanged (int)),
	         this, SLOT (set_rf_bias (int)));
	connect (tabWidget, SIGNAL (currentChanged (int)),
	         this, SLOT (switch_tab (int)));
	connect (dumpButton, SIGNAL (clicked ()),
	         this, SLOT (set_xmlDump ()));
	connect (filterSelector, SIGNAL (stateChanged (int)),
	         this, SLOT (set_filter (int)));
	connect (this, SIGNAL (new_tabSetting (int)),
	         tabWidget, SLOT (setCurrentIndex (int)));
//
	displaySerial->setText (getSerial());
	running.store (false);
	my_airspy_set_rf_bias (device, rf_bias ? 1 : 0);

	dumping.store (false);
	xmlDumper	= nullptr;
}

AirspyHandler::~AirspyHandler ()
{
	stopReader ();
	myFrame.hide ();
	filtering	= false;
	airspySettings->beginGroup ("airspySettings");
    airspySettings->setValue ("position-x", myFrame.pos ().x ());
    airspySettings->setValue ("position-y", myFrame.pos ().y ());

	airspySettings->setValue ("tabSettings",
	                                   tabWidget->currentIndex ());
	airspySettings->endGroup ();
	if (device != nullptr)
	{
	   i32 result = my_airspy_stop_rx (device);
	   if (result != AIRSPY_SUCCESS)
	   {
	      printf ("my_airspy_stop_rx() failed: %s (%d)\n",
	             my_airspy_error_name((airspy_error)result), result);
	   }

	   result = my_airspy_close (device);
	   if (result != AIRSPY_SUCCESS)
	   {
	      printf ("airspy_close() failed: %s (%d)\n",
	             my_airspy_error_name((airspy_error)result), result);
	   }
	}
	if (theFilter != nullptr)
	   delete theFilter;
	my_airspy_exit();
    if(phandle) delete(phandle);
}

void	AirspyHandler::setVFOFrequency (i32 nf)
{
	i32 result = my_airspy_set_freq (device, nf);

	vfoFrequency = nf;
	if (result != AIRSPY_SUCCESS) {
	   printf ("my_airspy_set_freq() failed: %s (%d)\n",
	            my_airspy_error_name((airspy_error)result), result);
	}
}

i32	AirspyHandler::getVFOFrequency()
{
	return vfoFrequency;
}

i32	AirspyHandler::defaultFrequency()
{
	return kHz (220000);
}

void	AirspyHandler::set_filter	(i32 c)
{
	(void)c;
	filtering = filterSelector->isChecked ();
//	fprintf (stderr, "filter set %s\n", filtering ? "on" : "off");
}

bool	AirspyHandler::restartReader	(i32 freq)
{
	i32	result;
	//i32	bufSize	= EXTIO_NS * EXTIO_BASE_TYPE_SIZE * 2;

	if (running.load())
	   return true;

	vfoFrequency	= freq;
	airspySettings->beginGroup ("airspySettings");
	QString key = "tabSettings-" + QString::number (freq / MHz (1));
	i32 tab	= airspySettings->value (key, 0).toInt ();
	airspySettings->endGroup ();
	tabWidget->blockSignals (true);
    new_tabSetting  (tab);
    while (tabWidget->currentIndex () != tab)
       usleep (1000);
    tabWidget->blockSignals (false);
//
//	sliders are now set,
	result = my_airspy_set_freq (device, freq);

	if (result != AIRSPY_SUCCESS)
	{
	   printf ("my_airspy_set_freq() failed: %s (%d)\n",
	            my_airspy_error_name((airspy_error)result), result);
	}
	_I_Buffer.flush_ring_buffer();
	result = my_airspy_set_sample_type (device, AIRSPY_SAMPLE_INT16_IQ);
//	result = my_airspy_set_sample_type (device, AIRSPY_SAMPLE_FLOAT32_IQ);
	if (result != AIRSPY_SUCCESS)
	{
	   printf ("my_airspy_set_sample_type() failed: %s (%d)\n",
	            my_airspy_error_name ((airspy_error)result), result);
	   return false;
	}

	result = my_airspy_start_rx (device, (airspy_sample_block_cb_fn)callback, this);
	if (result != AIRSPY_SUCCESS)
	{
	   printf ("my_airspy_start_rx() failed: %s (%d)\n",
	         my_airspy_error_name((airspy_error)result), result);
	   return false;
	}

//
//	Hier moeten we de tab en gain weer zetten
	restore_gainSliders (freq / MHz (1), tab);
	restore_gainSettings (tab);
//
	running.store (true);
	return true;
}

void	AirspyHandler::stopReader()
{
	i32	result;

	if (!running.load())
	   return;

	close_xmlDump ();
	airspySettings->beginGroup ("airspySettings");
	QString key = "tabSettings-" +
	            QString::number (getVFOFrequency () / MHz (1));
	airspySettings->setValue (key, tabWidget->currentIndex ());
	airspySettings->endGroup ();
	record_gainSettings (getVFOFrequency () / MHz (1),
	                           tabWidget->currentIndex ());
	result = my_airspy_stop_rx (device);

	if (result != AIRSPY_SUCCESS )
	   printf ("my_airspy_stop_rx() failed: %s (%d)\n",
	          my_airspy_error_name ((airspy_error)result), result);
	running.store (false);
	resetBuffer ();
}
//
//	Directly copied from the airspy extio dll from Andrea Montefusco
i32 AirspyHandler::callback (airspy_transfer* transfer)
{
	AirspyHandler *p;

	if (!transfer)
	   return 0;		// should not happen
	p = static_cast<AirspyHandler *> (transfer->ctx);

// we read  AIRSPY_SAMPLE_INT16_IQ:
	i32 bytes_to_write = transfer->sample_count * sizeof (i16) * 2;
	u8 *pt_rx_buffer   = (u8 *)transfer->samples;
	p->data_available (pt_rx_buffer, bytes_to_write);
	return 0;
}

//	called from AIRSPY data callback
//	2*2 = 4 bytes for sample, as per AirSpy USB data stream format
//	we do the rate conversion here, read in groups of 2 * xxx samples
//	and transform them into groups of 2 * 512 samples
i32 	AirspyHandler::data_available (void *buf, i32 buf_size)
{
	i16	*sbuf	= (i16 *)buf;
	i32 nSamples	= buf_size / (sizeof (i16) * 2);
	cf32 temp [2048];
	i32  i, j;

	if (dumping.load ())
	   xmlWriter->add ((std::complex<i16> *)sbuf, nSamples);
	if (filtering)
	{
	   if (filterDepth->value () != currentDepth)
	   {
	      airspySettings->beginGroup ("airspySettings");
	      airspySettings->setValue ("filterDepth",
	                                      filterDepth->value ());
	      airspySettings	->endGroup();
	      currentDepth = filterDepth->value ();
	      theFilter->resize (currentDepth);
	   }
	   for (i = 0; i < nSamples; i ++)
	   {
	      convBuffer [convIndex ++] = theFilter->Pass (
	                                     cf32 (
	                                        sbuf [2 * i] / (f32)2048,
	                                        sbuf [2 * i + 1] / (f32)2048)
	                                     );
	      if (convIndex > convBufferSize)
	      {
	         for (j = 0; j < 2048; j ++)
	         {
	            i16  inpBase	= mapTable_int [j];
	            f32    inpRatio	= mapTable_float [j];
	            temp [j]	= convBuffer [inpBase + 1] * inpRatio +
	                        convBuffer [inpBase] * (1 - inpRatio);
	         }

	         _I_Buffer.put_data_into_ring_buffer (temp, 2048);
//
//	shift the sample at the end to the beginning, it is needed
//	as the starting sample for the next time
	         convBuffer [0] = convBuffer [convBufferSize];
	         convIndex = 1;
	      }
	   }
	}
	else
	for (i = 0; i < nSamples; i ++)
	{
	   convBuffer [convIndex ++] = cf32 (
	                                     sbuf [2 * i] / (f32)2048,
	                                     sbuf [2 * i + 1] / (f32)2048);
	   if (convIndex > convBufferSize)
	   {
	      for (j = 0; j < 2048; j ++)
	      {
	         i16  inpBase	= mapTable_int [j];
	         f32    inpRatio	= mapTable_float [j];
	         temp [j]	= convBuffer [inpBase + 1] * inpRatio +
                      convBuffer [inpBase] * (1 - inpRatio);
	      }

	      _I_Buffer.put_data_into_ring_buffer (temp, 2048);
//
//	shift the sample at the end to the beginning, it is needed
//	as the starting sample for the next time
	      convBuffer [0] = convBuffer [convBufferSize];
	      convIndex = 1;
	   }
	}
	return 0;
}
//
const char *AirspyHandler::getSerial()
{
	airspy_read_partid_serialno_t read_partid_serialno;
	i32 result = my_airspy_board_partid_serialno_read (device,
	                                          &read_partid_serialno);
	if (result != AIRSPY_SUCCESS)
	{
	   printf ("failed: %s (%d)\n",
	         my_airspy_error_name ((airspy_error)result), result);
	   return "UNKNOWN";
	}
	else
	{
	   snprintf (serial, sizeof(serial), "%08X%08X",
	             read_partid_serialno.serial_no [2],
	             read_partid_serialno.serial_no [3]);
	}
	return serial;
}
//
//	not used here
i32	AirspyHandler::open()
{
//i32 result = my_airspy_open (&device);
//
//	if (result != AIRSPY_SUCCESS) {
//	   printf ("airspy_open() failed: %s (%d)\n",
//	          my_airspy_error_name((airspy_error)result), result);
//	   return -1;
//	} else {
//	   return 0;
//	}
	return 0;
}

//
//	These functions are added for the SDR-J interface
void	AirspyHandler::resetBuffer	()
{
	_I_Buffer.flush_ring_buffer	();
}

i16	AirspyHandler::bitDepth		()
{
	return 13;
}

i32	AirspyHandler::getSamples (cf32 *v, i32 size)
{

	return _I_Buffer.get_data_from_ring_buffer (v, size);
}

i32	AirspyHandler::Samples		() {
	return _I_Buffer.get_ring_buffer_read_available();
}

const char* AirspyHandler::board_id_name()
{
	u8 bid;

	if (my_airspy_board_id_read (device, &bid) == AIRSPY_SUCCESS)
	   return my_airspy_board_id_name ((airspy_board_id)bid);
	else
	   return "UNKNOWN";
}

bool	AirspyHandler::load_airspyFunctions()
{
//	link the required procedures
	my_airspy_init	= (pfn_airspy_init)phandle->resolve("airspy_init");
	if (my_airspy_init == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_init\n");
	   return false;
	}

	my_airspy_exit	= (pfn_airspy_exit)phandle->resolve("airspy_exit");
	if (my_airspy_exit == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_exit\n");
	   return false;
	}

	my_airspy_list_devices = (pfn_airspy_list_devices)
	                         phandle->resolve("airspy_list_devices");
	if (my_airspy_list_devices == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_list_devices\n");
	   return false;
	}

	my_airspy_open	= (pfn_airspy_open)phandle->resolve("airspy_open");
	if (my_airspy_open == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_open\n");
	   return false;
	}

	my_airspy_close	= (pfn_airspy_close)phandle->resolve("airspy_close");
	if (my_airspy_close == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_close\n");
	   return false;
	}

	my_airspy_get_samplerates = (pfn_airspy_get_samplerates)
	                            phandle->resolve("airspy_get_samplerates");
	if (my_airspy_get_samplerates == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_get_samplerates\n");
	   return false;
	}

	my_airspy_set_samplerate = (pfn_airspy_set_samplerate)
	                           phandle->resolve("airspy_set_samplerate");
	if (my_airspy_set_samplerate == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_set_samplerate\n");
	   return false;
	}

	my_airspy_start_rx = (pfn_airspy_start_rx)phandle->resolve("airspy_start_rx");
	if (my_airspy_start_rx == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_start_rx\n");
	   return false;
	}

	my_airspy_stop_rx = (pfn_airspy_stop_rx)phandle->resolve("airspy_stop_rx");
	if (my_airspy_stop_rx == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_stop_rx\n");
	   return false;
	}

	my_airspy_set_sample_type = (pfn_airspy_set_sample_type)
	                            phandle->resolve("airspy_set_sample_type");
	if (my_airspy_set_sample_type == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_set_sample_type\n");
	   return false;
	}

	my_airspy_set_freq = (pfn_airspy_set_freq)phandle->resolve("airspy_set_freq");
	if (my_airspy_set_freq == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_set_freq\n");
	   return false;
	}

	my_airspy_set_lna_gain = (pfn_airspy_set_lna_gain)
	                         phandle->resolve("airspy_set_lna_gain");
	if (my_airspy_set_lna_gain == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_set_lna_gain\n");
	   return false;
	}

	my_airspy_set_mixer_gain = (pfn_airspy_set_mixer_gain)
	                           phandle->resolve("airspy_set_mixer_gain");
	if (my_airspy_set_mixer_gain == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_set_mixer_gain\n");
	   return false;
	}

	my_airspy_set_vga_gain = (pfn_airspy_set_vga_gain)
	                         phandle->resolve("airspy_set_vga_gain");
	if (my_airspy_set_vga_gain == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_set_vga_gain\n");
	   return false;
	}

	my_airspy_set_linearity_gain = (pfn_airspy_set_linearity_gain)
	                               phandle->resolve("airspy_set_linearity_gain");
	if (my_airspy_set_linearity_gain == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_set_linearity_gain\n");
	   fprintf (stderr, "You probably did install an old library\n");
	   return false;
	}

	my_airspy_set_sensitivity_gain = (pfn_airspy_set_sensitivity_gain)
	                                 phandle->resolve("airspy_set_sensitivity_gain");
	if (my_airspy_set_sensitivity_gain == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_set_sensitivity_gain\n");
	   fprintf (stderr, "You probably did install an old library\n");
	   return false;
	}

	my_airspy_set_lna_agc = (pfn_airspy_set_lna_agc)
	                        phandle->resolve("airspy_set_lna_agc");
	if (my_airspy_set_lna_agc == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_set_lna_agc\n");
	   return false;
	}

	my_airspy_set_mixer_agc	= (pfn_airspy_set_mixer_agc)
	                          phandle->resolve("airspy_set_mixer_agc");
	if (my_airspy_set_mixer_agc == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_set_mixer_agc\n");
	   return false;
	}

	my_airspy_set_rf_bias = (pfn_airspy_set_rf_bias)
	                        phandle->resolve("airspy_set_rf_bias");
	if (my_airspy_set_rf_bias == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_set_rf_bias\n");
	   return false;
	}

	my_airspy_error_name = (pfn_airspy_error_name)
	                       phandle->resolve("airspy_error_name");
	if (my_airspy_error_name == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_error_name\n");
	   return false;
	}

	my_airspy_board_id_read	= (pfn_airspy_board_id_read)
	                          phandle->resolve("airspy_board_id_read");
	if (my_airspy_board_id_read == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_board_id_read\n");
	   return false;
	}

	my_airspy_board_id_name	= (pfn_airspy_board_id_name)
	                          phandle->resolve("airspy_board_id_name");
	if (my_airspy_board_id_name == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_board_id_name\n");
	   return false;
	}

	my_airspy_board_partid_serialno_read =(pfn_airspy_board_partid_serialno_read)
	                       phandle->resolve("airspy_board_partid_serialno_read");
	if (my_airspy_board_partid_serialno_read == nullptr)
	{
	   fprintf (stderr, "Could not find airspy_board_partid_serialno_read\n");
	   return false;
	}

	return true;
}


i32	AirspyHandler::getBufferSpace()
{
	return _I_Buffer.get_ring_buffer_write_available ();
}

QString	AirspyHandler::deviceName()
{
	return "AIRspy";
}

void	AirspyHandler::set_xmlDump()
{
	if (xmlDumper == nullptr)
	{
	  if (setup_xmlDump ())
	      dumpButton->setText("writing");
	}
	else
	{
	   close_xmlDump ();
	   dumpButton	->setText ("Dump");
	}
}

static inline bool	isValid (QChar c)
{
	return c.isLetterOrNumber () || (c == '-');
}

bool	AirspyHandler::setup_xmlDump ()
{
	QTime	theTime;
	QDate	theDate;
	QString saveDir = airspySettings->value (sSettingSampleStorageDir,
                                           QDir::homePath ()).toString ();
    if ((saveDir != "") && (!saveDir.endsWith ("/")))
       saveDir += "/";
	QString channel	= airspySettings->value("channel", "xx").toString();
    QString timeString = theDate.currentDate().toString() + "-" +
	                     theTime.currentTime().toString();
	for (i32 i = 0; i < timeString.length (); i ++)
	   if (!isValid (timeString.at (i)))
	      timeString.replace (i, 1, "-");
    QString suggestedFileName =
                saveDir + "AIRspy" + "-" + channel + "-" + timeString;
	QString fileName =
	           QFileDialog::getSaveFileName (nullptr,
	                                         tr ("Save file ..."),
	                                         suggestedFileName + ".uff",
	                                         tr ("Xml (*.uff)"));
    fileName        = QDir::toNativeSeparators (fileName);
    xmlDumper	= fopen (fileName.toUtf8().data(), "w");
	if (xmlDumper == nullptr)
	   return false;

	xmlWriter = new XmlFileWriter(xmlDumper,
                                  12,
                                  "int16",
                                  selectedRate,
                                  getVFOFrequency (),
                                  "AIRspy",
                                  "I",
                                  recorderVersion);
	dumping.store (true);

	QString dumper	= QDir::fromNativeSeparators (fileName);
	i32 x		= dumper.lastIndexOf ("/");
    saveDir		= dumper.remove (x, dumper.size () - x);
    airspySettings	->setValue (sSettingSampleStorageDir, saveDir);
	return true;
}

void	AirspyHandler::close_xmlDump ()
{
	if (xmlDumper == nullptr)	// this can happen !!
	   return;
	dumping.store (false);
	usleep (1000);
	xmlWriter	->computeHeader ();
	delete xmlWriter;
	fclose (xmlDumper);
	xmlDumper	= nullptr;
}

void	AirspyHandler::show	()
{
	myFrame.show ();
}

void	AirspyHandler::hide	()
{
	myFrame.hide	();
}

bool	AirspyHandler::isHidden	()
{
	return myFrame.isHidden ();
}
//
//	gain settings are maintained on a per-channel and per tab base,
//	Values are recorded on both switching tabs and changing channels
void	AirspyHandler::record_gainSettings	(i32 freq, i32 tab)
{
	QString	res;
	QString key;

    airspySettings	->beginGroup ("airspySettings");
	key = QString::number (freq) + "-" + QString::number (tab);
	switch (tab)
	{
	   case 0:	// sensitity screen is on
	      res = QString::number (sensitivitySlider->value ());
	      break;
	   case 1:	// linearity screen is on
	      res = QString::number (linearitySlider->value ());
	      break;
	   case 3:	// classic screen
	   default:
	      res = QString::number (vgaSlider->value ());
	      res = res + ":" + QString::number (mixerSlider->value ());
	      res = res + ":" + QString::number (lnaSlider->value ());
	      break;
	}

	res	= res + ":" + QString::number (lnaButton->isChecked ());
	res	= res + ":" + QString::number (mixerButton->isChecked ());
	res	= res + ":" + QString::number (biasButton->isChecked ());

	airspySettings	->setValue (key, res);
    airspySettings	->endGroup ();
}
//
//	When starting a channel, the gain sliders from the previous
//	time that channel was the current channel, are restored
//	Note that the device settings are NOT yet updated
void	AirspyHandler::restore_gainSliders	(i32 freq, i32 tab)
{
	i32	lna	= 0;
	i32	mixer	= 0;
	i32	bias	= 0;
	//i32	newTab	= 0;
	QString key	= QString::number (freq) + "-" + QString::number (tab);

	airspySettings->beginGroup ("airspySettings");
	QString gainValues = airspySettings->value(key, "").toString();
	airspySettings->endGroup ();
	if (gainValues == "") // we create default values
    {
       if ((tab == 0) || (tab == 1))
       {
          gainValues = "10:0:0:0";
       }
       else
       {
          gainValues = "10:10:10:0:0:0";
       }
    }
    QStringList list = gainValues.split (":");
	switch (tab)
	{
	   case 0:
	      disconnect (sensitivitySlider, SIGNAL (valueChanged (int)),
	                  this, SLOT (set_sensitivity (int)));
	      sensitivitySlider->setValue (list.at (0).toInt ());
	      connect (sensitivitySlider, SIGNAL (valueChanged (int)),
	               this, SLOT (set_sensitivity (int)));
	      lna	= list.at (1).toInt ();
	      mixer	= list.at (2).toInt ();
	      bias	= list.at (3).toInt ();
	      break;
	   case 1:
	      disconnect (linearitySlider, SIGNAL (valueChanged (int)),
	                  this, SLOT (set_linearity (int)));
	      linearitySlider->setValue (list.at (0).toInt ());
	      connect (linearitySlider, SIGNAL (valueChanged (int)),
	               this, SLOT (set_linearity (int)));
	      lna	= list.at (1).toInt ();
	      mixer	= list.at (2).toInt ();
	      bias	= list.at (3).toInt ();
	      break;

	   default:	// classic view
	      disconnect (vgaSlider, SIGNAL (valueChanged (int)),
	                  this, SLOT (set_vga_gain (int)));
	      disconnect (mixerSlider, SIGNAL (valueChanged (int)),
	                  this, SLOT (set_mixer_gain (int)));
	      disconnect (lnaSlider, SIGNAL (valueChanged (int)),
	                  this, SLOT (set_lna_gain (int)));
	      vgaSlider->setValue (list.at (0).toInt ());
	      mixerSlider->setValue (list.at (1).toInt ());
	      lnaSlider->setValue (list.at (2).toInt ());
	      connect (vgaSlider, SIGNAL (valueChanged (int)),
	               this, SLOT (set_vga_gain (int)));
	      connect (mixerSlider, SIGNAL (valueChanged (int)),
	               this, SLOT (set_mixer_gain (int)));
	      connect (lnaSlider, SIGNAL (valueChanged (int)),
	               this, SLOT (set_lna_gain (int)));
	      lna	= list.at (3).toInt ();
	      mixer	= list.at (4).toInt ();
	      bias	= list.at (5).toInt ();
	}
//
//	Now the agc settings
	disconnect (lnaButton, SIGNAL (stateChanged (int)),
	            this, SLOT (set_lna_agc (int)));
	disconnect (mixerButton, SIGNAL (stateChanged (int)),
	            this, SLOT (set_mixer_agc (int)));
	disconnect (biasButton, SIGNAL (stateChanged (int)),
	            this, SLOT (set_rf_bias (int)));
	if (lna != 0)
	   lnaButton->setChecked (true);
	if (mixer != 0)
	   mixerButton->setChecked (true);
	if (bias != 0)
	   biasButton->setChecked (true);
	connect (lnaButton, SIGNAL (stateChanged (int)),
	         this, SLOT (set_lna_agc (int)));
	connect (mixerButton, SIGNAL (stateChanged (int)),
	         this, SLOT (set_mixer_agc (int)));
	connect (biasButton, SIGNAL (stateChanged (int)),
	         this, SLOT (set_rf_bias (int)));
}

void	AirspyHandler::restore_gainSettings	(i32 tab)
{
	switch (tab)
	{
	   case 0:	//sensitivity
	      set_sensitivity (sensitivitySlider->value ());
	      break;
	   case 1:	// linearity
	      set_linearity (linearitySlider->value ());
	      break;
	   case 2:	// classic view
	   default:
	      set_lna_gain	(lnaSlider	->value ());
	      set_mixer_gain(mixerSlider	->value ());
	      set_vga_gain	(vgaSlider	->value ());
	      break;
	}
	if (lnaButton->isChecked ())
	   set_lna_agc (1);
	if (mixerButton->isChecked ())
	   set_mixer_agc (1);
	if (biasButton->isChecked ())
	   set_rf_bias (1);
}

void	AirspyHandler::switch_tab (i32 t)
{
	record_gainSettings (getVFOFrequency () / MHz (1),
                         tabWidget->currentIndex ());
	tabWidget     ->blockSignals (true);
	new_tabSetting	(t);
    while (tabWidget->currentIndex () != t)
       usleep (1000);
    tabWidget     ->blockSignals (false);
	airspySettings	->beginGroup ("airspySettings");
	airspySettings	->setValue ("tabSettings", t);
	airspySettings	->endGroup ();
	restore_gainSliders (getVFOFrequency () / MHz (1), t);
	restore_gainSettings (t);
}

#define GAIN_COUNT (22)

u8 airspy_linearity_vga_gains[GAIN_COUNT] = { 13, 12, 11, 11, 11, 11, 11, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 8, 7, 6, 5, 4 };
u8 airspy_linearity_mixer_gains[GAIN_COUNT] = { 12, 12, 11, 9, 8, 7, 6, 6, 5, 0, 0, 1, 0, 0, 2, 2, 1, 1, 1, 1, 0, 0 };
u8 airspy_linearity_lna_gains[GAIN_COUNT] = { 14, 14, 14, 13, 12, 10, 9, 9, 8, 9, 8, 6, 5, 3, 1, 0, 0, 0, 0, 0, 0, 0 };
u8 airspy_sensitivity_vga_gains[GAIN_COUNT] = { 13, 12, 11, 10, 9, 8, 7, 6, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4 };
u8 airspy_sensitivity_mixer_gains[GAIN_COUNT] = { 12, 12, 12, 12, 11, 10, 10, 9, 9, 8, 7, 4, 4, 4, 3, 2, 2, 1, 0, 0, 0, 0 };
u8 airspy_sensitivity_lna_gains[GAIN_COUNT] = { 14, 14, 14, 14, 14, 14, 14, 14, 14, 13, 12, 12, 9, 9, 8, 7, 6, 5, 3, 2, 1, 0 };

void	AirspyHandler::set_linearity (i32 value)
{
	i32	result = my_airspy_set_linearity_gain (device, value);
	i32	temp;
	if (result != AIRSPY_SUCCESS)
	{
	   printf ("airspy_set_lna_gain() failed: %s (%d)\n",
	            my_airspy_error_name ((airspy_error)result), result);
	   return;
	}
	linearityDisplay->display (value);
	temp = airspy_linearity_lna_gains [GAIN_COUNT - 1 - value];
	linearity_lnaDisplay->display (temp);
	temp = airspy_linearity_mixer_gains [GAIN_COUNT - 1 - value];
	linearity_mixerDisplay->display (temp);
	temp = airspy_linearity_vga_gains [GAIN_COUNT - 1 - value];
	linearity_vgaDisplay->display (temp);
}

void	AirspyHandler::set_sensitivity (i32 value)
{
	i32	result = my_airspy_set_sensitivity_gain (device, value);
	i32	temp;
	if (result != AIRSPY_SUCCESS)
	{
	   printf ("airspy_set_mixer_gain() failed: %s (%d)\n",
	            my_airspy_error_name ((airspy_error)result), result);
	   return;
	}
	sensitivityDisplay->display (value);
	temp = airspy_sensitivity_lna_gains [GAIN_COUNT - 1 - value];
	sensitivity_lnaDisplay->display (temp);
	temp = airspy_sensitivity_mixer_gains [GAIN_COUNT - 1 - value];
	sensitivity_mixerDisplay->display (temp);
	temp = airspy_sensitivity_vga_gains [GAIN_COUNT - 1 - value];
	sensitivity_vgaDisplay->display (temp);
}

//	Original functions from the airspy extio dll
/* Parameter value shall be between 0 and 15 */
void	AirspyHandler::set_lna_gain (i32 value)
{
	i32 result = my_airspy_set_lna_gain (device, lnaGain = value);

	if (result != AIRSPY_SUCCESS)
	{
	   printf ("airspy_set_lna_gain() failed: %s (%d)\n",
	            my_airspy_error_name ((airspy_error)result), result);
	}
	else
	   lnaDisplay	->display (value);
}

/* Parameter value shall be between 0 and 15 */
void	AirspyHandler::set_mixer_gain (i32 value)
{
	i32 result = my_airspy_set_mixer_gain (device, mixerGain = value);

	if (result != AIRSPY_SUCCESS)
	{
	   printf ("airspy_set_mixer_gain() failed: %s (%d)\n",
	            my_airspy_error_name ((airspy_error)result), result);
	}
	else
	   mixerDisplay->display (value);
}

/* Parameter value shall be between 0 and 15 */
void	AirspyHandler::set_vga_gain (i32 value)
{
	i32 result = my_airspy_set_vga_gain (device, vgaGain = value);

	if (result != AIRSPY_SUCCESS)
	{
	   printf ("airspy_set_vga_gain() failed: %s (%d)\n",
	            my_airspy_error_name ((airspy_error)result), result);
	}
	else
	   vgaDisplay	->display (value);
}
//
//	agc's
/* Parameter value:
	0=Disable LNA Automatic Gain Control
	1=Enable LNA Automatic Gain Control
*/
void	AirspyHandler::set_lna_agc	(i32 dummy)
{
	(void)dummy;
	lna_agc	= lnaButton->isChecked ();
	i32 result = my_airspy_set_lna_agc (device, lna_agc ? 1 : 0);

	if (result != AIRSPY_SUCCESS)
	{
	   printf ("airspy_set_lna_agc() failed: %s (%d)\n",
	            my_airspy_error_name ((airspy_error)result), result);
	}
}

/* Parameter value:
	0=Disable MIXER Automatic Gain Control
	1=Enable MIXER Automatic Gain Control
*/
void	AirspyHandler::set_mixer_agc	(i32 dummy)
{
	(void)dummy;
	mixer_agc	= mixerButton->isChecked ();

	i32 result = my_airspy_set_mixer_agc (device, mixer_agc ? 1 : 0);

	if (result != AIRSPY_SUCCESS)
	{
	   printf ("airspy_set_mixer_agc() failed: %s (%d)\n",
	            my_airspy_error_name ((airspy_error)result), result);
	}
}


/* Parameter value shall be 0=Disable BiasT or 1=Enable BiasT */
void	AirspyHandler::set_rf_bias (i32 dummy)
{
	(void)dummy;
	rf_bias	= biasButton->isChecked ();
	i32 result = my_airspy_set_rf_bias (device, rf_bias ? 1 : 0);

	if (result != AIRSPY_SUCCESS)
	{
	   printf("airspy_set_rf_bias() failed: %s (%d)\n",
	           my_airspy_error_name ((airspy_error)result), result);
	}
}

bool AirspyHandler::isFileInput()
{
  return false;
}

