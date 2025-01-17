/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation version 2 of the License.
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

#include  <QSettings>
#include  <QTime>
#include  <QLabel>
#include  <QFileDialog>
#include  "sdrplay-handler-v3.h"
#include  "sdrplay-commands.h"
#include  "xml-filewriter.h"
#include  "setting-helper.h"

//	The Rsp's
#include  "Rsp-device.h"
#include  "Rsp1-handler.h"
#include  "Rsp1A-handler.h"
#include  "Rsp2-handler.h"
#include  "RspDuo-handler.h"
#include  "RspDx-handler.h"

#include  "device-exceptions.h"
#include  <chrono>
#include <thread>

std::string errorMessage(int errorCode)
{
  switch (errorCode)
  {
  case  1: return {"Could not fetch library"};
  case  2: return {"error in fetching functions from library"};
  case  3: return {"sdrplay_api_Open failed"};
  case  4: return {"could not open sdrplay_api_ApiVersion"};
  case  5: return {"API versions do not match"};
  case  6: return {"sdrplay_api_GetDevices failed"};
  case  7: return {"no valid SDRplay device found"};
  case  8: return {"sdrplay_api_SelectDevice failed"};
  case  9: return {"sdrplay_api_GetDeviceParams failed"};
  case 10: return {"sdrplay_api+GetDeviceParams returns null"};
  default: return {"unidentified error with sdrplay device"};
  }
  return "";
}

SdrPlayHandler_v3::SdrPlayHandler_v3(QSettings * s, const QString & recorderVersion)
  : _I_Buffer(4 * 1024 * 1024)
  , myFrame(nullptr)
{
  sdrplaySettings = s;
  this->recorderVersion = recorderVersion;
  sdrplaySettings->beginGroup("sdrplaySettings_v3");
  int x = sdrplaySettings->value("position-x", 100).toInt();
  int y = sdrplaySettings->value("position-y", 100).toInt();
  sdrplaySettings->endGroup();

  setupUi(&myFrame);

  myFrame.setFixedSize(myFrame.geometry().size());
  myFrame.move(QPoint(x, y));
  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  myFrame.show();

  slot_overload_detected(false);

  antennaSelector->hide();
  //	denominator		= 2048;	// default

  xmlDumper = nullptr;
  dumping.store(false);
  //	See if there are settings from previous incarnations
  //	and config stuff

  sdrplaySettings->beginGroup("sdrplaySettings_v3");

  GRdBSelector->setValue(sdrplaySettings->value("sdrplay-ifgrdb", 20).toInt());
  GRdBValue = GRdBSelector->value();

  lnaGainSetting->setValue(sdrplaySettings->value("sdrplay-lnastate", 4).toInt());

  lnaState = lnaGainSetting->value();

  ppmControl->setValue(sdrplaySettings->value("sdrplay-ppm", 0.0).toDouble());
  ppmValue = ppmControl->value();

  agcMode = sdrplaySettings->value("sdrplay-agcMode", 0).toInt() != 0;
  if (agcMode)
  {
    agcControl->setChecked(true);
    GRdBSelector->setEnabled(false);
    gainsliderLabel->setEnabled(false);
  }

  biasT = sdrplaySettings->value("biasT_selector", 0).toInt() != 0;
  if (biasT)
    biasT_selector->setChecked(true);

  notch = sdrplaySettings->value("notch_selector", 0).toInt() != 0;
  if (notch)
    notch_selector->setChecked(true);

  sdrplaySettings->endGroup();



  //	and be prepared for future changes in the settings
  connect(GRdBSelector, qOverload<int>(&QSpinBox::valueChanged), this, &SdrPlayHandler_v3::set_ifgainReduction);
  connect(lnaGainSetting, qOverload<int>(&QSpinBox::valueChanged), this, &SdrPlayHandler_v3::set_lnagainReduction);
  connect(agcControl, &QCheckBox::stateChanged, this, &SdrPlayHandler_v3::set_agcControl);
  connect(ppmControl, SIGNAL(valueChanged(double)), this, SLOT(set_ppmControl(double)));
  connect(dumpButton, &QPushButton::clicked, this, &SdrPlayHandler_v3::set_xmlDump);
  connect(biasT_selector, &QCheckBox::stateChanged, this, &SdrPlayHandler_v3::set_biasT);
  connect(notch_selector, &QCheckBox::stateChanged, this, &SdrPlayHandler_v3::set_notch);
  connect(antennaSelector, &QComboBox::textActivated, this, &SdrPlayHandler_v3::set_selectAntenna);

  vfoFrequency = MHz (220);
  failFlag.store(false);
  successFlag.store(false);
  errorCode = 0;
  start();
  while (!failFlag.load() && !successFlag.load() && isRunning())
  {
    usleep(1000);
  }
  if (failFlag.load())
  {
    while (isRunning())
    {
      usleep(1000);
    }
    throw std_exception_string(errorMessage(errorCode));
  }

  fprintf(stderr, "setup sdrplay v3 seems successfull\n");
}

