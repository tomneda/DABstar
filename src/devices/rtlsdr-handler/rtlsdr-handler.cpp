/*
 *    Copyright (C) 2013 .. 2017
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
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-SDR; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 	This particular driver is a very simple wrapper around the
 * 	librtlsdr. In order to keep things simple, we dynamically
 * 	load the dll (or .so).The librtlsdr is osmocom software and all rights
 * 	are greatly acknowledged
 */

#include	<QThread>
#include	<QFileDialog>
#include	<QTime>
#include	<QDate>
#include	<QDir>
#include	"rtlsdr-handler.h"
#include	"rtl-dongleselect.h"
#include	"rtl-sdr.h"
#include	"xml-filewriter.h"
#include	"device-exceptions.h"

#define	DEFAULT_FREQUENCY (kHz (220000))

#define	READLEN_DEFAULT	(4 * 8192)

static clock_t time1 = 0;

//	For the callback, we do need some environment which
//	is passed through the ctx parameter
//
//	This is the user-side call back function
//	ctx is the calling task
static void RTLSDRCallBack(uint8_t *buf, uint32_t len, void *ctx)
{
	clock_t time2;
    RtlSdrHandler *theStick	= (RtlSdrHandler *)ctx;

	if ((theStick == nullptr) || (len != READLEN_DEFAULT))
    {
	   fprintf(stderr, "%d \n", len);
	   return;
	}

	if (theStick->isActive.load())
    {
	    if (theStick->_I_Buffer.get_ring_buffer_write_available() < (signed)len / 2)
	    	fprintf(stderr, "xx? ");
	    (void)theStick->_I_Buffer.put_data_into_ring_buffer((std::complex<uint8_t> *)buf, len / 2);
		//fprintf(stderr, "put_data=%d\n", len/2);

		time2 = clock();
		if((time2 - time1) >= (CLOCKS_PER_SEC)) // >= 1s
		{
			int overload = (int)theStick->detect_overload(buf, len);
		  	//fprintf(stderr, "Overload=%d\n", overload);
			emit theStick->signal_timer(overload);
			time1 = time2;
		}

	}
}

//	for handling the events in libusb, we need a controlthread
//	whose sole purpose is to process the rtlsdr_read_async function
//	from the lib.
class	dll_driver : public QThread
{
private:
	RtlSdrHandler *theStick;
public:

	dll_driver(RtlSdrHandler *d)
    {
    	theStick = d;
    	start();
    }

	~dll_driver()
    {
    }

private:
    void run()
    {
    	(theStick->rtlsdr_read_async) (theStick->theDevice,
            (rtlsdr_read_async_cb_t)&RTLSDRCallBack,
            (void *)theStick,
            0,
            READLEN_DEFAULT);
	}
};

