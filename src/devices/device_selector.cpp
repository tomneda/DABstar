/*
 * Copyright (c) 2024
 * Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is acknowledged.
 */

#ifdef  HAVE_SPYSERVER
  #include "spyserver_client.h"
// #include "spy-handler.h"
#endif
#ifdef  HAVE_RTLSDR
  #include "rtlsdr_handler.h"
#endif
#ifdef  HAVE_SDRPLAY
  #include "sdrplay_handler.h"
#endif
#ifdef  _WIN32
  #ifdef  HAVE_EXTIO
    #include "extio_handler.h"
  #endif
#endif
#ifdef  HAVE_RTL_TCP
  #include "rtl_tcp_client.h"
#endif
#ifdef  HAVE_AIRSPY
  #include "airspy_handler.h"
#endif
#ifdef  HAVE_HACKRF
  #include "hackrf_handler.h"
#endif
#ifdef  HAVE_LIME
  #include "lime_handler.h"
#endif
#ifdef  HAVE_PLUTO
  #include "pluto_handler.h"
#endif
#ifdef  HAVE_SOAPY
  #include "soapy_handler.h"
#endif
#ifdef  HAVE_ELAD
  #include "elad_handler.h"
#endif
#ifdef  HAVE_UHD
  #include "uhd_handler.h"
#endif
#include "device_selector.h"
#include <QSettings>
#include "xml_filereader.h"
#include "wavfiles.h"
#include "rawfiles.h"
#include "setting_helper.h"
#include <QSettings>
#include <thread>

[[maybe_unused]] static const char DN_SDRPLAY[]   = "SDRplay";
[[maybe_unused]] static const char DN_RTLTCP[]    = "RTL-TCP";
[[maybe_unused]] static const char DN_RTLSDR[]    = "RTL-SDR";
[[maybe_unused]] static const char DN_AIRSPY[]    = "Airspy";
[[maybe_unused]] static const char DN_SPYSERVER[] = "SpyServer (exp.)";
[[maybe_unused]] static const char DN_HACKRF[]    = "HackRf";
[[maybe_unused]] static const char DN_LIMESDR[]   = "LimeSDR";
[[maybe_unused]] static const char DN_PLUTO[]     = "Pluto";
[[maybe_unused]] static const char DN_SOAPY[]     = "SoapySDR (exp.)";
[[maybe_unused]] static const char DN_EXTIO[]     = "ExtIO";
[[maybe_unused]] static const char DN_ELAD[]      = "Elad-S1";
[[maybe_unused]] static const char DN_UHD[]       = "UHD/USRP";

DeviceSelector::DeviceSelector(QSettings * ipSettings) :
  mpSettings(ipSettings),
  mOpenFileDialog(ipSettings)
{
}