SdrPlayHandler_v3::~SdrPlayHandler_v3()
{
  delete theRsp;

  threadRuns.store(false);
  while (isRunning())
  {
    usleep(1000);
  }
  //	thread should be stopped by now
  myFrame.hide();
  sdrplaySettings->beginGroup("sdrplaySettings_v3");
  sdrplaySettings->setValue("position-x", myFrame.pos().x());
  sdrplaySettings->setValue("position-y", myFrame.pos().y());

  sdrplaySettings->setValue("sdrplay-ppm", ppmControl->value());
  sdrplaySettings->setValue("sdrplay-ifgrdb", GRdBSelector->value());
  sdrplaySettings->setValue("sdrplay-lnastate", lnaGainSetting->value());
  sdrplaySettings->setValue("sdrplay-agcMode", agcControl->isChecked() ? 1 : 0);
  sdrplaySettings->setValue("biasT_selector", biasT_selector->isChecked() ? 1 : 0);
  sdrplaySettings->setValue("notch_selector", notch_selector->isChecked() ? 1 : 0);
  sdrplaySettings->endGroup();
  sdrplaySettings->sync();
}

/////////////////////////////////////////////////////////////////////////
//	Implementing the interface
/////////////////////////////////////////////////////////////////////////

//int32_t	sdrplayHandler_v3::defaultFrequency	() {
//	return MHz (220);
//}

int32_t SdrPlayHandler_v3::getVFOFrequency()
{
  return vfoFrequency;
}

bool SdrPlayHandler_v3::restartReader(int32_t newFreq)
{
  restartRequest r(newFreq);

  if (receiverRuns.load())
  {
    return true;
  }
  vfoFrequency = newFreq;
  return messageHandler(&r);
}

void SdrPlayHandler_v3::stopReader()
{
  stopRequest r;
  close_xmlDump();
  if (!receiverRuns.load())
  {
    return;
  }
  messageHandler(&r);
}

//
int32_t SdrPlayHandler_v3::getSamples(cmplx * V, int32_t size)
{
  static constexpr float denominator = (float)(1 << (nrBits-1));

  auto * const temp = make_vla(std::complex<int16_t>, size);

  const int amount = _I_Buffer.get_data_from_ring_buffer(temp, size);

  for (int i = 0; i < amount; i++)
  {
    V[i] = cmplx((float)real(temp[i]) / denominator, (float)imag(temp[i]) / denominator);
  }

  if (dumping.load())
  {
    xmlWriter->add(temp, amount);
  }
  return amount;
}

int32_t SdrPlayHandler_v3::Samples()
{
  return _I_Buffer.get_ring_buffer_read_available();
}

void SdrPlayHandler_v3::resetBuffer()
{
  _I_Buffer.flush_ring_buffer();
}

int16_t SdrPlayHandler_v3::bitDepth()
{
  return nrBits;
}

QString SdrPlayHandler_v3::deviceName()
{
  return deviceModel;
}

