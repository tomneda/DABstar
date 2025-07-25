/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2016 .. 2023
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
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
 *
 *    A simple client for spyServer
 *
 *	Inspired by the spyserver client from Mike Weber
 *	and some functions are copied.
 *	The code is simplified since Qt-DAB functions best with
 *	16 bit codes and a samplerate of 2048000 S/s
 *	for Functions copied (more or less) from Mike weber's version
 *	copyrights are gratefully acknowledged
 */

#include "dab-constants.h"
#include "device-exceptions.h"
#include "setting-helper.h"
#include "spyserver-client.h"
#include <QMessageBox>
#include <QTimer>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(sLogSpyServerClient, "SpyServerClient", QtDebugMsg)

#define	DEFAULT_FREQUENCY	(Khz (227360))
#define	SPY_SERVER_8_SETTINGS	"SPY_SERVER_8_SETTINGS"

SpyServerClient::SpyServerClient(QSettings * /*s*/)
{
  // spyServer_settings = s;
  setupUi(&mFrame);
  mFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  mFrame.show();

  const QString url = "https://airspy.com/directory/";
  const QString style = "style='color: lightblue;'";
  const QString htmlLink = "Get public URLs on e.g.:<br><a " + style + " href='" + url + "'>" + url + "</a>";
  lblHelp->setText(htmlLink);
  lblHelp->setOpenExternalLinks(true);

  Settings::SpyServer::posAndSize.read_widget_geometry(&mFrame, 240, 220, true);
  Settings::SpyServer::sbGain.register_widget_and_update_ui_from_setting(sbGain, 20);
  Settings::SpyServer::cbAutoGain.register_widget_and_update_ui_from_setting(cbAutoGain, 2); // 2 = checked

  editIpAddress->setText(Settings::SpyServer::varIpAddress.read().toString());

  mSettings.gain = sbGain->value();
  mSettings.auto_gain = cbAutoGain->isChecked();
  // settings.basePort = 5555;

  // portNumber->setValue(settings.basePort);
  // if (settings.auto_gain2)
  // {
  //   autogain_selector->setChecked(true);
  // }

  // spyServer_gain->setValue(theGain); // TODO: tomneda
  // lastFrequency = DEFAULT_FREQUENCY; // TODO: tomneda
  // theServer = nullptr;
  // hostLineEdit = new QLineEdit(nullptr);
  mSettings.resample_quality = 2;
  mSettings.batchSize = 4096;
  mSettings.sample_bits = 16;

  mByteBuffer.resize(mSettings.batchSize * 2);

  connect(btnConnect, &QPushButton::clicked, this, &SpyServerClient::_slot_handle_connect_button);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(cbAutoGain, &QCheckBox::checkStateChanged, this, &SpyServerClient::_slot_handle_autogain);
#else
  connect (cbAutoGain, &QCheckBox::stateChanged, this, &SpyServerClient::_slot_handle_autogain);
#endif
  connect(sbGain, qOverload<i32>(&QSpinBox::valueChanged), this, &SpyServerClient::_slot_handle_gain);
  // connect(portNumber, qOverload<i32>(&QSpinBox::valueChanged), this, &spyServer_client_8::set_portNumber);
  // connect(editIpAddress, &QLineEdit::textChanged, this, &spyServer_client_8::_check_and_cleanup_ip_address);
  btnConnect->setStyleSheet("background-color: #668888FF;");
  lblState->setStyleSheet("color: #8888FF;");
  lblState->setText("Disconnected");
}

SpyServerClient::~SpyServerClient()
{
  if (mIsConnected)
  {		// close previous connection
    stopReader();
    mIsConnected = false;
  }

#ifdef HAVE_LIQUID
  if (mLiquidResampler != nullptr)
  {
    resamp_crcf_destroy(mLiquidResampler);
  }
#endif

  Settings::SpyServer::posAndSize.write_widget_geometry(&mFrame);
}