QStringList DeviceSelector::get_device_name_list() const
{
  QStringList sl;
  sl << "(no device)";
#ifdef  HAVE_SDRPLAY
  sl << DN_SDRPLAY;
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
#ifdef  HAVE_PLUTO
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

std::unique_ptr<IDeviceHandler> DeviceSelector::create_device(const QString & iDeviceNameOrFileName, const bool iIsFileDevice, const bool iSuppressWarnings)
{
  std::unique_ptr<IDeviceHandler> inputDevice;
  mMessage.clear();

  if (iDeviceNameOrFileName.isEmpty())
  {
    mMessage = "No device or filename given";
    return nullptr;
  }

  try
  {
    inputDevice = _create_device(iDeviceNameOrFileName, iIsFileDevice);
  }
  catch (const std::exception & e)
  {
    if (!iSuppressWarnings)  // e.g., while scanning
    {
      mMessage = QString(e.what()) + " (" + iDeviceNameOrFileName + ")";
    }
  }
  catch (...)
  {
    if (!iSuppressWarnings)
    {
      mMessage = QString("Unknown exception happens in device handling (") + iDeviceNameOrFileName + ")";
    }
  }

  return inputDevice;
}

std::unique_ptr<IDeviceHandler> DeviceSelector::_open_input_file_device_from_file_type(const QString & iFilepath) const
{
  const OpenFileDialog::EFileType typeLoc = mOpenFileDialog.get_file_type(iFilepath);

  switch (typeLoc)
  {
  case OpenFileDialog::EFileType::UNDEF:   return nullptr;
  case OpenFileDialog::EFileType::UFF_XML: return std::make_unique<XmlFileReader>(iFilepath);
  case OpenFileDialog::EFileType::SDR_WAV: return std::make_unique<WavFileHandler>(iFilepath);
  case OpenFileDialog::EFileType::RAW_IQ:  return std::make_unique<RawFileHandler>(iFilepath);
  }

  return nullptr;
}

std::unique_ptr<IDeviceHandler> DeviceSelector::_create_device(const QString & iDeviceNameOrFileName, bool iIsFileDevice) const
{
  std::unique_ptr<IDeviceHandler> inputDevice;

  if (iIsFileDevice)
  {
    if (!iDeviceNameOrFileName.isEmpty())
    {
      if (QFile::exists(iDeviceNameOrFileName))
      {
        inputDevice = _open_input_file_device_from_file_type(iDeviceNameOrFileName);
      }
      else
      {
        mMessage = QString("File does not exist: ") + iDeviceNameOrFileName;
      }
    }
    else
    {
      mMessage = QString("No filename given to play");
    }
  }
  else
#ifdef  HAVE_SDRPLAY
  if (iDeviceNameOrFileName == DN_SDRPLAY)
  {
    inputDevice = std::make_unique<SdrPlayHandler>(mpSettings, mVersionStr);
  }
  else
#endif
#ifdef  HAVE_RTLSDR
  if (iDeviceNameOrFileName == DN_RTLSDR)
  {
    inputDevice = std::make_unique<RtlSdrHandler>(mpSettings, mVersionStr);
  }
  else
#endif
#ifdef  HAVE_AIRSPY
  if (iDeviceNameOrFileName == DN_AIRSPY)
  {
    inputDevice = std::make_unique<AirspyHandler>(mpSettings, mVersionStr);
  }
  else
#endif
#ifdef  HAVE_SPYSERVER
  if (iDeviceNameOrFileName == DN_SPYSERVER)
  {
    inputDevice = std::make_unique<SpyServerClient>(mpSettings);
  }
  else
#endif
#ifdef  HAVE_HACKRF
  if (iDeviceNameOrFileName == DN_HACKRF)
  {
    inputDevice = std::make_unique<HackRfHandler>(mpSettings, mVersionStr);
  }
  else
#endif
#ifdef  HAVE_LIME
  if (iDeviceNameOrFileName == DN_LIMESDR)
  {
    inputDevice = std::make_unique<LimeHandler>(mpSettings, mVersionStr);
  }
  else
#endif
#ifdef  HAVE_PLUTO
  if (iDeviceNameOrFileName == DN_PLUTO)
  {
    inputDevice = std::make_unique<PlutoHandler>(mpSettings, mVersionStr);
  }
  else
#endif
#ifdef HAVE_RTL_TCP
  if (iDeviceNameOrFileName == DN_RTLTCP)
  {
    inputDevice = std::make_unique<RtlTcpClient>(mpSettings, mVersionStr);
  }
  else
#endif
#ifdef  HAVE_ELAD
  if (iDeviceNameOrFileName == DN_ELAD)
  {
    inputDevice = std::make_unique<eladHandler>(mpSettings);
  }
  else
#endif
#ifdef  HAVE_UHD
  if (iDeviceNameOrFileName == DN_UHD)
  {
    inputDevice = std::make_unique<UhdHandler>(mpSettings);
  }
  else
#endif
#ifdef  HAVE_SOAPY
  if (iDeviceNameOrFileName == DN_SOAPY)
  {
    inputDevice = std::make_unique<SoapyHandler>(mpSettings);
  }
  else
#endif
#ifdef  HAVE_EXTIO
  // extio is - in its current settings - for Windows, it is a wrap around the dll
  if (iDeviceNameOrFileName == DN_EXTIO)
  {
    inputDevice = std::make_unique<extioHandler>(mpSettings);
  }
  else
#endif
  {
    mMessage = QString("Unknown device: ") + iDeviceNameOrFileName;
  }

  if (inputDevice != nullptr)
  {
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
  }

  return inputDevice;
}

std::unique_ptr<IDeviceSelector> create_device_selector(QSettings * const ipSettings)
{
  return std::make_unique<DeviceSelector>(ipSettings);
}