//
//	Our wrapper is a simple classs
RtlSdrHandler::RtlSdrHandler(QSettings *s,
	                         const QString &recorderVersion):
	                         _I_Buffer(8 * 1024 * 1024),
	                         myFrame(nullptr),
	                         theFilter(5, 1560000 / 2, INPUT_RATE)
{
    int16_t	deviceCount;
    int32_t	r;
    int16_t	deviceIndex;
    int16_t	i;
    int	k;
    QString	temp;
    char manufac[256], product[256], serial[256];

	for(i = 0; i < 256; i++)
		mapTable[i] = ((float)i - 127.38) / 128.0;
	rtlsdrSettings = s;
	this->recorderVersion = recorderVersion;
	rtlsdrSettings->beginGroup("rtlsdrSettings");
	int x = rtlsdrSettings->value("position-x", 100).toInt();
    int y = rtlsdrSettings->value("position-y", 100).toInt();
	filtering = rtlsdrSettings->value("filterSelector", 0).toInt();
	currentDepth = rtlsdrSettings->value("filterDepth", 5).toInt();
    rtlsdrSettings->endGroup();

    setupUi(&myFrame);
    myFrame.move(QPoint(x, y));
    myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
	myFrame.show();

	filterDepth->setValue(currentDepth);
	theFilter.resize(currentDepth);
    filterSelector->setChecked(filtering);

	inputRate = INPUT_RATE;
	workerHandle = nullptr;
	isActive.store(false);
#ifdef	__MINGW32__
	const char *libraryString = "librtlsdr.dll";
#elif __linux__
    const char *libraryString = "librtlsdr.so";
#elif __APPLE__
    const char *libraryString = "librtlsdr.dylib";
#endif
	phandle = new QLibrary(libraryString);
	phandle->load();

	if (!phandle->isLoaded())
	   throw(std_exception_string(std::string("failed to open ") + std::string(libraryString)));

	if (!load_rtlFunctions())
    {
	   delete(phandle);
	   throw(std_exception_string("could not load one or more library functions"));
	}

	//	Ok, from here we have the library functions accessible
	deviceCount = this->rtlsdr_get_device_count();
	if (deviceCount == 0)
    {
	   delete(phandle);
	   throw(std_exception_string("No rtlsdr device found"));
	}

	deviceIndex = 0;	// default
	if (deviceCount > 1)
    {
	   rtl_dongleSelect dongleSelector;
	   for (deviceIndex = 0; deviceIndex < deviceCount; deviceIndex ++)
       {
	      dongleSelector.addtoDongleList(rtlsdr_get_device_name(deviceIndex));
	   }
	   deviceIndex = dongleSelector.QDialog::exec();
	}

	//	OK, now open the hardware
	r = this->rtlsdr_open(&theDevice, deviceIndex);
	if (r < 0)
    {
	   delete phandle;
	   throw(std_exception_string("Opening rtlsdr device failed"));
	}
	deviceModel	= rtlsdr_get_device_name(deviceIndex);
	deviceVersion->setText(deviceModel);
	r = this->rtlsdr_set_sample_rate(theDevice, inputRate);
	if (r < 0)
    {
	   delete phandle;
	   throw(std_exception_string("Setting samplerate for rtlsdr failed"));
	}
	setVFOFrequency(DEFAULT_FREQUENCY);


	gainsCount = rtlsdr_get_tuner_gains(theDevice, nullptr);
	fprintf(stdout, "Supported gain values (%d): ", gainsCount);
	{
       int gains[gainsCount];
	   gainsCount = rtlsdr_get_tuner_gains(theDevice, gains);
	   for (i = gainsCount; i > 0; i--)
       {
	      fprintf(stdout, "%.1f ", gains[i - 1] / 10.0);
	      gainControl->addItem(QString::number(gains[i - 1] / 10.0));
	   }
	   fprintf(stdout, "\n");
	}

	int tuner_type = rtlsdr_get_tuner_type(theDevice);
	switch (tuner_type)
	{
	   case RTLSDR_TUNER_E4000:
	      tunerType->setText("E4000");
	      break;
	   case RTLSDR_TUNER_FC0012:
	      tunerType->setText("FC0012");
	      break;
	   case RTLSDR_TUNER_FC0013:
	      tunerType->setText("FC0013");
	      break;
	   case RTLSDR_TUNER_FC2580:
	      tunerType->setText("FC2580");
	      break;
	   case RTLSDR_TUNER_R820T:
	      tunerType->setText("R820T");
	      break;
	   case RTLSDR_TUNER_R828D:
	      tunerType->setText("R828D");
	      break;
	   default:
	      tunerType->setText("unknown");
	}

	//	See what the saved values are and restore the GUI settings
	rtlsdrSettings->beginGroup("rtlsdrSettings");
	temp = rtlsdrSettings->value("externalGain", "10").toString();
	k = gainControl->findText(temp);
    //qDebug() << "gain =" << temp << ", index =" << k;
	gainControl->setCurrentIndex(k != -1 ? k : gainsCount / 2);

    agcControl = rtlsdrSettings->value("autogain", 0).toInt();
	biasControl->setChecked(rtlsdrSettings->value("biasControl", 0).toInt());
	bandwidth->setValue(rtlsdrSettings->value("bandwidth", 0).toInt());
	ppm_correction->setValue(rtlsdrSettings->value("ppm_correction", 0.0).toDouble());
	rtlsdrSettings->endGroup();

	rtlsdr_get_usb_strings(theDevice, manufac, product, serial);
	fprintf(stdout, "%s %s %s\n", manufac, product, serial);
	product_display->setText(product);

	//	all values are set to previous values, now do the settings
	set_ExternalGain(gainControl->currentIndex());
    set_autogain(agcControl);
	set_ppmCorrection(ppm_correction->value());
	set_bandwidth(bandwidth->value());
	set_biasControl(biasControl->isChecked());
	enable_gainControl(agcControl);

	//	and attach the buttons/sliders to the actions
	connect(ppm_correction, SIGNAL(valueChanged(double)),
	        this, SLOT(set_ppmCorrection(double)));
	connect(gainControl, SIGNAL(activated(int)),
			this, SLOT(set_ExternalGain(int)));
	connect(hw_agc, SIGNAL(clicked()), this, SLOT(handle_hw_agc()));
	connect(sw_agc, SIGNAL(clicked()), this, SLOT(handle_sw_agc()));
	connect(manual, SIGNAL(clicked()), this, SLOT(handle_manual()));
	connect(biasControl, SIGNAL(stateChanged(int)),
	        this, SLOT(set_biasControl(int)));
	connect(filterSelector, SIGNAL(stateChanged(int)),
	        this, SLOT(set_filter(int)));
	connect(bandwidth, SIGNAL(valueChanged(int)),
	        this, SLOT(set_bandwidth(int)));
	connect(xml_dumpButton, SIGNAL(clicked()), this, SLOT(set_xmlDump()));
	connect(iq_dumpButton, SIGNAL(clicked()), this, SLOT(set_iqDump()));
    connect(this, &RtlSdrHandler::signal_timer,
    		this, &RtlSdrHandler::slot_timer);

	xmlDumper = nullptr;
	iqDumper = nullptr;
	iq_dumping.store(false);
	xml_dumping.store(false);
}

