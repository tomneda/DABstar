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
#include  <cstdio> //printf
#include  <iostream>
#include  "soapy-handler.h"
#include  "soapy-converter.h"
#include  "dongleselect.h"
#include  "device-exceptions.h"

SoapyHandler::SoapyHandler(QSettings * soapySettings)
  : myFrame(nullptr)
{
  int deviceIndex = 0;
  std::vector<QString> deviceString;
  std::vector<QString> serialString;
  std::vector<QString> labelString;

  this->soapySettings = soapySettings;
  soapySettings->beginGroup("soapySettings");
  i32 x = soapySettings->value("position-x", 100).toInt();
  i32 y = soapySettings->value("position-y", 100).toInt();
  soapySettings->endGroup();
  setupUi(&myFrame);
  myFrame.move(QPoint(x, y));
  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  //myFrame.show();
  const auto results = SoapySDR::Device::enumerate();
  u32 length = (u32)results.size();

  if (length == 0)
  {
    throw (std_exception_string("Could not find soapy support"));
    return;
  }

  deviceString.resize(length);
  serialString.resize(length);
  labelString.resize(length);
  for (u32 i = 0; i < length; i++)
  {
    fprintf(stderr, "Found device #%d:\n", i);
    for (const auto &it : results[i])
    {
      fprintf(stderr, "  %s = %s\n", it.first.c_str(), it.second.c_str());
      if (QString(it.first.c_str()) == "driver")
      {
        deviceString[i] = QString(it.second.c_str());
      }
      if (QString(it.first.c_str()) == "serial")
      {
        serialString[i] = QString(it.second.c_str());
      }
      if (QString(it.first.c_str()) == "label")
      {
        labelString[i] = QString(it.second.c_str());
      }
    }
    fprintf(stderr, "\n");
  }

  if (length > 1)
  {
    dongleSelect deviceSelector;
    for (u32 i = 0; i < length; i++)
    {
      deviceSelector.addtoDongleList(labelString[i]);
    }
    deviceIndex = deviceSelector.QDialog::exec();
  }

  worker = nullptr;
  comboBox_1->hide();
  spinBox_0->hide();
  spinBox_1->hide();
  spinBox_2->hide();
  labelSpinbox_0->hide();
  labelSpinbox_1->hide();
  labelSpinbox_2->hide();
  agcControl->hide();
  ppm_correction->hide();
  labelCorr->hide();
  deviceLabel->setText(deviceString[deviceIndex]);
  serialNumber->setText(serialString[deviceIndex]);

  createDevice(deviceString[deviceIndex], serialString[deviceIndex]);
}

SoapyHandler::~SoapyHandler(void)
{
  soapySettings->beginGroup("soapySettings");
  soapySettings->setValue("position-x", myFrame.pos().x());
  soapySettings->setValue("position-y", myFrame.pos().y());
  soapySettings->endGroup();
  myFrame.hide();
  if (worker != nullptr)
    delete worker;
  if (device != nullptr)
  {
    SoapySDR::Device::unmake(device);
    device = nullptr;
  }
}

template <typename Type>
std::string toString(const std::vector<Type> &options)
{
  std::stringstream ss;
  if (options.empty()) return "";
  for (size_t i = 0; i < options.size(); i++)
  {
    if (!ss.str().empty()) ss << ", ";
    ss << options[i];
  }
  return ss.str();
}

std::string toString(const SoapySDR::Range &range)
{
  std::stringstream ss;
  ss << "[" << range.minimum() << ", " << range.maximum();
  if (range.step() != 0.0) ss << ", " << range.step();
  ss << "]";
  return ss.str();
}

std::string toString(const SoapySDR::RangeList &range, const double scale)
{
  const size_t MAXRLEN = 10; //for abbreviating long lists
  std::stringstream ss;
  for (size_t i = 0; i < range.size(); i++)
  {
    if (range.size() >= MAXRLEN && i >= MAXRLEN/2 && i < (range.size()-MAXRLEN/2))
    {
      if (i == MAXRLEN) ss << ", ...";
      continue;
    }
    if (!ss.str().empty()) ss << ", ";
    if (range[i].minimum() == range[i].maximum()) ss << (range[i].minimum()/scale);
    else ss << "[" << (range[i].minimum()/scale) << ", " << (range[i].maximum()/scale) << "]";
  }
  return ss.str();
}

