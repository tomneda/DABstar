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

#include    "lime-handler.h"
#include    "xml-filewriter.h"
#include    "device-exceptions.h"
#include    "openfiledialog.h"

#define FIFO_SIZE   32768
static i16 localBuffer [4 * FIFO_SIZE];
lms_info_str_t limedevices [10];

LimeHandler::LimeHandler(QSettings *s,
                         const QString & recorderVersion):
                         myFrame (nullptr),
                         _I_Buffer (4* 1024 * 1024),
                         theFilter (5, 1560000 / 2, 2048000)
{
  this->limeSettings = s;
  this->recorderVersion = recorderVersion;
  setupUi(&myFrame);
  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  myFrame.show();

  filtering = false;

  limeSettings->beginGroup ("limeSettings");
  currentDepth = limeSettings->value("filterDepth", 5).toInt();
  limeSettings->endGroup();
  filterDepth->setValue(currentDepth);
  theFilter.resize(currentDepth);
#ifdef  _WIN32
  const char *libraryString = "LimeSuite.dll";
#elif __APPLE__
  const char *libraryString = "/opt/local/lib/libLimeSuite.dylib";
#else
  const char *libraryString = "libLimeSuite.so";
#endif
  phandle = new QLibrary(libraryString);
  phandle->load();

  if (!phandle->isLoaded())
  {
    throw(std_exception_string("failed to open " + std::string (libraryString)));
  }
  else
    libraryLoaded = true;

  if (!load_limeFunctions())
  {
    throw(std_exception_string("could not load all required lib functions"));
  }

  //      From here we have a library available
  i32 ndevs = LMS_GetDeviceList(limedevices);
  if (ndevs == 0)
  {   // no devices found
    throw (std_exception_string("No lime device found"));
  }

  for (i32 i = 0; i < ndevs; i ++)
    fprintf (stderr, "device %s\n", limedevices[i]);

  i32 res = LMS_Open(&theDevice, nullptr, nullptr);
  if (res < 0)
  {  // some error
    throw(std_exception_string("failed to open device"));
  }

  res = LMS_Init(theDevice);
  if (res < 0)
  {  // some error
    LMS_Close(&theDevice);
    throw (std_exception_string("failed to initialize device"));
  }

  res = LMS_GetNumChannels(theDevice, LMS_CH_RX);
  if (res < 0)
  {  // some error
    LMS_Close(&theDevice);
    throw(std_exception_string("could not set number of channels"));
  }

  fprintf(stderr, "device %s supports %d channels\n", limedevices [0], res);

  res = LMS_EnableChannel(theDevice, LMS_CH_RX, 0, true);
  if (res < 0)
  {  // some error
    LMS_Close(theDevice);
    throw(std_exception_string("could not enable channels"));
  }

  res = LMS_SetSampleRate(theDevice, 2048000.0, 0);
  if (res < 0)
  {
    LMS_Close(theDevice);
    throw(std_exception_string("could not set samplerate"));
  }

  float_type host_Hz, rf_Hz;
  res = LMS_GetSampleRate(theDevice, LMS_CH_RX, 0, &host_Hz, &rf_Hz);

  fprintf (stderr, "samplerate = %f %f\n", (f32)host_Hz, (f32)rf_Hz);

  res = LMS_GetAntennaList (theDevice, LMS_CH_RX, 0, antennas);
  for (i32 i = 0; i < res; i ++)
    antennaList->addItem(QString(antennas[i]));

  limeSettings->beginGroup("limeSettings");
  QString antenne = limeSettings->value("antenna", "default").toString();
  save_gainSettings = limeSettings->value("save_gainSettings", 1).toInt() != 0;
  limeSettings->endGroup();

  i32 k = antennaList->findText(antenne);
    if (k != -1)
      antennaList->setCurrentIndex(k);

  connect(antennaList, SIGNAL(activated(int)), this, SLOT(setAntenna(int)));

  //  default antenna setting
  res = LMS_SetAntenna(theDevice, LMS_CH_RX, 0, antennaList->currentIndex());

  //  default frequency
  res = LMS_SetLOFrequency(theDevice, LMS_CH_RX, 0, 220000000.0);

  if (res < 0)
  {
    LMS_Close(theDevice);
    throw(std_exception_string("could not set LO frequency"));
  }

  res = LMS_SetLPFBW(theDevice, LMS_CH_RX, 0, 1536000.0);

  if (res < 0)
  {
    LMS_Close (theDevice);
    throw(std_exception_string("could not set bandwidth"));
  }

  LMS_SetGaindB(theDevice, LMS_CH_RX, 0, 50);

  LMS_Calibrate(theDevice, LMS_CH_RX, 0, 2500000.0, 0);

  limeSettings->beginGroup("limeSettings");
  k = limeSettings->value("gain", 50).toInt();
  limeSettings->endGroup();
  gainSelector->setValue(k);
  setGain(k);
  connect(gainSelector, SIGNAL(valueChanged(int)), this, SLOT(setGain(int)));
  connect(dumpButton, SIGNAL(clicked ()), this, SLOT(set_xmlDump()));
  connect(this, SIGNAL(new_gainValue(int)), gainSelector, SLOT(setValue(int)));
  connect(filterSelector, SIGNAL(stateChanged(int)), this, SLOT(set_filter(int)));
  xmlDumper = nullptr;
  dumping.store(false);
  running.store(false);
}