///////////////////////////////////////////////////////////////////////////
//	Handling the GUI
//////////////////////////////////////////////////////////////////////
//
//	Since the daemon is not threadproof, we have to package the
//	actual interface into its own thread.
//	Communication with that thread is synchronous!
//

void SdrPlayHandler_v3::set_lnabounds(int low, int high)
{
  lnaGainSetting->setRange(low, high);
}

void SdrPlayHandler_v3::set_deviceName(const QString & s)
{
  deviceModel = s;
  deviceLabel->setText(s);
}

void SdrPlayHandler_v3::set_serial(const QString & s)
{
  serialNumber->setText(s);
}

void SdrPlayHandler_v3::set_apiVersion(float version)
{
  QString v = QString::number(version, 'r', 2);
  api_version->display(v);
}

void SdrPlayHandler_v3::show_lnaGain(int g)
{
  lnaGRdBDisplay->display(g);
}

void SdrPlayHandler_v3::set_ifgainReduction(int GRdB)
{
  GRdBRequest r(GRdB);

  if (!receiverRuns.load())
  {
    return;
  }
  messageHandler(&r);
}

void SdrPlayHandler_v3::set_lnagainReduction(int lnaState)
{
  lnaRequest r(lnaState);

  if (!receiverRuns.load())
  {
    return;
  }
  messageHandler(&r);
}

void SdrPlayHandler_v3::set_agcControl(int dummy)
{
  bool agcMode = agcControl->isChecked();
  agcRequest r(agcMode, -20);
  (void)dummy;
  messageHandler(&r);

  GRdBSelector->setEnabled(!agcMode);
  gainsliderLabel->setEnabled(!agcMode);

  if (!agcMode)
  {
    GRdBRequest r2(GRdBSelector->value());
    messageHandler(&r2);
  }
}

void SdrPlayHandler_v3::set_ppmControl(double ppm)
{
  ppmRequest r(ppm);
  messageHandler(&r);
}

void SdrPlayHandler_v3::set_biasT(int v)
{
  (void)v;
  biasT_Request r(biasT_selector->isChecked() ? 1 : 0);
  messageHandler(&r);
  sdrplaySettings->beginGroup("sdrplaySettings_v3");
  sdrplaySettings->setValue("biasT_selector", biasT_selector->isChecked() ? 1 : 0);
  sdrplaySettings->endGroup();
}

void SdrPlayHandler_v3::set_notch(int v)
{
  (void)v;
  notch_Request r(notch_selector->isChecked() ? 1 : 0);
  messageHandler(&r);
  sdrplaySettings->beginGroup("sdrplaySettings_v3");
  sdrplaySettings->setValue("biasT_selector", notch_selector->isChecked() ? 1 : 0);
  sdrplaySettings->endGroup();
}

void SdrPlayHandler_v3::set_selectAntenna(const QString & s)
{
  messageHandler(new antennaRequest(s == "Antenna A" ? 'A' : s == "Antenna B" ? 'B' : 'C'));
}

void SdrPlayHandler_v3::set_xmlDump()
{
  if (xmlDumper == nullptr)
  {
    if (setup_xmlDump())
    {
      dumpButton->setText("writing");
    }
  }
  else
  {
    close_xmlDump();
    dumpButton->setText("Dump");
  }
}

//
////////////////////////////////////////////////////////////////////////
//	showing data
////////////////////////////////////////////////////////////////////////
void SdrPlayHandler_v3::set_antennaSelect(int n)
{
  if (n > 0)
  {
    antennaSelector->addItem("Antenna B");
    if (n > 1)
    {
      antennaSelector->addItem("Antenna C");
    }
    antennaSelector->show();
  }
  else
  {
    antennaSelector->hide();
  }
}

static inline bool isValid(QChar c)
{
  return c.isLetterOrNumber() || (c == '-');
}