RtlSdrHandler::~RtlSdrHandler()
{
	fprintf(stdout, "closing on freq %d\n",
	        (int32_t)(this->rtlsdr_get_center_freq(theDevice)));
	stopReader();
	rtlsdrSettings->beginGroup("rtlsdrSettings");
    rtlsdrSettings->setValue("position-x", myFrame.pos().x());
    rtlsdrSettings->setValue("position-y", myFrame.pos().y());
	rtlsdrSettings->setValue("externalGain", gainControl->currentText());
	rtlsdrSettings->setValue("autogain", agcControl);
	rtlsdrSettings->setValue("ppm_correction", ppm_correction->value());
	rtlsdrSettings->setValue("bandwidth", bandwidth->value());
	rtlsdrSettings->setValue("biasControl", biasControl->isChecked() ? 1 : 0);
	rtlsdrSettings->setValue("filterSelector", filterSelector->isChecked() ? 1 : 0);
	rtlsdrSettings->setValue("filterDepth", filterDepth->value());
	rtlsdrSettings->sync();
	rtlsdrSettings->endGroup();
	myFrame.hide();
	usleep(1000);
	rtlsdr_close(theDevice);
	delete phandle;
}

void RtlSdrHandler::setVFOFrequency(int32_t f)
{
	(void)(this->rtlsdr_set_center_freq(theDevice, f));
}

int32_t	RtlSdrHandler::getVFOFrequency()
{
	return (int32_t)(this->rtlsdr_get_center_freq(theDevice));
}

void RtlSdrHandler::set_filter(int c)
{
	filtering = c ? 1 : 0;
}

bool RtlSdrHandler::restartReader(int32_t freq)
{
	_I_Buffer.flush_ring_buffer();

	(void)(this->rtlsdr_set_center_freq(theDevice, freq));

	set_ExternalGain(gainControl->currentIndex());
	if (workerHandle == nullptr)
    {
	    (void)this->rtlsdr_reset_buffer(theDevice);
	    workerHandle	= new dll_driver(this);
	}
	isActive.store(true);
	return true;
}

void RtlSdrHandler::stopReader()
{
	if (workerHandle == nullptr)
	    return;
	isActive.store(false);
	this->rtlsdr_cancel_async(theDevice);
	this->rtlsdr_reset_buffer(theDevice);
	if (workerHandle != nullptr)
    {
	    while (!workerHandle->isFinished())
	    	usleep(100);
	    _I_Buffer.flush_ring_buffer();
	    delete  workerHandle;
	    workerHandle = nullptr;
	}
	close_xmlDump();
}