LimeHandler::~LimeHandler()
{
  stopReader();
  running.store(false);
  while (isRunning())
     usleep(100);
  myFrame.hide();
  limeSettings->beginGroup("limeSettings");
  limeSettings->setValue("antenna", antennaList->currentText());
  limeSettings->setValue("gain", gainSelector->value());
  limeSettings->setValue("filterDepth", filterDepth->value ());
  limeSettings->endGroup();
  LMS_Close(theDevice);
  if(libraryLoaded)
    delete(phandle);
}

void LimeHandler::setVFOFrequency(i32 f)
{
  LMS_SetLOFrequency(theDevice, LMS_CH_RX, 0, f);
}

i32 LimeHandler::getVFOFrequency()
{
  float_type freq;
  /*i32 res = */LMS_GetLOFrequency (theDevice, LMS_CH_RX, 0, &freq);
  return (i32)freq;
}

void LimeHandler::setGain(i32 g)
{
  float_type gg;
  LMS_SetGaindB(theDevice, LMS_CH_RX, 0, g);
  LMS_GetNormalizedGain(theDevice, LMS_CH_RX, 0, &gg);
  actualGain->display(gg);
}

void LimeHandler::setAntenna(i32 ind)
{
  (void)LMS_SetAntenna(theDevice, LMS_CH_RX, 0, ind);
}

void LimeHandler::set_filter(i32 c)
{
  (void)c;
  filtering = filterSelector->isChecked();
  fprintf(stderr, "filter set %s\n", filtering ? "on" : "off");
}

bool LimeHandler::restartReader(i32 freq)
{
  i32 res;

  if (isRunning())
    return true;

  vfoFrequency    = freq;
  if (save_gainSettings)
  {
    update_gainSettings(freq / MHz(1));
    setGain(gainSelector->value());
  }
  LMS_SetLOFrequency(theDevice, LMS_CH_RX, 0, freq);
  stream.isTx = false;
  stream.channel = 0;
  stream.fifoSize = FIFO_SIZE;
  stream.throughputVsLatency = 0.1;  // ???
  stream.dataFmt = lms_stream_t::LMS_FMT_I12;    // 12 bit ints

  res = LMS_SetupStream(theDevice, &stream);
  if (res < 0)
    return false;
  res = LMS_StartStream(&stream);
  if (res < 0)
    return false;

  start ();
  return true;
}

void LimeHandler::stopReader()
{
  if (!isRunning())
    return;
  close_xmlDump();
  if (save_gainSettings)
    record_gainSettings(vfoFrequency);

  running.store(false);
  while (isRunning())
    usleep (200);
  (void)LMS_StopStream(&stream);
  (void)LMS_DestroyStream(theDevice, &stream);
}