bool SdrPlayHandler_v3::setup_xmlDump()
{
  QTime theTime;
  QDate theDate;
  QString saveDir = sdrplaySettings->value(sSettingSampleStorageDir, QDir::homePath()).toString();
  if ((saveDir != "") && (!saveDir.endsWith("/")))
  {
    saveDir += "/";
  }

  QString channel = sdrplaySettings->value("channel", "xx").toString();
  QString timeString = theDate.currentDate().toString() + "-" + theTime.currentTime().toString();
  for (int i = 0; i < timeString.length(); i++)
  {
    if (!isValid(timeString.at(i)))
    {
      timeString.replace(i, 1, "-");
    }
  }

  const bool useNativeFileDialog = SettingHelper::get_instance().read(SettingHelper::cbUseNativeFileDialog).toBool();
  QString suggestedFileName = saveDir + deviceModel + "-" + channel + "-" + timeString;
  QString fileName = QFileDialog::getSaveFileName(nullptr, tr("Save file ..."), suggestedFileName + ".uff", tr("Xml (*.uff)"), nullptr,
                                                  (useNativeFileDialog ? QFileDialog::Options() : QFileDialog::DontUseNativeDialog));
  fileName = QDir::toNativeSeparators(fileName);
  xmlDumper = fopen(fileName.toUtf8().data(), "wb");
  if (xmlDumper == nullptr)
  {
    return false;
  }

  xmlWriter = new XmlFileWriter(xmlDumper, nrBits, "int16", 2048000, vfoFrequency, "SDRplay", deviceModel, recorderVersion);
  dumping.store(true);

  QString dumper = QDir::fromNativeSeparators(fileName);
  int x = dumper.lastIndexOf("/");
  saveDir = dumper.remove(x, dumper.size() - x);
  sdrplaySettings->setValue(sSettingSampleStorageDir, saveDir);
  return true;
}

void SdrPlayHandler_v3::close_xmlDump()
{
  if (xmlDumper == nullptr)
  {  // this can happen !!
    return;
  }
  dumping.store(false);
  usleep(1000);
  xmlWriter->computeHeader();
  delete xmlWriter;
  fclose(xmlDumper);
  xmlDumper = nullptr;
}
//
///////////////////////////////////////////////////////////////////////
//	the real controller starts here
///////////////////////////////////////////////////////////////////////

bool SdrPlayHandler_v3::messageHandler(generalCommand * r)
{
  server_queue.push(r);
  serverjobs.release(1);
  while (!r->waiter.tryAcquire(1, 1000))
  {
    if (!threadRuns.load())
    {
      return false;
    }
  }
  return true;
}

static void StreamACallback(short * xi, short * xq, sdrplay_api_StreamCbParamsT * params, unsigned int numSamples, unsigned int reset, void * cbContext)
{
  SdrPlayHandler_v3 * p = static_cast<SdrPlayHandler_v3 *> (cbContext);
  std::complex<int16_t> localBuf[numSamples];

  (void)params;
  if (reset)
  {
    return;
  }
  if (!p->receiverRuns.load())
  {
    return;
  }

  for (int i = 0; i < (int)numSamples; i++)
  {
    std::complex<int16_t> symb = std::complex<int16_t>(xi[i], xq[i]);
    localBuf[i] = symb;
  }
  p->_I_Buffer.put_data_into_ring_buffer(localBuf, numSamples);
}

static void StreamBCallback(short * xi, short * xq, sdrplay_api_StreamCbParamsT * params, unsigned int numSamples, unsigned int reset, void * cbContext)
{
  (void)xi;
  (void)xq;
  (void)params;
  (void)cbContext;
  if (reset)
  {
    printf("sdrplay_api_StreamBCallback: numSamples=%d\n", numSamples);
  }
}

static void EventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT * params, void * cbContext)
{
  SdrPlayHandler_v3 * p = static_cast<SdrPlayHandler_v3 *> (cbContext);
  (void)tuner;
  switch (eventId)
  {
    case sdrplay_api_GainChange: emit p->signal_tuner_gain(params->gainParams.currGain, params->gainParams.lnaGRdB);
      break;

    case sdrplay_api_PowerOverloadChange: p->update_PowerOverload(params);
      break;

    default: fprintf(stderr, "event %d\n", eventId);
      break;
  }
}