//	when selecting the gain from a table, use the table value
void RtlSdrHandler::set_ExternalGain(int gain_index)
{
	const QString gain = gainControl->currentText();
	rtlsdr_set_tuner_gain(theDevice, (int)(gain.toFloat() * 10));
    //qDebug() << "gain =" << gain << ", index =" << gainControl->currentIndex();
    //fprintf(stderr, "rtlsdr_set_tuner_gain = %d\n", (int)(gain.toFloat() * 10));
}

void RtlSdrHandler::set_bandwidth(int32_t bandwidth)
{
	if (rtlsdr_set_tuner_bandwidth != nullptr)
    	rtlsdr_set_tuner_bandwidth(theDevice, bandwidth*1000);
    //fprintf(stderr, "Bandwidth = %d\n", bandwidth*1000);
}

void RtlSdrHandler::set_autogain(int agc)
{
	//fprintf(stderr, "set_autogain = %d\n", agc);
	agcControl = agc;
	if(agc == 0)
		hw_agc->setChecked(true);
	else if(agc == 1)
		manual->setChecked(true);
	else
		sw_agc->setChecked(true);
  	rtlsdr_set_agc_mode(theDevice, agc == 0);
    rtlsdr_set_tuner_gain_mode(theDevice, agc);
	enable_gainControl(agc);
	if(agc == 1) set_ExternalGain(0);
}

void RtlSdrHandler::set_biasControl(int bias)
{
	if (rtlsdr_set_bias_tee != nullptr)
	    rtlsdr_set_bias_tee(theDevice, bias ? 1 : 0);
}

//	correction is in ppm
void RtlSdrHandler::set_ppmCorrection(double ppm)
{
	int corr = ppm*1000;
	this->rtlsdr_set_freq_correction_ppb(theDevice, corr);
	//fprintf(stderr,"ppm=%d\n",corr);
}

int32_t	RtlSdrHandler::getSamples(cmplx *V, int32_t size)
{
    std::complex<uint8_t> temp[size];
    int	amount;
    static uint8_t dumpBuffer[4096];
    static int iqTeller	= 0;

	if (!isActive.load())
	    return 0;

	amount = _I_Buffer.get_data_from_ring_buffer(temp, size);
	//if(size > 1) fprintf(stderr, "get_data=%d\n", size);
	if (filtering)
    {
	    if (filterDepth->value() != currentDepth)
        {
	        currentDepth = filterDepth->value();
	        theFilter.resize (currentDepth);
	    }
	    for (int i = 0; i < amount; i ++)
	        V[i] = theFilter.Pass(cmplx(mapTable[real(temp[i]) & 0xFF],
	                                    mapTable[imag(temp[i]) & 0xFF]));
	}
	else
	    for (int i = 0; i < amount; i ++)
	        V[i] = cmplx(mapTable[real(temp[i]) & 0xFF],
	                     mapTable[imag(temp[i]) & 0xFF]);
	if (xml_dumping.load())
	    xmlWriter->add(temp, amount);
	else if (iq_dumping.load())
    {
	    for (int i = 0; i < size; i ++)
        {
	        dumpBuffer[iqTeller] = real(temp[i]);
	        dumpBuffer[iqTeller + 1] = imag(temp[i]);
	        iqTeller += 2;
	        if (iqTeller >= 4096)
            {
	            fwrite(dumpBuffer, 1, 4096, iqDumper);
	            iqTeller = 0;
	        }
	    }
	}
	return amount;
}

int32_t	RtlSdrHandler::Samples()
{
	if (!isActive.load())
	    return 0;
	return _I_Buffer.get_ring_buffer_read_available();
}