//
void SpyServerClient::_slot_handle_connect_button()
{
  if (mIsConnected)
  {
    stopReader();
    mIsConnected = false;
    mSpyServerHandler.reset();
    mSettings = {};
    btnConnect->setStyleSheet("background-color: #668888FF;");
    btnConnect->setText("Connect");
    lblState->setStyleSheet("color: #8888FF;");
    lblState->setText("Disconnected");
    editIpAddress->setEnabled(true);
    return;
  }

  if (!_check_and_cleanup_ip_address())
  {
    return;
  }

  if (_setup_connection())
  {
    editIpAddress->setEnabled(false); // TODO: when to enable again?
    btnConnect->setStyleSheet("background-color: #6688FF88;");
    btnConnect->setText("Disconnect");
    lblState->setStyleSheet("color: #88FF88;");
    lblState->setText("Connected");
    mIsConnected = true;
  }

  // QString ipAddress;
  // QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
  //
  // // use the first non-localhost IPv4 address
  // for (i16 i = 0; i < ipAddressesList.size(); ++i)
  // {
  //   if (ipAddressesList.at(i) != QHostAddress::LocalHost &&
  //       ipAddressesList.at(i).toIPv4Address())
  //   {
  //     ipAddress = ipAddressesList.at(i).toString();
  //     break;
  //   }
  // }
  //
  // // if we did not find one, use IPv4 localhost
  // if (ipAddress.isEmpty())
  // {
  //   ipAddress = QHostAddress(QHostAddress::LocalHost).toString();
  // }

  // ipAddress = value_s(spyServer_settings, SPY_SERVER_8_SETTINGS, "remote-server", ipAddress);  // TODO: tomneda
  // hostLineEdit->setText(ipAddress);

  // hostLineEdit->setInputMask("000.000.000.000");
  //Setting default IP address
  // hostLineEdit->show();
  // theState->setText("Enter IP address, \nthen press return");
  // connect(hostLineEdit, &QLineEdit::returnPressed, this, &spyServer_client_8::setConnection);
}

