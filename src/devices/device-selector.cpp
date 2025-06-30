/*
 * Copyright (c) 2024
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
#ifdef  HAVE_SPYSERVER
  #include "spyserver-client.h"
  // #include "spy-handler.h"
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
#include "device-selector.h"
#include "xml-filereader.h"
#include "wavfiles.h"
#include "rawfiles.h"
#include "setting-helper.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <thread>

static const char DN_FILE_INP[]   = "File input";
static const char DN_SDRPLAY_V3[] = "SDR-Play V3";
static const char DN_SDRPLAY_V2[] = "SDR-Play V2";
static const char DN_RTLTCP[]     = "RTL-TCP";
static const char DN_RTLSDR[]     = "RTL-SDR";
static const char DN_AIRSPY[]     = "Airspy";
static const char DN_SPYSERVER[]  = "SpyServer (experimental)";
static const char DN_HACKRF[]     = "HackRf";
static const char DN_LIMESDR[]    = "LimeSDR";
static const char DN_PLUTO_RXTX[] = "Pluto-RxTx";
static const char DN_PLUTO[]      = "Pluto";
static const char DN_SOAPY[]      = "Soapy";
static const char DN_EXTIO[]      = "ExtIO";
static const char DN_ELAD[]       = "Elad-S1";
static const char DN_UHD[]        = "UHD/USRP";


DeviceSelector::DeviceSelector(QSettings * ipSettings) :
  mpSettings(ipSettings),
  mOpenFileDialog(ipSettings)
{
}

QStringList DeviceSelector::get_device_name_list() const
{
  QStringList sl;
  sl << "select input";
  sl << DN_FILE_INP;
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
#ifdef  HAVE_SPYSERVER
  sl << DN_SPYSERVER;
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

std::unique_ptr<IDeviceHandler> DeviceSelector::create_device(const QString & iDeviceName, bool & oRealDevice)
{
  std::unique_ptr<IDeviceHandler> inputDevice;

  try
  {
    inputDevice = _create_device(iDeviceName, oRealDevice);
  }
  catch (const std::exception & e)
  {
    QMessageBox::warning(this, "Warning", e.what());
    inputDevice = nullptr;
  }
  catch (...)
  {
    QMessageBox::warning(this, "Warning", "Unknown exception happens in device handling");
    inputDevice = nullptr;
  }
  mCurDeviceName = iDeviceName;
  return inputDevice;
}

std::unique_ptr<IDeviceHandler> DeviceSelector::_create_device(const QString & iDeviceName, bool & oRealDevice)
{
  std::unique_ptr<IDeviceHandler> inputDevice;

  oRealDevice = true; // until proven otherwise

#ifdef  HAVE_SDRPLAY_V2
  if (iDeviceName == DN_SDRPLAY_V2)
  {
#ifdef  __MINGW32__
    QMessageBox::warning (this, tr ("Warning"), tr ("If SDRuno is installed with drivers 3.10,\nV2.13 drivers will not work anymore, choose \"sdrplay\" instead\n"));
    return nullptr;
#endif
    inputDevice = std::make_unique<SdrPlayHandler_v2>(mpSettings, mVersionStr);
  }
  else
#endif
#ifdef  HAVE_SDRPLAY_V3
  if (iDeviceName == DN_SDRPLAY_V3)
  {
    inputDevice = std::make_unique<SdrPlayHandler_v3>(mpSettings, mVersionStr);
  }
  else
#endif
#ifdef  HAVE_RTLSDR
  if (iDeviceName == DN_RTLSDR)
  {
    inputDevice = std::make_unique<RtlSdrHandler>(mpSettings, mVersionStr);
  }
  else
#endif
#ifdef  HAVE_AIRSPY
  if (iDeviceName == DN_AIRSPY)
  {
    inputDevice = std::make_unique<AirspyHandler>(mpSettings, mVersionStr);
  }
  else
#endif
#ifdef  HAVE_SPYSERVER
  if (iDeviceName == DN_SPYSERVER)
  {
    inputDevice = std::make_unique<SpyServerClient>(mpSettings);
  }
  else
#endif
#ifdef  HAVE_HACKRF
  if (iDeviceName == DN_HACKRF)
  {
    inputDevice = std::make_unique<HackRfHandler>(mpSettings, mVersionStr);
  }
  else
#endif
#ifdef  HAVE_LIME
  if (iDeviceName == DN_LIMESDR)
  {
    inputDevice = std::make_unique<LimeHandler>(mpSettings, mVersionStr);
  }
  else
#endif
#ifdef  HAVE_PLUTO_2
  if (s == DN_PLUTO)
  {
    inputDevice = std::make_unique<plutoHandler>(mpSettings, version);
  }
  else
#endif
#ifdef  HAVE_PLUTO_RXTX
  if (s == DN_PLUTO_RXTX)
  {
    inputDevice = std::make_unique<plutoHandler>(mpSettings, version, fmFrequency);
    streamerOut = new dabStreamer(48000, 192000, (plutoHandler *)inputDevice);
    ((plutoHandler *)inputDevice)->startTransmitter(fmFrequency);
  }
  else
#endif
#ifdef HAVE_RTL_TCP
  if (iDeviceName == DN_RTLTCP)
  {
    inputDevice = std::make_unique<RtlTcpClient>(mpSettings);
  }
  else
#endif
#ifdef  HAVE_ELAD
  if (s == DN_ELAD)
  {
    inputDevice = std::make_unique<eladHandler>(mpSettings);
  }
  else
#endif
#ifdef  HAVE_UHD
  if (iDeviceName == DN_UHD)
  {
    inputDevice = std::make_unique<UhdHandler>(mpSettings);
  }
  else
#endif
#ifdef  HAVE_SOAPY
  if (s == DN_SOAPY)
  {
    inputDevice = std::make_unique<soapyHandler>(mpSettings);
  }
  else
#endif
#ifdef  HAVE_EXTIO
  // extio is - in its current settings - for Windows, it is a wrap around the dll
  if (s == DN_EXTIO)
  {
    inputDevice = std::make_unique<extioHandler>(mpSettings);
  }
  else
#endif
  if (iDeviceName == DN_FILE_INP)
  {
    oRealDevice = false;
    OpenFileDialog::EType type = OpenFileDialog::EType::UNDEF;
    QString file;

    // check if last played file is still valid
    const QString lastFileName = Settings::Main::varDeviceFile.read().toString();

    if (!lastFileName.isEmpty() && QFile::exists(lastFileName))
    {
      OpenFileDialog::EType typeLoc = mOpenFileDialog.get_file_type(lastFileName);
      if (typeLoc != OpenFileDialog::EType::UNDEF)
      {
        type = typeLoc;
        file = lastFileName;
      }
    }

    if (type == OpenFileDialog::EType::UNDEF)
    {
      file = mOpenFileDialog.open_sample_data_file_dialog_for_reading(type);

      if (file.isEmpty()) // dialog closed with cancel?
      {
        Settings::Main::varDeviceFile.write("");
        return nullptr;
      }
    }

    switch (type)
    {
    case OpenFileDialog::EType::XML: inputDevice = std::make_unique<XmlFileReader>(file); break;
    case OpenFileDialog::EType::SDR: inputDevice = std::make_unique<WavFileHandler>(file); break;
    case OpenFileDialog::EType::RAW:
    case OpenFileDialog::EType::IQ:  inputDevice = std::make_unique<RawFileHandler>(file);break;
    default: return nullptr;
    }

    Settings::Main::varDeviceFile.write(file);
  }
  else
  {
    return nullptr;
  }

  Settings::Main::varSdrDevice.write(iDeviceName);

  /*
   * Per default the device widget should be shown after creation.
   * If we want it switched off: Sometimes it has problems to be switched off too fast again (a frame remains on the screen).
   * With this delay the problem can be workaround but causes a more viewable flickering of the device widget.
   * For case of symmetry, we do this also with show() the widget (again) after creation.
   */
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  if (Settings::Main::varDeviceUiVisible.read().toBool())
  {
    inputDevice->show(); // should not be necessary as the device widget is shown by default
  }
  else
  {
    inputDevice->hide();
  }

  return inputDevice;
}

bool DeviceSelector::reset_file_input_last_file(const QString & iDeviceName)
{
  if (iDeviceName == DN_FILE_INP)
  {
    Settings::Main::varDeviceFile.write("");
    return true;
  }
  return false;
}