bool RtlSdrHandler::load_rtlFunctions()
{
    //	link the required procedures
	rtlsdr_open	= (pfnrtlsdr_open)phandle->resolve("rtlsdr_open");
	if (rtlsdr_open == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_open\n");
	    return false;
	}

	rtlsdr_close = (pfnrtlsdr_close)phandle->resolve("rtlsdr_close");
	if (rtlsdr_close == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_close\n");
	    return false;
	}

	rtlsdr_get_usb_strings = (pfnrtlsdr_get_usb_strings)
	                   phandle->resolve("rtlsdr_get_usb_strings");
	if (rtlsdr_get_usb_strings == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_get_usb_strings\n");
	    return false;
	}

	rtlsdr_set_sample_rate = (pfnrtlsdr_set_sample_rate)
	                  phandle->resolve("rtlsdr_set_sample_rate");
	if (rtlsdr_set_sample_rate == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_set_sample_rate\n");
	    return false;
	}

	rtlsdr_get_sample_rate = (pfnrtlsdr_get_sample_rate)
	                  phandle->resolve("rtlsdr_get_sample_rate");
	if (rtlsdr_get_sample_rate == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_get_sample_rate\n");
	    return false;
	}

	rtlsdr_get_tuner_gains	= (pfnrtlsdr_get_tuner_gains)
	                 phandle->resolve("rtlsdr_get_tuner_gains");
	if (rtlsdr_get_tuner_gains == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_get_tuner_gains\n");
	    return false;
	}

	rtlsdr_set_tuner_gain_mode = (pfnrtlsdr_set_tuner_gain_mode)
	                 phandle->resolve("rtlsdr_set_tuner_gain_mode");
	if (rtlsdr_set_tuner_gain_mode == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_set_tuner_gain_mode\n");
	    return false;
	}

	rtlsdr_set_agc_mode	= (pfnrtlsdr_set_agc_mode)
	                 phandle->resolve("rtlsdr_set_agc_mode");
	if (rtlsdr_set_agc_mode == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_set_agc_mode\n");
	    return false;
	}

	rtlsdr_set_tuner_gain = (pfnrtlsdr_set_tuner_gain)
	                phandle->resolve ("rtlsdr_set_tuner_gain");
	if (rtlsdr_set_tuner_gain == nullptr)
    {
	    fprintf (stderr, "Cound not find rtlsdr_set_tuner_gain\n");
	    return false;
	}

	rtlsdr_get_tuner_gain = (pfnrtlsdr_get_tuner_gain)
	                 phandle->resolve("rtlsdr_get_tuner_gain");
	if (rtlsdr_get_tuner_gain == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_get_tuner_gain\n");
	    return false;
	}

	rtlsdr_set_center_freq = (pfnrtlsdr_set_center_freq)
	                phandle->resolve("rtlsdr_set_center_freq");
	if (rtlsdr_set_center_freq == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_set_center_freq\n");
	    return false;
	}

	rtlsdr_get_center_freq = (pfnrtlsdr_get_center_freq)
	                phandle->resolve("rtlsdr_get_center_freq");
	if (rtlsdr_get_center_freq == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_get_center_freq\n");
	    return false;
	}

	rtlsdr_reset_buffer	= (pfnrtlsdr_reset_buffer)
	                phandle->resolve("rtlsdr_reset_buffer");
	if (rtlsdr_reset_buffer == nullptr)
	{
	    fprintf(stderr, "Could not find rtlsdr_reset_buffer\n");
	    return false;
	}

	rtlsdr_read_async = (pfnrtlsdr_read_async)
	                phandle->resolve("rtlsdr_read_async");
	if (rtlsdr_read_async == nullptr)
    {
	    fprintf(stderr, "Cound not find rtlsdr_read_async\n");
	    return false;
	}

	rtlsdr_get_device_count	= (pfnrtlsdr_get_device_count)
	               phandle->resolve("rtlsdr_get_device_count");
	if (rtlsdr_get_device_count == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_get_device_count\n");
	    return false;
	}

	rtlsdr_cancel_async	= (pfnrtlsdr_cancel_async)
	               phandle->resolve("rtlsdr_cancel_async");
	if (rtlsdr_cancel_async == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_cancel_async\n");
	    return false;
	}

	rtlsdr_set_direct_sampling = (pfnrtlsdr_set_direct_sampling)
	               phandle->resolve("rtlsdr_set_direct_sampling");
	if (rtlsdr_set_direct_sampling == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_set_direct_sampling\n");
	    return false;
	}

	rtlsdr_set_freq_correction = (pfnrtlsdr_set_freq_correction)
	               phandle->resolve("rtlsdr_set_freq_correction");
	if (rtlsdr_set_freq_correction == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_set_freq_correction\n");
	    return false;
	}

	rtlsdr_set_freq_correction_ppb = (pfnrtlsdr_set_freq_correction_ppb)
	               phandle->resolve("rtlsdr_set_freq_correction_ppb");
	if (rtlsdr_set_freq_correction_ppb == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_set_freq_correction_ppb\n");
	    return false;
	}

	rtlsdr_get_device_name = (pfnrtlsdr_get_device_name)
	               phandle->resolve("rtlsdr_get_device_name");
	if (rtlsdr_get_device_name == nullptr)
    {
	    fprintf(stderr, "Could not find rtlsdr_get_device_name\n");
	    return false;
	}

	rtlsdr_set_tuner_bandwidth = (pfnrtlsdr_set_center_freq)
	                phandle->resolve("rtlsdr_set_tuner_bandwidth");
	if (rtlsdr_set_tuner_bandwidth == nullptr)
    {
	    // nullpointer - if function is not available - is handled
	    fprintf(stderr, "no support for tuner bandwidth\n");
	}

	rtlsdr_set_bias_tee = (pfnrtlsdr_set_bias_tee)
	               phandle->resolve("rtlsdr_set_bias_tee");
	if (rtlsdr_set_bias_tee == nullptr)
    {
	    // nullpointer - if function is not available - is handled
	    fprintf(stderr, "biasControl will not work\n");
	}

	rtlsdr_get_tuner_i2c_register = (pfnrtlsdr_get_tuner_i2c_register)
	               phandle->resolve("rtlsdr_get_tuner_i2c_register");
	if (rtlsdr_get_tuner_i2c_register == nullptr)
    {
	    // nullpointer - if function is not available - is handled
	    fprintf(stderr, "get_tuner_i2c_register will not work\n");
	}

	rtlsdr_get_tuner_type = (pfnrtlsdr_get_tuner_type)
	               phandle->resolve("rtlsdr_get_tuner_type");
	if (rtlsdr_get_tuner_type == nullptr)
    {
	    // nullpointer - if function is not available - is handled
	    fprintf(stderr, "rtlsdr_get_tuner_type will not work\n");
	}

	fprintf(stderr, "RTLSDR functions loaded\n");
	return true;
}