void SoapyHandler::createDevice(QString driver, QString serial)
{
  std::stringstream ss;
  const int dir = SOAPY_SDR_RX;
  const int chan = 0;

  QString handlerName = "driver=" + driver + ",serial=" + serial;
  device = SoapySDR::Device::make(handlerName.toLatin1().data());
  if (device == nullptr)
  {
    throw std_exception_string("Could not find soapy support\n");
    return;
  }
  deviceNameLabel->setText(device->getHardwareKey().c_str());

  size_t numRxChans = device->getNumChannels(dir);
  ss << "  Channels: " << numRxChans << " Rx" << std::endl;

  // info
  const auto info = device->getChannelInfo(dir, chan);
  if (info.size() > 0)
  {
    ss << "  Channel Information:" << std::endl;
    for (const auto &it : info)
    {
      ss << "    " << it.first << "=" << it.second << std::endl;
    }
  }
  ss << "  Full-duplex: " << (device->getFullDuplex(dir, chan)?"YES":"NO") << std::endl;
  ss << "  Supports AGC: " << (device->hasGainMode(dir, chan)?"YES":"NO") << std::endl;
  if (device->hasGainMode(dir, chan))
  {
    agcControl->show();
    if (device->getGainMode(dir, chan))
      agcControl->setChecked(true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(agcControl, &QCheckBox::checkStateChanged,
#else
    connect(agcControl, &QCheckBox::stateChanged,
#endif
            this, &SoapyHandler::set_agcControl);
  }
  else
    agcControl->hide();

  //formats
  std::string formats = toString(device->getStreamFormats(dir, chan));
  if (!formats.empty()) ss << "  Stream formats: " << formats << std::endl;
  double fullScale = 0.0;
  std::string native = device->getNativeStreamFormat(dir, chan, fullScale);
  ss << "  Native format: " << native << " [full-scale=" << fullScale << "]" << std::endl;

  //antennas
  std::vector<std::string> antennas = device->listAntennas(dir, chan);
  std::string antenna = toString(antennas);
  if (!antennas.empty()) ss << "  Antennas: " << antenna << std::endl;
  if (antennas.size() > 1)
  {
    for (u32 i = 0; i < antennas.size(); i++)
      comboBox_1->addItem(antennas[i].c_str());
    connect(comboBox_1, SIGNAL(textActivated (const QString &)),
            this, SLOT(handleAntenna (const QString &)));
    comboBox_1->show();
  }

  //gains
  gainsList = device->listGains(dir, chan);
  for (size_t i = 0; i < gainsList.size(); i++)
  {
    const std::string name = gainsList[i];
    ss << "  " << name << " gain range: " << toString(device->getGainRange(dir, chan, name)) << " dB" << std::endl;
  }

  if (gainsList.size() > 0)
  {
    SoapySDR::Range r = device->getGainRange(dir, chan, gainsList[0]);
    spinBox_0->setMinimum(r.minimum());
    spinBox_0->setMaximum(r.maximum());
    labelSpinbox_0->setText(QString(gainsList[0].c_str()));
    spinBox_0->show();
    labelSpinbox_0->show();
    connect(spinBox_0, qOverload<int>(&QSpinBox::valueChanged), this, &SoapyHandler::handle_spinBox_0);
  }

  if (gainsList.size() > 1)
  {
    SoapySDR::Range r = device->getGainRange(dir, chan, gainsList[1]);
    spinBox_1->setMinimum(r.minimum());
    spinBox_1->setMaximum(r.maximum());
    labelSpinbox_1->setText(QString(gainsList[1].c_str()));
    spinBox_1->show();
    labelSpinbox_1->show();
    connect(spinBox_1, qOverload<int>(&QSpinBox::valueChanged), this, &SoapyHandler::handle_spinBox_1);
  }

  if (gainsList.size() > 2)
  {
    SoapySDR::Range r = device->getGainRange(dir, chan, gainsList[2]);
    spinBox_2->setMinimum(r.minimum());
    spinBox_2->setMaximum(r.maximum());
    labelSpinbox_2->setText(QString(gainsList[2].c_str()));
    spinBox_2->show();
    labelSpinbox_2->show();
    connect(spinBox_2, qOverload<int>(&QSpinBox::valueChanged), this, &SoapyHandler::handle_spinBox_2);
  }

  //frequencies
  std::vector<std::string> freqsList = device->listFrequencies(dir, chan);
  for (u32 i = 0; i < freqsList.size(); i++)
  {
    const std::string name = freqsList[i];
    ss << "  " << name << " freq range: " << toString(device->getFrequencyRange(dir, chan, name), 1e6) << " MHz" << std::endl;
  }
  if (device->hasFrequencyCorrection(dir, chan))
  {
    ppm_correction->show();
    labelCorr->show();
    connect(ppm_correction, &QDoubleSpinBox::valueChanged, this, &SoapyHandler::set_ppmCorrection);
  }

  //rates
  SoapySDR::RangeList rangelist = device->getSampleRateRange(dir, chan);
  ss << "  Sample rates: " << toString(rangelist, 1e6) << " MSps" << std::endl;
  i32 selectedRate = findDesiredSamplerate(rangelist);

   //bandwidths
  i32 selectedWidth = 0;
  const auto bws = device->getBandwidthRange(dir, chan);
  if (!bws.empty())
  {
    ss << "  Filter bandwidths: " << toString(bws, 1e6) << " MHz" << std::endl;
    selectedWidth = findDesiredBandwidth(bws);
  }

  if (driver == "uhd")
  {
    selectedRate = 2048000;
  }
  if(selectedWidth > 0)
  {
    ss << "  Selected bandwidth: " <<  selectedWidth << " Hz" << std::endl;
  }
  samplerateLabel->setText(QString::number(selectedRate));
  device->setSampleRate(dir, chan, selectedRate);
  ss << "  Selected samplerate: " <<  selectedRate << " Sps" << std::endl;
  std::cout << ss.str();

  device->setFrequency(dir, chan, 220000000.0);
  std::vector<size_t> xxx;
  SoapySDR::Stream *stream = device->setupStream(SOAPY_SDR_RX, "CF32", xxx, SoapySDR::Kwargs());
  if (stream == nullptr)
    throw (std_exception_string("cannot open stream"));
  if(selectedWidth > 0)
  {
    device->setBandwidth(dir, chan, selectedWidth);
  }
  worker = new SoapyConverter(device, stream, selectedRate);
  statusLabel->setText("running");
}

void SoapyHandler::setVFOFrequency(i32 f)
{
  if (device == nullptr)
    return;
  device->setFrequency(SOAPY_SDR_RX, 0, f);
}

i32 SoapyHandler::getVFOFrequency(void)
{
  if (device == nullptr)
    return 0;
  return device->getFrequency(SOAPY_SDR_RX, 0);
}

bool SoapyHandler::restartReader(i32 freq)
{
  setVFOFrequency(freq);
  return true;
}

void SoapyHandler::stopReader(void) {}

i32 SoapyHandler::getSamples(cf32 * v, i32 a)
{
  if (worker == nullptr)
    return 0;
  return worker->getSamples(v, a);
}

i32 SoapyHandler::Samples(void)
{
  if (worker == nullptr)
    return 0;
  return worker->Samples();
}

void SoapyHandler::resetBuffer(void) {}

i16 SoapyHandler::bitDepth(void) { return 12; }

void SoapyHandler::handle_spinBox_0(i32 v)
{
  if (device == nullptr)
    return;
  device->setGain(SOAPY_SDR_RX, 0, gainsList[0], (f32)v);
}

void SoapyHandler::handle_spinBox_1(i32 v)
{
  if (device == nullptr)
    return;
  device->setGain(SOAPY_SDR_RX, 0, gainsList[1], (f32)v);
}

void SoapyHandler::handle_spinBox_2(i32 v)
{
  if (device == nullptr)
    return;
  device->setGain(SOAPY_SDR_RX, 0, gainsList[2], (f32)v);
}

void SoapyHandler::set_agcControl(i32 status)
{
  (void)status;
  if (device == nullptr)
    return;
  bool b = agcControl->isChecked();
  device->setGainMode(SOAPY_SDR_RX, 0, b);
}

void SoapyHandler::handleAntenna(const QString & s)
{
  if (device == nullptr)
    return;
  device->setAntenna(SOAPY_SDR_RX, 0, s.toLatin1().data());
}

void SoapyHandler::show()
{
  myFrame.show();
}

void SoapyHandler::hide()
{
  myFrame.hide();
}

bool SoapyHandler::isHidden()
{
  return myFrame.isHidden();
}

bool SoapyHandler::isFileInput()
{
  return false;
}

i32 SoapyHandler::findDesiredSamplerate(const SoapySDR::RangeList &range)
{
  for (size_t i = 0; i < range.size(); i++)
  {
    if ((range[i].minimum() <= 2048000) && (2048000 <= range[i].maximum()))
      return 2048000;
  }
  // No exact match, do try something
  for (size_t i = 0; i < range.size(); i++)
    if ((2048000 < range[i].minimum()) && (range[i].minimum() - 2048000 < 5000000))
      return range[i].minimum();

  for (size_t i = 0; i < range.size(); i++)
    if ((2048000 > range[i].maximum()) && (2048000 - range[i].maximum() < 100000))
      return range[i].minimum();
  return -1;
}

i32 SoapyHandler::findDesiredBandwidth(const SoapySDR::RangeList &range)
{
  for (size_t i = 0; i < range.size(); i++)
  {
    if ((range[i].minimum() <= 1536000) && (1536000 <= range[i].maximum()))
      return 1536000;
  }
  // No exact match, do try something
  for (size_t i = 0; i < range.size(); i++)
    if (range[i].minimum() >= 1500000)
      return range[i].minimum();
  return -1;
}

void SoapyHandler::set_ppmCorrection(double ppm)
{
  if (device == nullptr)
    return;
  device->setFrequencyCorrection(SOAPY_SDR_RX, 0, ppm);
}
