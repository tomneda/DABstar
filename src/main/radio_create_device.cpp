/*
 * Copyright (c) 2023
 * Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is acknowledged.
 */

#ifdef  HAVE_RTLSDR
  #include  "rtlsdr-handler.h"
#endif
#ifdef  HAVE_SDRPLAY_V2
  #include  "sdrplay-handler-v2.h"
#endif
#ifdef  HAVE_SDRPLAY_V3
  #include  "sdrplay-handler-v3.h"
#endif
#ifdef  __MINGW32__
#ifdef	HAVE_EXTIO
  #include  "extio-handler.h"
#endif
#endif
#ifdef  HAVE_RTL_TCP
  #include  "rtl_tcp_client.h"
#endif
#ifdef  HAVE_AIRSPY
  #include "airspy-handler.h"
#endif
#ifdef  HAVE_HACKRF
  #include "hackrf-handler.h"
#endif
#ifdef  HAVE_LIME
  #include  "lime-handler.h"
#endif
#ifdef  HAVE_PLUTO_2
  #include  "pluto-handler-2.h"
#elif  HAVE_PLUTO_RXTX
  #include  "pluto-rxtx-handler.h"
  #include  "dab-streamer.h"
#endif
#ifdef  HAVE_SOAPY
  #include	"soapy-handler.h"
#endif
#ifdef  HAVE_ELAD
  #include	"elad-handler.h"
#endif
#ifdef  HAVE_UHD
  #include  "uhd-handler.h"
#endif

#include  "xml-filereader.h"
#include  "wavfiles.h"
#include  "rawfiles.h"
#include  "radio.h"

static const char DN_FILE_INP_RAW[] = "File input(.raw)";
static const char DN_FILE_INP_IQ[]  = "File input(.iq)";
static const char DN_FILE_INP_SDR[] = "File input(.sdr)";
static const char DN_FILE_INP_XML[] = "File input(.xml)";
static const char DN_SDRPLAY_V3[]   = "SDR-Play V3";
static const char DN_SDRPLAY_V2[]   = "SDR-Play V2";
static const char DN_RTLTCP[]       = "RTL-TCP";
static const char DN_RTLSDR[]       = "RTL-SDR";
static const char DN_AIRSPY[]       = "Airspy";
static const char DN_HACKRF[]       = "HackRf";
static const char DN_LIMESDR[]      = "LimeSDR";
static const char DN_PLUTO_RXTX[]   = "Pluto-RxTx";
static const char DN_PLUTO[]        = "Pluto";
static const char DN_SOAPY[]        = "Soapy";
static const char DN_EXTIO[]        = "ExtIO";
static const char DN_ELAD[]         = "Elad-S1";
static const char DN_UHD[]          = "UHD/USRP";


QStringList RadioInterface::get_device_name_list()
{
  QStringList sl;
  sl << "select input";
  sl << DN_FILE_INP_RAW;
  sl << DN_FILE_INP_IQ;
  sl << DN_FILE_INP_SDR;
  sl << DN_FILE_INP_XML;
#ifdef  HAVE_SDRPLAY_V3
  sl << DN_SDRPLAY_V3;
#endif
#ifdef  HAVE_SDRPLAY_V2
  sl << DN_SDRPLAY_V2;
#endif
#ifdef  HAVE_RTL_TCP
  sl << DN_RTLTCP;
#endif
#ifdef  HAVE_RTLSDR
  sl << DN_RTLSDR;
#endif
#ifdef  HAVE_AIRSPY
  sl << DN_AIRSPY;
#endif
#ifdef  HAVE_HACKRF
  sl << DN_HACKRF;
#endif
#ifdef  HAVE_LIME
  sl << DN_LIMESDR;
#endif
#ifdef  HAVE_PLUTO_RXTX
  sl << DN_PLUTO_RXTX;
#elif  HAVE_PLUTO_2
  sl << DN_PLUTO;
#endif
#ifdef  HAVE_SOAPY
  sl << DN_SOAPY;
#endif
#ifdef  HAVE_EXTIO
  sl << DN_EXTIO;
#endif
#ifdef  HAVE_ELAD
  sl << DN_ELAD;
#endif
#ifdef  HAVE_UHD
  sl << DN_UHD;
#endif

  return sl;
}