i32 LimeHandler::getSamples(cf32 *V, i32 size)
{
  std::complex<i16> *temp = make_vla(std::complex<i16>, size);

  i32 amount = _I_Buffer. get_data_from_ring_buffer(temp, size);
  if (filtering)
  {
    if (filterDepth->value() != currentDepth)
    {
      currentDepth = filterDepth->value ();
      theFilter. resize (currentDepth);
    }
    for (i32 i = 0; i < amount; i ++)
      V[i] = theFilter.Pass(cf32(real(temp[i]) / 2048.0, imag(temp[i]) / 2048.0));
  }
  else
    for (i32 i = 0; i < amount; i ++)
      V[i] = cf32(real(temp[i]) / 2048.0, imag(temp[i]) / 2048.0);

  if (dumping.load())
    xmlWriter->add(temp, amount);
  return amount;
}

i32 LimeHandler::Samples()
{
  return _I_Buffer.get_ring_buffer_read_available();
}

void LimeHandler::resetBuffer()
{
  _I_Buffer.flush_ring_buffer();
}

i16 LimeHandler::bitDepth()
{
  return 12;
}

QString LimeHandler::deviceName()
{
  return "limeSDR";
}

void LimeHandler::showErrors(i32 underrun, i32 overrun)
{
  underrunDisplay->display(underrun);
  overrunDisplay->display(overrun);
}


void    LimeHandler::run()
{
  i32 res;
  lms_stream_status_t streamStatus;
  i32 underruns  = 0;
  i32 overruns   = 0;
  i32 amountRead = 0;

  running.store(true);
  while (running.load())
  {
    res = LMS_RecvStream(&stream, localBuffer, FIFO_SIZE,  &meta, 1000);

    if (res > 0)
    {
      _I_Buffer.put_data_into_ring_buffer(localBuffer, res);
      amountRead += res;
      res = LMS_GetStreamStatus(&stream, &streamStatus);
      underruns += streamStatus.underrun;
      overruns  += streamStatus.overrun;
    }
    if (amountRead > 4 * 2048000)
    {
      amountRead = 0;
      showErrors (underruns, overruns);
      underruns = 0;
      overruns  = 0;
    }
  }
}