void RtlSdrHandler::resetBuffer()
{
	_I_Buffer.flush_ring_buffer();
}

int16_t	RtlSdrHandler::maxGain()
{
	return gainsCount;
}

int16_t	RtlSdrHandler::bitDepth()
{
	return 8;
}

QString	RtlSdrHandler::deviceName()
{
	return deviceModel;
}

void RtlSdrHandler::show()
{
	myFrame.show();
}

void RtlSdrHandler::hide()
{
	myFrame.hide();
}

bool RtlSdrHandler::isHidden()
{
	return myFrame.isHidden();
}

void RtlSdrHandler::set_iqDump()
{
	if (!iq_dumping.load())
	//if (iqDumper == nullptr)
    {
	    if (setup_iqDump())
        {
	        iq_dumpButton->setText("writing raw file");
	        xml_dumpButton->hide();
	    }
	}
	else
    {
	    close_iqDump();
	    iq_dumpButton->setText("Dump to raw");
	    xml_dumpButton->show();
	}
}

bool RtlSdrHandler::setup_iqDump()
{
	QString fileName = QFileDialog::getSaveFileName(nullptr,
	                                         tr("Save file ..."),
	                                         QDir::homePath(),
	                                         tr("raw (*.raw)"),
                                             nullptr,
                                             QFileDialog::DontUseNativeDialog);
	fileName = QDir::toNativeSeparators(fileName);
	iqDumper = fopen(fileName.toUtf8().data(), "wb");
	if (iqDumper == nullptr)
	    return false;

	iq_dumping.store(true);
	return true;
}

void RtlSdrHandler::close_iqDump()
{
	if (iqDumper == nullptr)	// this can happen !!
	    return;
	iq_dumping.store(false);
	fclose(iqDumper);
	iqDumper = nullptr;
}

void RtlSdrHandler::set_xmlDump()
{
	if (!xml_dumping.load())
    {
	    if (setup_xmlDump())
	    {
	        xml_dumpButton->setText("writing xml file");
		    iq_dumpButton->hide();
	    }
	}
	else
	{
	    close_xmlDump();
	    xml_dumpButton->setText("Dump to xml");
	    iq_dumpButton->show();
	}
}