void SdrPlayHandler_v3::update_PowerOverload(sdrplay_api_EventParamsT * params)
{
  sdrplay_api_Update(chosenDevice->dev, chosenDevice->tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck, sdrplay_api_Update_Ext1_None);
  emit signal_overload_detected(params->powerOverloadParams.powerOverloadChangeType == sdrplay_api_Overload_Detected);
}

void SdrPlayHandler_v3::run()
{
  sdrplay_api_ErrT err;
  sdrplay_api_DeviceT devs[6];
  uint32_t ndev = 0;
  startupCnt = 8;  // reconnect retry time in seconds

  threadRuns.store(false);
  receiverRuns.store(false);

  chosenDevice = nullptr;

  connect(this, &SdrPlayHandler_v3::signal_overload_detected, this, &SdrPlayHandler_v3::slot_overload_detected);
  connect(this, &SdrPlayHandler_v3::signal_tuner_gain, this, &SdrPlayHandler_v3::slot_tuner_gain);

  const char *libraryString = "sdrplay_api";
  phandle = new QLibrary(libraryString);
  phandle->load();

  if (!phandle->isLoaded())
  {
    fprintf(stderr,"failed to open %s\n", libraryString);
    failFlag.store(true);
    errorCode = 1;
    return;
  }
  //	load the functions
  if (!loadFunctions())
  {
    failFlag.store(true);
    errorCode = 2;
    return;
  }
  fprintf(stdout, "SDRPLAY functions loaded\n");

  //	try to open the API
  err = sdrplay_api_Open();
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "sdrplay_api_Open failed %s\n", sdrplay_api_GetErrorString(err));
    failFlag.store(true);
    errorCode = 3;
    return;
  }

  fprintf(stderr, "api opened\n");

  //	Check API versions match
  err = sdrplay_api_ApiVersion(&apiVersion);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "sdrplay_api_ApiVersion failed %s\n", sdrplay_api_GetErrorString(err));
    errorCode = 4;
    goto closeAPI;
  }

  if (apiVersion < 3.05)
  {
    //	if (apiVersion < (SDRPLAY_API_VERSION - 0.01)) {
    fprintf(stderr, "API versions don't match (local=%.2f dll=%.2f)\n", SDRPLAY_API_VERSION, apiVersion);
    errorCode = 5;
    goto closeAPI;
  }

  fprintf(stderr, "api version %f detected\n", apiVersion);
  //
  //	lock API while device selection is performed
  sdrplay_api_LockDeviceApi();
  do
  {
    int s = sizeof(devs) / sizeof(sdrplay_api_DeviceT);
    err = sdrplay_api_GetDevices(devs, &ndev, s);

    if (err != sdrplay_api_Success)
    {
      fprintf(stderr, "sdrplay_api_GetDevices failed %s\n", sdrplay_api_GetErrorString(err));
      errorCode = 6;
      goto unlockDevice_closeAPI;
    }

    /*
     * If this instance is restarted too fast then the call to sdrplay_api_GetDevices() gives no connected devices back.
     * If waiting some seconds (3.. 4s) then it works again.
     * Maybe there is a more suitable way doing this.
     * Also, not known how it works with more than one physical SDRPlay device is connected.
     */
    if (ndev == 0)
    {
      if (--startupCnt <= 0) // no more waiting a further second?
      {
        errorCode = 7;
        fprintf(stderr, "no valid device found, give up\n");
        goto unlockDevice_closeAPI;
      }
      fprintf(stderr, "no valid device found, try again (%d tries left)\n", startupCnt);
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
  while (ndev == 0);

  fprintf(stderr, "%d devices detected\n", ndev);
  chosenDevice = &devs[0];
  err = sdrplay_api_SelectDevice(chosenDevice);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "sdrplay_api_SelectDevice failed %s\n", sdrplay_api_GetErrorString(err));
    errorCode = 8;
    goto unlockDevice_closeAPI;
  }
  //
  //	assign callback functions
  cbFns.StreamACbFn = StreamACallback;
  cbFns.StreamBCbFn = StreamBCallback;
  cbFns.EventCbFn = EventCallback;

  //	we have a device, unlock
  sdrplay_api_UnlockDeviceApi();
  //
  hwVersion = devs[0].hwVer;
  set_serial(devs[0].SerNo);
  set_apiVersion(apiVersion);
  //
  try
  {
    switch (hwVersion)
    {
      case SDRPLAY_RSP1_ID:

	    set_deviceName("RSP1");
        biasT_selector->setEnabled (false);
        notch_selector->setEnabled (false);
        theRsp	= new Rsp1_handler(this, chosenDevice, MHz (174), agcMode, lnaState, GRdBValue, ppmValue);
	    break;
      case SDRPLAY_RSP1A_ID:
	    set_deviceName("RSP1A");
	    theRsp = new Rsp1A_handler(this, chosenDevice, MHz (174), agcMode, lnaState, GRdBValue, biasT, notch, ppmValue);
	    break;
      case SDRPLAY_RSP1B_ID:
	    set_deviceName("RSP1B");
	    theRsp = new Rsp1A_handler(this, chosenDevice, MHz (174), agcMode, lnaState, GRdBValue, biasT, notch, ppmValue);
	    break;
      case SDRPLAY_RSP2_ID:
	    set_deviceName("RSP2");
    	theRsp = new Rsp2_handler(this, chosenDevice, MHz (174), agcMode, lnaState, GRdBValue, biasT, notch, ppmValue);
    	break;
      case SDRPLAY_RSPdx_ID:
	    set_deviceName("RSPdx");
    	theRsp = new RspDx_handler(this, chosenDevice, MHz (174), agcMode, lnaState, GRdBValue, biasT, notch, ppmValue);
    	break;
      case SDRPLAY_RSPdxR2_ID:
	    set_deviceName("RSPdxR2");
    	theRsp = new RspDx_handler(this, chosenDevice, MHz (174), agcMode, lnaState, GRdBValue, biasT, notch, ppmValue);
    	break;
      case SDRPLAY_RSPduo_ID:
	    set_deviceName("RSPduo");
    	theRsp = new RspDuo_handler(this, chosenDevice, MHz (174), agcMode, lnaState, GRdBValue, biasT, notch, ppmValue);
    	break;
      default:
    	theRsp = new Rsp_device(this, chosenDevice, MHz (174), agcMode, lnaState, GRdBValue, ppmValue);
    	break;
    }
  }
  catch (int e)
  {
    goto closeAPI;
  }

  threadRuns.store(true);       // it seems we can do some work
  successFlag.store(true);
  while (threadRuns.load())
  {
    while (!serverjobs.tryAcquire(1, 1000))
    {
      if (!threadRuns.load())
      {
        goto normal_exit;
      }
    }

    //	here we could assert that the server_queue is not empty
    //	Note that we emulate synchronous calling, so
    //	we signal the caller when we are done
    switch (server_queue.front()->cmd)
    {
    case RESTART_REQUEST:
    {
      restartRequest * p = (restartRequest *)(server_queue.front());
      server_queue.pop();
      fprintf(stderr, "restart request\n");
      p->result = theRsp->restart(p->freq);
      receiverRuns.store(true);
      p->waiter.release(1);
      break;
    }

    case STOP_REQUEST:
    {
      stopRequest * p = (stopRequest *)(server_queue.front());
      server_queue.pop();
      receiverRuns.store(false);
      p->waiter.release(1);
      break;
    }

    case AGC_REQUEST:
    {
      agcRequest * p = (agcRequest *)(server_queue.front());
      server_queue.pop();
      p->result = theRsp->set_agc(p->setPoint, p->agcMode);
      p->waiter.release(1);
      break;
    }

    case GRDB_REQUEST:
    {
      GRdBRequest * p = (GRdBRequest *)(server_queue.front());
      server_queue.pop();
      p->result = theRsp->set_GRdB(p->GRdBValue);
      p->waiter.release(1);
      break;
    }

    case PPM_REQUEST:
    {
      ppmRequest * p = (ppmRequest *)(server_queue.front());
      server_queue.pop();
      p->result = theRsp->set_ppm(p->ppmValue);
      p->waiter.release(1);
      break;
    }

    case LNA_REQUEST:
    {
      lnaRequest * p = (lnaRequest *)(server_queue.front());
      server_queue.pop();
      p->result = theRsp->set_lna(p->lnaState);
      p->waiter.release(1);
      break;
    }

    case ANTENNASELECT_REQUEST:
    {
      antennaRequest * p = (antennaRequest *)(server_queue.front());
      server_queue.pop();
      p->result = theRsp->set_antenna(p->antenna);
      p->waiter.release(1);
      break;
    }

    case BIAS_T_REQUEST:
    {
      biasT_Request * p = (biasT_Request *)(server_queue.front());
      server_queue.pop();
      p->result = theRsp->set_biasT(p->checked);
      p->waiter.release(1);
      break;
    }

    case NOTCH_REQUEST:
    {
      notch_Request * p = (notch_Request *)(server_queue.front());
      server_queue.pop();
      p->result = theRsp->set_notch(p->checked);
      p->waiter.release(1);
      break;
    }

    default:    // cannot happen
      fprintf(stderr, "Helemaal fout\n");
      break;
    }
  }