IDeviceHandler * RadioInterface::create_device(const QString & s)
{
  QString file;
  IDeviceHandler * inputDevice = nullptr;

  channel.realChannel = true;    // until proven otherwise
#ifdef  HAVE_SDRPLAY_V2
  if (s == DN_SDRPLAY_V2)
  {
#ifdef  __MINGW32__
    QMessageBox::warning (this, tr ("Warning"), tr ("If SDRuno is installed with drivers 3.10,\nV2.13 drivers will not work anymore, choose \"sdrplay\" instead\n"));
    return nullptr;
#endif
    try
    {
      inputDevice = new SdrPlayHandler_v2(dabSettings, version);
      showButtons();
    }
    catch (const exception & e)
    {
      QMessageBox::warning(this, tr("Warning"), tr(e.what()));
      return nullptr;
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("sdrplay-v2 fails"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_SDRPLAY_V3
  if (s == DN_SDRPLAY_V3)
  {
    try
    {
      inputDevice = new SdrPlayHandler_v3(dabSettings, version);
      showButtons();
    }
    catch (const exception & e)
    {
      QMessageBox::warning(this, tr("Warning"), tr(e.what()));
      return nullptr;
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("sdrplay v3 fails"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_RTLSDR
  if (s == DN_RTLSDR)
  {
    try
    {
      inputDevice = new RtlSdrHandler(dabSettings, version);
      showButtons();
    }
    catch (const exception & ex)
    {
      QMessageBox::warning(this, tr("Warning"), tr(ex.what()));
      return nullptr;
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("rtlsdr device fails"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_AIRSPY
  if (s == DN_AIRSPY)
  {
    try
    {
      inputDevice = new AirspyHandler(dabSettings, version);
      showButtons();
    }
    catch (const exception & e)
    {
      QMessageBox::warning(this, tr("Warning"), tr(e.what()));
      return nullptr;
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("airspy device fails"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_HACKRF
  if (s == DN_HACKRF)
  {
    try
    {
      inputDevice = new HackRfHandler(dabSettings, version);
      showButtons();
    }
    catch (const exception & e)
    {
      QMessageBox::warning(this, tr("Warning"), tr(e.what()));
      return nullptr;
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("hackrf device fails"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_LIME
  if (s == DN_LIMESDR)
  {
    try
    {
      inputDevice = new LimeHandler(dabSettings, version);
      showButtons();
    }
    catch (const exception & e)
    {
      QMessageBox::warning(this, tr("Warning"), tr(e.what()));
      return nullptr;
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("lime device fails"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_PLUTO_2
  if (s == DN_PLUTO)
  {
    try
    {
      inputDevice = new plutoHandler(dabSettings, version);
      showButtons();
    }
    catch (const std::exception & e)
    {
      QMessageBox::warning(this, tr("Warning"), tr(e.what()));
      return nullptr;
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("pluto device fails"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_PLUTO_RXTX
  if (s == DN_PLUTO_RXTX)
  {
    try
    {
      inputDevice = new plutoHandler(dabSettings, version, fmFrequency);
      showButtons();
      streamerOut = new dabStreamer(48000, 192000, (plutoHandler *)inputDevice);
      ((plutoHandler *)inputDevice)->startTransmitter(fmFrequency);
    }
    catch (const std::exception & e)
    {
      QMessageBox::warning(this, tr("Warning"), tr(e.what()));
      return nullptr;
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("pluto device fails"));
      return nullptr;
    }
  }
  else
#endif
#ifdef HAVE_RTL_TCP
    //	RTL_TCP might be working.
  if (s == DN_RTLTCP)
  {
    try
    {
      inputDevice = new RtlTcpClient(dabSettings);
      showButtons();
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("rtl_tcp: no luck\n"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_ELAD
  if (s == DN_ELAD)
  {
    try
    {
      inputDevice = new eladHandler(dabSettings);
      showButtons();
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("no elad device found\n"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_UHD
  if (s == DN_UHD)
  {
    try
    {
      inputDevice = new UhdHandler(dabSettings);
      showButtons();
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("no UHD device found\n"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_SOAPY
  if (s == DN_SOAPY)
  {
    try
    {
      inputDevice = new soapyHandler(dabSettings);
      showButtons();
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("no soapy device found\n"));
      return nullptr;
    }
  }
  else
#endif
#ifdef  HAVE_EXTIO
  // extio is - in its current settings - for Windows, it is a
  // wrap around the dll
  if (s == DN_EXTIO)
  {
    try
    {
      inputDevice = new extioHandler(dabSettings);
      showButtons();
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("extio: no luck\n"));
      return nullptr;
    }
  }
  else
#endif
  if (s == DN_FILE_INP_XML)
  {
    file = QFileDialog::getOpenFileName(this, tr("Open file ..."), QDir::homePath(), tr("xml data (*.*)"));
    if (file == QString(""))
    {
      return nullptr;
    }
    file = QDir::toNativeSeparators(file);
    try
    {
      inputDevice = new xml_fileReader(file);
      channel.realChannel = false;
      hideButtons();
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("file not found"));
      return nullptr;
    }
  }
  else if ((s == DN_FILE_INP_IQ) || (s == DN_FILE_INP_RAW))
  {
    const char * p;
    if (s == DN_FILE_INP_IQ)
    {
      p = "iq data (*iq)";
    }
    else
    {
      p = "raw data (*raw)";
    }

    file = QFileDialog::getOpenFileName(this, tr("Open file ..."), QDir::homePath(), tr(p));
    if (file == QString(""))
    {
      return nullptr;
    }

    file = QDir::toNativeSeparators(file);
    try
    {
      inputDevice = new RawFileHandler(file);
      hideButtons();
      channel.realChannel = false;
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("file not found"));
      return nullptr;
    }
  }
  else if (s == DN_FILE_INP_SDR)
  {
    file = QFileDialog::getOpenFileName(this, tr("Open file ..."), QDir::homePath(), tr("raw data (*.sdr)"));
    if (file == QString(""))
    {
      return nullptr;
    }

    file = QDir::toNativeSeparators(file);
    try
    {
      inputDevice = new WavFileHandler(file);
      channel.realChannel = false;
      hideButtons();
    }
    catch (...)
    {
      QMessageBox::warning(this, tr("Warning"), tr("file not found"));
      return nullptr;
    }
  }
  else
  {
    fprintf(stderr, "unknown device, failing\n");
    return nullptr;
  }

  //my_spectrumViewer.set_bit_depth(inputDevice->bitDepth());

  dabSettings->setValue("device", s);
  //	do we want to see the widget for device control?
  if (dabSettings->value("deviceVisible", 0).toInt())
  {
    inputDevice->show();
  }
  else
  {
    inputDevice->hide();
  }

  return inputDevice;
}