//	if/when a return is pressed in the line edit,
//	a signal appears and we are able to collect the
//	inserted text. The format is the IP-V4 format.
//	Using this text, we try to connect,
bool SpyServerClient::_setup_connection()
{
  // QString s = hostLineEdit->text();
  // QString theAddress = QHostAddress(s).toString();
  //onConnect.store(false);
  mSpyServerHandler.reset();

  try
  {
    mSpyServerHandler = std::make_unique<SpyServerHandler>(this, mSettings.ipAddress, (i32)mSettings.basePort, &mRingBuffer1);
  }
  catch (...)
  {
    mSpyServerHandler.reset(); // not sure if the object is already destroyed here (or not created)
    QMessageBox::warning(nullptr, "Warning", "Connection failed");
    return false;
  }

  if (mSpyServerHandler == nullptr)
  {
    qCCritical(sLogSpyServerClient) << "Failed to create SpyServerHandler";
    return false;
  }

  QEventLoop eventLoop;
  QTimer checkTimer;
  checkTimer.setSingleShot(true);

  // Tomneda: changed this to a QEventLoop as the QTimer never timed-out within this thread
  // could be a bit tricky because SpyServerHandler::signal_call_parent could be called before the connection is done -> false timeout will occur
  bool timedOut = false;
  const auto timerConn = connect(&checkTimer, &QTimer::timeout, [&]() { timedOut = true; eventLoop.quit(); });
  const auto eventConn = connect(mSpyServerHandler.get(), &SpyServerHandler::signal_call_parent, &eventLoop, &QEventLoop::quit);
  checkTimer.start(2000);
  eventLoop.exec(); // waits until QEventLoop::quit() was called
  checkTimer.stop();
  disconnect(timerConn);
  disconnect(eventConn);

  if (timedOut)
  {
    qCCritical(sLogSpyServerClient()) << "Connection timed out";
    QMessageBox::warning(nullptr, "Warning", "Timeout while waiting for an answer from the server");
    mSpyServerHandler.reset();
    return false;
  }

  mSpyServerHandler->connection_set();

  DeviceInfo theDevice;
  mSpyServerHandler->get_deviceInfo(theDevice);

  bool validDevice = false;
  lblDeviceName->setStyleSheet("color: #FFBB00;");

  switch (theDevice.DeviceType)
  {
  case DEVICE_AIRSPY_ONE:
    lblDeviceName->setText("AIRSPY One");
    validDevice = true;
    break;
  case DEVICE_AIRSPY_HF:
    lblDeviceName->setText("AIRSPY HF");
    break;
  case DEVICE_RTLSDR:
    lblDeviceName->setText("RTLSDR");
    validDevice = true;
    break;
  default:
    lblDeviceName->setText("Invalid device");
  }

  if (!validDevice)
  {
    lblState->setStyleSheet("color: #FF8888;");
    lblState->setText("Invalid device");
    return false;
  }

  if (theDevice.MinimumFrequency > 174'928'000 ||  // DAB frequency range
      theDevice.MaximumFrequency < 239'200'000)
  {
    lblState->setStyleSheet("color: #FF8888;");
    lblState->setText("Invalid frequency range");
    // lblState->setText("Device works out of necessary frequency range");
    return false;
  }



  lblDeviceNumber->setStyleSheet("color: #FF8800;");
  lblDeviceNumber->setText(QString::number(theDevice.DeviceSerial));
  const u32 max_samp_rate = theDevice.MaximumSampleRate;
  const u32 decim_stages = theDevice.DecimationStageCount;
  i32 desired_decim_stage = -1;
  f64 resample_ratio = 1.0;

  if (max_samp_rate == 0)
  {
    qCCritical(sLogSpyServerClient()) << "Device does not support sampling";
    mSpyServerHandler.reset();
    return false;
  }

  for (u32 i = 0; i < decim_stages; ++i)
  {
    const i32 targetRate = (i32)(max_samp_rate / (1 << i));

    if (targetRate == INPUT_RATE)
    {
      desired_decim_stage = i;
      resample_ratio = 1;
      mSettings.sample_rate = targetRate;
      break; // we can stop here
    }
    else if (targetRate > INPUT_RATE) // would include also the "==" case but maybe we have rounding errors to exactly 1 when calculating the resample_ratio
    {
      // remember the next-largest rate that is available
      desired_decim_stage = i;
      resample_ratio = INPUT_RATE / (f64)targetRate;
      mSettings.sample_rate = targetRate;
    }
    else
    {
      break; // no lower valid sample rates found
    }
    // std::cerr << "Desired decimation stage: " << desired_decim_stage <<" (" << max_samp_rate << " / " <<(1 << desired_decim_stage) << " = " << max_samp_rate / (1 << desired_decim_stage) << ") resample ratio: " << resample_ratio << std::endl;
  }

  if (desired_decim_stage < 0)
  {
    qCCritical(sLogSpyServerClient()) << "Device does not support sampling at" << INPUT_RATE << "Sps" << " (max_samp_rate" << max_samp_rate << ")";
    mSpyServerHandler.reset();
    return false;
  }

  qCInfo(sLogSpyServerClient()) << "Device supports sampling at" << mSettings.sample_rate << "Sps" << " (resampling_ratio" << resample_ratio << ")";

  const i32 maxGain = theDevice.MaximumGainIndex;
  sbGain->setMaximum(maxGain);
  mSettings.resample_ratio = resample_ratio;
  mSettings.desired_decim_stage = desired_decim_stage;

  if (!mSpyServerHandler->set_sample_rate_by_decim_stage(desired_decim_stage))
  {
    qCCritical(sLogSpyServerClient()) << "Failed to set sample rate " << desired_decim_stage;
    return false;
  }

  // disconnect(btnConnect, &QPushButton::clicked, this, &SpyServerClient::_slot_connect);

  // fprintf(stderr, "The samplerate = %f\n", (f32)(theServer->get_sample_rate()));
//	start ();		// start the reader

//	Since we are down sampling, creating an outputbuffer with the
//	same size as the input buffer is OK

  if (mSettings.resample_ratio != 1.0)
  {
#ifdef HAVE_LIQUID
    constexpr u32 filterLen = 13;
    constexpr float resampBw = 0.5f * (float)(cK * cCarrDiff + 2*35000) / (float)INPUT_RATE;
    constexpr float sideLopeSuppr = 40.0f;
    constexpr u32 numFiltersInBank = 32;
    mLiquidResampler = resamp_crcf_create(mSettings.resample_ratio, filterLen, resampBw, sideLopeSuppr, numFiltersInBank);
    resamp_crcf_print(mLiquidResampler);
    mResampBuffer.resize(2048 + 10); // add some exaggerated samples
#else
    // we process chunks of 1 msec
    mMapTable_int.resize(2048);
    mMapTable_float.resize(2048);

    for (i32 i = 0; i < 2048; i++)
    {
      const f32 inVal = (f32)mSettings.sample_rate / 1000.0f;
      mMapTable_int[i] = (i16)std::floor((f32)i * (inVal / 2048.0f));
      mMapTable_float[i] = (f32)i * (inVal / 2048.0f) - (f32)mMapTable_int[i];
    }
    mConvIndex = 0;
    mResampBuffer.resize(2048);
#endif

    mConvBufferSize = mSettings.sample_rate / 1000;
    mConvBuffer.resize(std::max(mConvBufferSize + 1, mSettings.batchSize));
  }

  return true;
}