static inline bool isValid(QChar c)
{
	return c.isLetterOrNumber() || (c == '-');
}

bool RtlSdrHandler::setup_xmlDump()
{
    QTime	theTime;
    QDate	theDate;
    QString saveDir = rtlsdrSettings->value("saveDir_xmlDump",
	                                   QDir::homePath()).toString();

	if (xml_dumping.load())
	    return false;

	if ((saveDir != "") && (!saveDir.endsWith("/")))
	    saveDir += "/";

	QString channel	= rtlsdrSettings->value("channel", "xx").toString();
	QString timeString = theDate.currentDate().toString() + "-" +
	                     theTime.currentTime().toString();
	for (int i = 0; i < timeString.length(); i ++)
	    if (!isValid(timeString.at(i)))
	        timeString.replace(i, 1, '-');
	QString suggestedFileName =
	            saveDir + deviceModel + "-" + channel + "-" + timeString;
	QString fileName =
	           QFileDialog::getSaveFileName(nullptr,
	                                        tr("Save file ..."),
	                                        suggestedFileName + ".uff",
	                                        tr("Xml (*.uff)"),
	                                        nullptr,
                                            QFileDialog::DontUseNativeDialog);
	fileName = QDir::toNativeSeparators(fileName);
	xmlDumper = fopen(fileName.toUtf8().data(), "wb");
	if (xmlDumper == nullptr)
	    return false;

	xmlWriter = new XmlFileWriter(xmlDumper,
	                               8,
	                               "uint8",
	                               INPUT_RATE,
	                               getVFOFrequency(),
	                               "rtlsdr",
	                               deviceModel,
	                               recorderVersion);
	xml_dumping.store(true);

	QString	dumper = QDir::fromNativeSeparators(fileName);
	int x = dumper.lastIndexOf("/");
	saveDir	= dumper.remove(x, dumper.length() - x);
	rtlsdrSettings->setValue("saveDir_xmlDump", saveDir);

	return true;
}

void RtlSdrHandler::close_xmlDump()
{
	if (!xml_dumping.load())
	    return;
	if (xmlDumper == nullptr)	// cannot happen
	    return;
	xml_dumping.store(false);
	usleep(1000);
	xmlWriter->computeHeader();
	delete xmlWriter;
	fclose(xmlDumper);
	xmlDumper = nullptr;
}

bool RtlSdrHandler::isFileInput()
{
    return false;
}

void RtlSdrHandler::handle_hw_agc()
{
	set_autogain(0);
}

void RtlSdrHandler::handle_sw_agc()
{
	set_autogain(2);
}

void RtlSdrHandler::handle_manual()
{
	set_autogain(1);
}

void RtlSdrHandler::enable_gainControl(int agc)
{
	gainControl->setEnabled(agc == 1);
	gainLabel->setEnabled(agc == 1);
	dbLabel->setEnabled(agc == 1);
}

bool RtlSdrHandler::detect_overload(uint8_t *buf, int len)
{
	int overload_count = 0;
	for(int i=0; i<len; i++)
	{
		if ((buf[i] == 0) || (buf[i] == 255))
			overload_count++;
	}
	return (8000 * overload_count >= len);
}

void RtlSdrHandler::slot_timer(int overload)
{
    unsigned char data[192];
    int reg_len, tuner_gain;
    int result;

    if(old_overload != overload)
    {
   		if (overload)
  		{
    		labelOverload->setText("Overload");
    		labelOverload->setStyleSheet("QLabel {background-color : red; color: white}");
  		}
  		else
  		{
  			labelOverload->setText("Ok");
  			labelOverload->setStyleSheet("QLabel {background-color : green; color: white}");
  		}
    	old_overload = overload;
  	}

  	result = rtlsdr_get_tuner_i2c_register(theDevice, data, &reg_len, &tuner_gain);
  	if (result == 0)
  	{
		tuner_gain = (tuner_gain + 5) / 10;
		if(old_gain != tuner_gain)
		{
			if(agcControl)
				tunerGain->setText(QString::number(tuner_gain) + " dB");
			else
				tunerGain->setText("unknown");
			old_gain = tuner_gain;
		}
  	}
	else
		fprintf(stderr, "rtlsdr_get_tuner_i2c_register failed\n");
}