bool LimeHandler::load_limeFunctions()
{
  LMS_GetDeviceList = (pfn_LMS_GetDeviceList)
                      phandle->resolve("LMS_GetDeviceList");
  if (LMS_GetDeviceList == nullptr)
  {
    fprintf(stderr, "could not find LMS_GetdeviceList\n");
    return false;
  }
  LMS_Open = (pfn_LMS_Open)phandle->resolve("LMS_Open");
  if (LMS_Open == nullptr)
  {
    fprintf(stderr, "could not find LMS_Open\n");
    return false;
  }
  LMS_Close = (pfn_LMS_Close)phandle->resolve("LMS_Close");
  if (LMS_Close == nullptr)
  {
    fprintf(stderr, "could not find LMS_Close\n");
    return false;
  }
  LMS_Init = (pfn_LMS_Init)phandle->resolve("LMS_Init");
  if (LMS_Init == nullptr)
  {
    fprintf(stderr, "could not find LMS_Init\n");
    return false;
  }
  LMS_GetNumChannels = (pfn_LMS_GetNumChannels)
                       phandle->resolve("LMS_GetNumChannels");
  if (LMS_GetNumChannels == nullptr)
  {
     fprintf(stderr, "could not find LMS_GetNumChannels\n");
     return false;
  }
  LMS_EnableChannel = (pfn_LMS_EnableChannel)
                      phandle->resolve("LMS_EnableChannel");
  if (LMS_EnableChannel == nullptr)
  {
    fprintf(stderr, "could not find LMS_EnableChannel\n");
    return false;
  }
  LMS_SetSampleRate = (pfn_LMS_SetSampleRate)
                      phandle->resolve("LMS_SetSampleRate");
  if (LMS_SetSampleRate == nullptr)
  {
    fprintf(stderr, "could not find LMS_SetSampleRate\n");
    return false;
  }
  LMS_GetSampleRate = (pfn_LMS_GetSampleRate)
                      phandle->resolve("LMS_GetSampleRate");
  if (LMS_GetSampleRate == nullptr)
  {
    fprintf(stderr, "could not find LMS_GetSampleRate\n");
    return false;
  }
  LMS_SetLOFrequency = (pfn_LMS_SetLOFrequency)
                        phandle->resolve("LMS_SetLOFrequency");
  if (LMS_SetLOFrequency == nullptr)
  {
    fprintf(stderr, "could not find LMS_SetLOFrequency\n");
    return false;
  }
  LMS_GetLOFrequency = (pfn_LMS_GetLOFrequency)
                       phandle->resolve("LMS_GetLOFrequency");
  if (LMS_GetLOFrequency == nullptr)
  {
    fprintf(stderr, "could not find LMS_GetLOFrequency\n");
    return false;
  }
  LMS_GetAntennaList = (pfn_LMS_GetAntennaList)
                       phandle->resolve("LMS_GetAntennaList");
  if (LMS_GetAntennaList == nullptr)
  {
    fprintf(stderr, "could not find LMS_GetAntennaList\n");
    return false;
  }
  LMS_SetAntenna = (pfn_LMS_SetAntenna)phandle->resolve("LMS_SetAntenna");
  if (LMS_SetAntenna == nullptr)
  {
    fprintf(stderr, "could not find LMS_SetAntenna\n");
    return false;
  }
  LMS_GetAntenna = (pfn_LMS_GetAntenna)phandle->resolve("LMS_GetAntenna");
  if (LMS_GetAntenna == nullptr)
  {
    fprintf(stderr, "could not find LMS_GetAntenna\n");
    return false;
  }
  LMS_GetAntennaBW = (pfn_LMS_GetAntennaBW)
                     phandle->resolve("LMS_GetAntennaBW");
  if (LMS_GetAntennaBW == nullptr)
  {
    fprintf(stderr, "could not find LMS_GetAntennaBW\n");
    return false;
  }
  LMS_SetNormalizedGain = (pfn_LMS_SetNormalizedGain)
                          phandle->resolve("LMS_SetNormalizedGain");
  if (LMS_SetNormalizedGain == nullptr)
  {
    fprintf(stderr, "could not find LMS_SetNormalizedGain\n");
    return false;
  }
  LMS_GetNormalizedGain = (pfn_LMS_GetNormalizedGain)
                          phandle->resolve("LMS_GetNormalizedGain");
  if (LMS_GetNormalizedGain == nullptr)
  {
    fprintf(stderr, "could not find LMS_GetNormalizedGain\n");
    return false;
  }
  LMS_SetGaindB = (pfn_LMS_SetGaindB)phandle->resolve("LMS_SetGaindB");
  if (LMS_SetGaindB == nullptr)
  {
    fprintf(stderr, "could not find LMS_SetGaindB\n");
    return false;
  }
  LMS_GetGaindB = (pfn_LMS_GetGaindB)phandle->resolve("LMS_GetGaindB");
  if (LMS_GetGaindB == nullptr)
  {
    fprintf(stderr, "could not find LMS_GetGaindB\n");
    return false;
  }
  LMS_SetLPFBW = (pfn_LMS_SetLPFBW)phandle->resolve("LMS_SetLPFBW");
  if (LMS_SetLPFBW == nullptr)
  {
    fprintf(stderr, "could not find LMS_SetLPFBW\n");
    return false;
  }
  LMS_GetLPFBW = (pfn_LMS_GetLPFBW)phandle->resolve("LMS_GetLPFBW");
  if (LMS_GetLPFBW == nullptr)
  {
    fprintf(stderr, "could not find LMS_GetLPFBW\n");
    return false;
  }
  LMS_Calibrate = (pfn_LMS_Calibrate)phandle->resolve("LMS_Calibrate");
  if (LMS_Calibrate == nullptr)
  {
    fprintf(stderr, "could not find LMS_Calibrate\n");
    return false;
  }
  LMS_SetupStream = (pfn_LMS_SetupStream)phandle->resolve("LMS_SetupStream");
  if (LMS_SetupStream == nullptr)
  {
    fprintf(stderr, "could not find LMS_SetupStream\n");
    return false;
  }
  LMS_DestroyStream = (pfn_LMS_DestroyStream)phandle->resolve("LMS_DestroyStream");
  if (LMS_DestroyStream == nullptr)
  {
    fprintf(stderr, "could not find LMS_DestroyStream\n");
    return false;
  }
  LMS_StartStream = (pfn_LMS_StartStream)phandle->resolve("LMS_StartStream");
  if (LMS_StartStream == nullptr)
  {
    fprintf(stderr, "could not find LMS_StartStream\n");
    return false;
  }
  LMS_StopStream = (pfn_LMS_StopStream)phandle->resolve("LMS_StopStream");
  if (LMS_StopStream == nullptr)
  {
    fprintf(stderr, "could not find LMS_StopStream\n");
    return false;
  }
  LMS_RecvStream = (pfn_LMS_RecvStream)phandle->resolve("LMS_RecvStream");
  if (LMS_RecvStream == nullptr)
  {
    fprintf(stderr, "could not find LMS_RecvStream\n");
    return false;
  }
  LMS_GetStreamStatus = (pfn_LMS_GetStreamStatus)
                        phandle->resolve("LMS_GetStreamStatus");
  if (LMS_GetStreamStatus == nullptr)
  {
    fprintf(stderr, "could not find LMS_GetStreamStatus\n");
    return false;
  }

  return true;
}