i32 SpyServerClient::getRate()
{
  return INPUT_RATE;
}

bool SpyServerClient::restartReader(i32 freq)
{
  if (!mIsConnected)
  {
    return false;
  }

  std::cerr << "spy-handler: setting center_freq to " << freq << std::endl;

  if (!mSpyServerHandler->set_iq_center_freq(freq))
  {
    std::cerr << "Failed to set freq\n";
    return false;
  }

  if (!mSpyServerHandler->set_gain(mSettings.gain))
  {
    std::cerr << "Failed to set gain\n";
    return false;
  }

  // toSkip = skipped;

  mSpyServerHandler->start_running();
  mIsRunning = true;
  return true;
}

void SpyServerClient::stopReader()
{
  fprintf(stderr, "stopReader is called\n");
  if (mSpyServerHandler == nullptr)
    return;

  if (!mIsConnected || !mIsRunning)	// seems double???
    return;

  if (!mSpyServerHandler->is_streaming())
    return;

  mSpyServerHandler->stop_running();
  mIsRunning = false;
}

//
//
i32 SpyServerClient::getSamples(cf32 * V, i32 size)
{
  i32 amount = 0;
  amount = mRingBuffer2.get_data_from_ring_buffer(V, size);
  return amount;
}

i32 SpyServerClient::Samples()
{
  return mRingBuffer2.get_ring_buffer_read_available();
}

i16 SpyServerClient::bitDepth()
{
  return 8;
}

void SpyServerClient::_slot_handle_gain(i32 gain)
{
  mSettings.gain = gain;

  if (mSpyServerHandler != nullptr && !mSpyServerHandler->set_gain(mSettings.gain))
  {
    std::cerr << "Failed to set gain\n";
    return;
  }
}

void SpyServerClient::_slot_handle_autogain(i32 d)
{
  (void)d;
  const bool x = cbAutoGain->isChecked();
  mSettings.auto_gain = x;

  if (mIsConnected && mSpyServerHandler != nullptr)
  {
    mSpyServerHandler->set_gain_mode((bool)d != x, 0);
  }
}

// void SpyServerClient::connect_on()
// {
//   onConnect.store(true);
// }