normal_exit:
  err = sdrplay_api_Uninit(chosenDevice->dev);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "sdrplay_api_Uninit failed %s\n", sdrplay_api_GetErrorString(err));
  }

  err = sdrplay_api_ReleaseDevice(chosenDevice);
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "sdrplay_api_ReleaseDevice failed %s\n", sdrplay_api_GetErrorString(err));
  }

  //	sdrplay_api_UnlockDeviceApi	(); ??
  sdrplay_api_Close();
  if (err != sdrplay_api_Success)
  {
    fprintf(stderr, "sdrplay_api_Close failed %s\n", sdrplay_api_GetErrorString(err));
  }

  if(phandle) delete(phandle);
  fprintf(stderr, "library released, ready to stop thread\n");
  msleep(200);
  return;

unlockDevice_closeAPI:
  sdrplay_api_UnlockDeviceApi();
closeAPI:
  failFlag.store(true);
  sdrplay_api_ReleaseDevice(chosenDevice);
  sdrplay_api_Close();
  if(phandle) delete(phandle);
  fprintf(stderr, "SDRPlay API closed\n");
}

/////////////////////////////////////////////////////////////////////////////
//	handling the library
/////////////////////////////////////////////////////////////////////////////
bool SdrPlayHandler_v3::loadFunctions()
{
  sdrplay_api_Open = (sdrplay_api_Open_t)phandle->resolve("sdrplay_api_Open");
  if ((void *)sdrplay_api_Open == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_Open\n");
    return false;
  }

  sdrplay_api_Close = (sdrplay_api_Close_t)phandle->resolve("sdrplay_api_Close");
  if (sdrplay_api_Close == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_Close\n");
    return false;
  }

  sdrplay_api_ApiVersion = (sdrplay_api_ApiVersion_t)phandle->resolve("sdrplay_api_ApiVersion");
  if (sdrplay_api_ApiVersion == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_ApiVersion\n");
    return false;
  }

  sdrplay_api_LockDeviceApi = (sdrplay_api_LockDeviceApi_t)phandle->resolve("sdrplay_api_LockDeviceApi");
  if (sdrplay_api_LockDeviceApi == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_LockdeviceApi\n");
    return false;
  }

  sdrplay_api_UnlockDeviceApi = (sdrplay_api_UnlockDeviceApi_t)phandle->resolve("sdrplay_api_UnlockDeviceApi");
  if (sdrplay_api_UnlockDeviceApi == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_UnlockdeviceApi\n");
    return false;
  }

  sdrplay_api_GetDevices = (sdrplay_api_GetDevices_t)phandle->resolve("sdrplay_api_GetDevices");
  if (sdrplay_api_GetDevices == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_GetDevices\n");
    return false;
  }

  sdrplay_api_SelectDevice = (sdrplay_api_SelectDevice_t)phandle->resolve("sdrplay_api_SelectDevice");
  if (sdrplay_api_SelectDevice == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_SelectDevice\n");
    return false;
  }

  sdrplay_api_ReleaseDevice = (sdrplay_api_ReleaseDevice_t)phandle->resolve("sdrplay_api_ReleaseDevice");
  if (sdrplay_api_ReleaseDevice == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_ReleaseDevice\n");
    return false;
  }

  sdrplay_api_GetErrorString = (sdrplay_api_GetErrorString_t)phandle->resolve("sdrplay_api_GetErrorString");
  if (sdrplay_api_GetErrorString == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_GetErrorString\n");
    return false;
  }

  sdrplay_api_GetLastError = (sdrplay_api_GetLastError_t)phandle->resolve("sdrplay_api_GetLastError");
  if (sdrplay_api_GetLastError == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_GetLastError\n");
    return false;
  }

  sdrplay_api_DebugEnable = (sdrplay_api_DebugEnable_t)phandle->resolve("sdrplay_api_DebugEnable");
  if (sdrplay_api_DebugEnable == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_DebugEnable\n");
    return false;
  }

  sdrplay_api_GetDeviceParams = (sdrplay_api_GetDeviceParams_t)phandle->resolve("sdrplay_api_GetDeviceParams");
  if (sdrplay_api_GetDeviceParams == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_GetDeviceParams\n");
    return false;
  }

  sdrplay_api_Init = (sdrplay_api_Init_t)phandle->resolve("sdrplay_api_Init");
  if (sdrplay_api_Init == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_Init\n");
    return false;
  }

  sdrplay_api_Uninit = (sdrplay_api_Uninit_t)phandle->resolve("sdrplay_api_Uninit");
  if (sdrplay_api_Uninit == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_Uninit\n");
    return false;
  }

  sdrplay_api_Update = (sdrplay_api_Update_t)phandle->resolve("sdrplay_api_Update");
  if (sdrplay_api_Update == nullptr)
  {
    fprintf(stderr, "Could not find sdrplay_api_Update\n");
    return false;
  }

  sdrplay_api_SwapRspDuoActiveTuner = (sdrplay_api_SwapRspDuoActiveTuner_t)phandle->resolve("sdrplay_api_SwapRspDuoActiveTuner");
  if (sdrplay_api_SwapRspDuoActiveTuner == nullptr)
  {
	fprintf (stderr, "could not find sdrplay_api_SwapRspDuoActiveTuner\n");
	return false;
  }

  return true;
}

void SdrPlayHandler_v3::show()
{
  myFrame.show();
}

void SdrPlayHandler_v3::hide()
{
  myFrame.hide();
}

bool SdrPlayHandler_v3::isHidden()
{
  return myFrame.isHidden();
}

void SdrPlayHandler_v3::setVFOFrequency(int32_t iFreq)
{
  restartReader(iFreq);
}

bool SdrPlayHandler_v3::isFileInput()
{
  return false;
}

void SdrPlayHandler_v3::slot_overload_detected(bool iOvlDetected)
{
  if (iOvlDetected)
  {
     lblOverload->setStyleSheet("QLabel {background-color : red; color: white}");
  }
  else
  {
    lblOverload->setStyleSheet("QLabel {background-color : #444444; color: #333333}");
  }
}

void SdrPlayHandler_v3::slot_tuner_gain(double gain, int g)
{
  tunerGain->setText(QString::number(gain, 'f', 0) + " dB");
  lnaGRdBDisplay->display(g);
}