void LimeHandler::set_xmlDump()
{
  if (xmlDumper == nullptr)
  {
    if(setup_xmlDump())
      dumpButton->setText("writing");
  }
  else
  {
    close_xmlDump();
  }
}

bool LimeHandler::setup_xmlDump()
{
  OpenFileDialog filenameFinder(limeSettings);
  xmlDumper = filenameFinder.open_raw_dump_xmlfile_ptr(deviceName());
  if (xmlDumper == nullptr)
    return false;

  xmlWriter = new XmlFileWriter(xmlDumper,
                                bitDepth(),
                                "int16",
                                2048000,
                                getVFOFrequency(),
                                "lime",
                                "1",
                                recorderVersion);
    dumping.store(true);

    return true;
}

void LimeHandler::close_xmlDump()
{
  dumpButton->setText("Dump");
  if (xmlDumper == nullptr)   // this can happen !!
    return;
  dumping.store(false);
  usleep(1000);
  xmlWriter->computeHeader();
  delete xmlWriter;
  fclose(xmlDumper);
  xmlDumper = nullptr;
}

void LimeHandler::show()
{
  myFrame.show();
}

void LimeHandler::hide()
{
  myFrame.hide();
}

bool LimeHandler::isHidden()
{
  return myFrame.isHidden();
}

void LimeHandler::record_gainSettings(i32 key)
{
  i32 gainValue = gainSelector->value();
  QString theValue = QString::number(gainValue);

  limeSettings->beginGroup("limeSettings");
  limeSettings->setValue(QString::number(key), theValue);
  limeSettings->endGroup();
}

void LimeHandler::update_gainSettings(i32 key)
{
  i32 gainValue;

  limeSettings->beginGroup("limeSettings");
  gainValue = limeSettings->value(QString::number(key), -1).toInt();
  limeSettings->endGroup();

  if (gainValue == -1)
    return;

  gainSelector->blockSignals(true);
  new_gainValue(gainValue);
  while (gainSelector->value() != gainValue)
    usleep(1000);
  actualGain->display(gainValue);
  gainSelector->blockSignals(false);
}

bool LimeHandler::isFileInput()
{
  return false;
}