static constexpr std::array<const f32, 256> cConvTable =
{
  -128 / 128.0, -127 / 128.0, -126 / 128.0, -125 / 128.0, -124 / 128.0, -123 / 128.0, -122 / 128.0, -121 / 128.0, -120 / 128.0, -119 / 128.0, -118 / 128.0, -117 / 128.0, -116 / 128.0, -115 / 128.0, -114 / 128.0, -113 / 128.0, -112 / 128.0,
  -111 / 128.0, -110 / 128.0, -109 / 128.0, -108 / 128.0, -107 / 128.0, -106 / 128.0, -105 / 128.0, -104 / 128.0, -103 / 128.0, -102 / 128.0, -101 / 128.0, -100 / 128.0, -99 / 128.0, -98 / 128.0, -97 / 128.0, -96 / 128.0, -95 / 128.0,
  -94 / 128.0, -93 / 128.0, -92 / 128.0, -91 / 128.0, -90 / 128.0, -89 / 128.0, -88 / 128.0, -87 / 128.0, -86 / 128.0, -85 / 128.0, -84 / 128.0, -83 / 128.0, -82 / 128.0, -81 / 128.0, -80 / 128.0, -79 / 128.0, -78 / 128.0, -77 / 128.0,
  -76 / 128.0, -75 / 128.0, -74 / 128.0, -73 / 128.0, -72 / 128.0, -71 / 128.0, -70 / 128.0, -69 / 128.0, -68 / 128.0, -67 / 128.0, -66 / 128.0, -65 / 128.0, -64 / 128.0, -63 / 128.0, -62 / 128.0, -61 / 128.0, -60 / 128.0, -59 / 128.0,
  -58 / 128.0, -57 / 128.0, -56 / 128.0, -55 / 128.0, -54 / 128.0, -53 / 128.0, -52 / 128.0, -51 / 128.0, -50 / 128.0, -49 / 128.0, -48 / 128.0, -47 / 128.0, -46 / 128.0, -45 / 128.0, -44 / 128.0, -43 / 128.0, -42 / 128.0, -41 / 128.0,
  -40 / 128.0, -39 / 128.0, -38 / 128.0, -37 / 128.0, -36 / 128.0, -35 / 128.0, -34 / 128.0, -33 / 128.0, -32 / 128.0, -31 / 128.0, -30 / 128.0, -29 / 128.0, -28 / 128.0, -27 / 128.0, -26 / 128.0, -25 / 128.0, -24 / 128.0, -23 / 128.0,
  -22 / 128.0, -21 / 128.0, -20 / 128.0, -19 / 128.0, -18 / 128.0, -17 / 128.0, -16 / 128.0, -15 / 128.0, -14 / 128.0, -13 / 128.0, -12 / 128.0, -11 / 128.0, -10 / 128.0, -9 / 128.0, -8 / 128.0, -7 / 128.0, -6 / 128.0, -5 / 128.0,
  -4 / 128.0, -3 / 128.0, -2 / 128.0, -1 / 128.0, 0 / 128.0, 1 / 128.0, 2 / 128.0, 3 / 128.0, 4 / 128.0, 5 / 128.0, 6 / 128.0, 7 / 128.0, 8 / 128.0, 9 / 128.0, 10 / 128.0, 11 / 128.0, 12 / 128.0, 13 / 128.0, 14 / 128.0, 15 / 128.0,
  16 / 128.0, 17 / 128.0, 18 / 128.0, 19 / 128.0, 20 / 128.0, 21 / 128.0, 22 / 128.0, 23 / 128.0, 24 / 128.0, 25 / 128.0, 26 / 128.0, 27 / 128.0, 28 / 128.0, 29 / 128.0, 30 / 128.0, 31 / 128.0, 32 / 128.0, 33 / 128.0, 34 / 128.0,
  35 / 128.0, 36 / 128.0, 37 / 128.0, 38 / 128.0, 39 / 128.0, 40 / 128.0, 41 / 128.0, 42 / 128.0, 43 / 128.0, 44 / 128.0, 45 / 128.0, 46 / 128.0, 47 / 128.0, 48 / 128.0, 49 / 128.0, 50 / 128.0, 51 / 128.0, 52 / 128.0, 53 / 128.0,
  54 / 128.0, 55 / 128.0, 56 / 128.0, 57 / 128.0, 58 / 128.0, 59 / 128.0, 60 / 128.0, 61 / 128.0, 62 / 128.0, 63 / 128.0, 64 / 128.0, 65 / 128.0, 66 / 128.0, 67 / 128.0, 68 / 128.0, 69 / 128.0, 70 / 128.0, 71 / 128.0, 72 / 128.0,
  73 / 128.0, 74 / 128.0, 75 / 128.0, 76 / 128.0, 77 / 128.0, 78 / 128.0, 79 / 128.0, 80 / 128.0, 81 / 128.0, 82 / 128.0, 83 / 128.0, 84 / 128.0, 85 / 128.0, 86 / 128.0, 87 / 128.0, 88 / 128.0, 89 / 128.0, 90 / 128.0, 91 / 128.0,
  92 / 128.0, 93 / 128.0, 94 / 128.0, 95 / 128.0, 96 / 128.0, 97 / 128.0, 98 / 128.0, 99 / 128.0, 100 / 128.0, 101 / 128.0, 102 / 128.0, 103 / 128.0, 104 / 128.0, 105 / 128.0, 106 / 128.0, 107 / 128.0, 108 / 128.0, 109 / 128.0, 110 / 128.0,
  111 / 128.0, 112 / 128.0, 113 / 128.0, 114 / 128.0, 115 / 128.0, 116 / 128.0, 117 / 128.0, 118 / 128.0, 119 / 128.0, 120 / 128.0, 121 / 128.0, 122 / 128.0, 123 / 128.0, 124 / 128.0, 125 / 128.0, 126 / 128.0, 127 / 128.0
};

void SpyServerClient::slot_data_ready()
{
  while (mIsConnected && mRingBuffer1.get_ring_buffer_read_available() > 2 * mSettings.batchSize)
  {
    i32 numSamples = mRingBuffer1.get_data_from_ring_buffer(mByteBuffer.data(), 2 * mSettings.batchSize);
    assert(numSamples == 2 * mSettings.batchSize);
    numSamples /= 2;

    if (!mIsRunning)
    {
      continue;
    }

    if (mSettings.resample_ratio != 1)
    {
      for (i32 i = 0; i < numSamples; i++)
      {
        mConvBuffer[mConvIndex++] = cf32(cConvTable[mByteBuffer[2 * i + 0]], cConvTable[mByteBuffer[2 * i + 1]]);

        if (mConvIndex > mConvBufferSize)
        {
#ifdef HAVE_LIQUID
          u32 usedResultSamples = 0;
          resamp_crcf_execute_block(mLiquidResampler, mConvBuffer.data(), mConvBufferSize, mResampBuffer.data(), &usedResultSamples);
          assert(usedResultSamples <= mResampBuffer.size());
          mRingBuffer2.put_data_into_ring_buffer(mResampBuffer.data(), (i32)usedResultSamples);
#else
          for (i32 j = 0; j < 2048; j++)
          {
            const i16 inpBase = mMapTable_int[j];
            const f32 inpRatio = mMapTable_float[j];
            mResampBuffer[j] = mConvBuffer[inpBase + 1] * inpRatio + mConvBuffer[inpBase] * (1 - inpRatio);
          }
          mRingBuffer2.put_data_into_ring_buffer(mResampBuffer.data(), 2048);
#endif
          mConvBuffer[0] = mConvBuffer[mConvBufferSize];
          mConvIndex = 1;
        }
      }
    }
    else
    {
      // no resampling necessary
      assert((i32)mConvBuffer.size() >= numSamples);

      for (i32 i = 0; i < numSamples; i++)
      {
        mConvBuffer[i] = cf32(cConvTable[mByteBuffer[2 * i + 0]], cConvTable[mByteBuffer[2 * i + 1]]);
      }

      mRingBuffer2.put_data_into_ring_buffer(mConvBuffer.data(), numSamples);
    }
  }
}

void SpyServerClient::_slot_handle_checkTimer()
{
  // timedOut = true;
}

bool SpyServerClient::_check_and_cleanup_ip_address()
{
  QString addr = editIpAddress->text();

  // remove "sdr://" or similar from the front
  const i32 pos = addr.indexOf("://");
  if (pos >= 0)
  {
    addr = addr.mid(pos + 3);
  }

  const QStringList parts = addr.split(":");
  mSettings.ipAddress = parts[0];
  mSettings.basePort = parts.size() > 1 ? parts[1].toInt() : 5555;
  const QString addrStr = mSettings.ipAddress + ":" + QString::number(mSettings.basePort);
  editIpAddress->setText(addrStr);
  Settings::SpyServer::varIpAddress.write(addrStr);

  return true; // TODO: check address
}

void SpyServerClient::show()
{
  mFrame.show();
}

void SpyServerClient::hide()
{
  mFrame.hide();
}

bool SpyServerClient::isHidden()
{
  return mFrame.isHidden();
}

bool SpyServerClient::isFileInput()
{
  return false;
}

void SpyServerClient::setVFOFrequency(i32)
{
}

i32 SpyServerClient::getVFOFrequency()
{
  return 0;
}

void SpyServerClient::resetBuffer()
{
}

QString SpyServerClient::deviceName()
{
  return "SpyServer_NW"; // NW == network;
}
