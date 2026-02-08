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
#include <iostream>
#include  "soapy-handler.h"
#include  "soapy-converter.h"
#include  "soapy-deviceselect.h"
#include  "device-exceptions.h"

SoapyHandler::SoapyHandler(QSettings * soapySettings)
  : myFrame(nullptr)
{
  int deviceIndex = 0;
  QString deviceString[10];
  QString serialString[10];
  QString labelString[10];

  this->soapySettings = soapySettings;
  setupUi(&myFrame);
  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  myFrame.show();
  const auto results = SoapySDR::Device::enumerate();
  for (size_t i = 0; i < results.size(); i++)
  {
    fprintf(stderr, "Found device #%d:\n", (int)i);
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

  if (results.size() > 1)
  {
    soapy_deviceSelect deviceSelector;
    for (size_t i = 0; i < results.size(); i++)
    {
      deviceSelector.addtoDeviceList(labelString[i].toStdString().c_str());
    }
    deviceIndex = deviceSelector.QDialog::exec();
  }
  if (results.empty())
  {
    throw (std_exception_string("Could not find soapy support"));
    return;
  }

  worker = nullptr;
  comboBox_1->hide();
  spinBox_1->hide();
  spinBox_2->hide();
  labelSpinbox_1->hide();
  labelSpinbox_2->hide();
  agcControl->hide();
  deviceLabel->setText(deviceString[deviceIndex]);
  serialNumber->setText(serialString[deviceIndex]);

  createDevice(deviceString[deviceIndex], serialString[deviceIndex]);
}

SoapyHandler::~SoapyHandler(void)
{
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

  QString handlerName = "driver=";
  handlerName.append(driver);
  handlerName.append(",serial=");
  handlerName.append(serial);
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
    connect(agcControl, SIGNAL(stateChanged(int)), this, SLOT(set_agcControl(int)));
    agcControl->show();
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
  spinBox_1->hide();
  labelSpinbox_1->hide();
  spinBox_2->hide();
  labelSpinbox_2->hide();

  gainsList = device->listGains(dir, chan);
  for (size_t i = 0; i < gainsList.size(); i++)
  {
      const std::string name = gainsList[i];
      ss << "  " << name << " gain range: " << toString(device->getGainRange(dir, chan, name)) << " dB" << std::endl;
  }

  if (gainsList.size() > 0)
  {
    SoapySDR::Range r = device->getGainRange(dir, chan, gainsList[0]);
    spinBox_1->setMinimum(r.minimum());
    spinBox_1->setMaximum(r.maximum());
    labelSpinbox_1->setText(QString(gainsList[0].c_str()));
    spinBox_1->show();
    labelSpinbox_1->show();
    connect(spinBox_1, SIGNAL(valueChanged (int)),
            this, SLOT(handle_spinBox_1 (int)));
  }

  if (gainsList.size() > 1)
  {
    SoapySDR::Range r = device->getGainRange(dir, chan, gainsList[1]);
    spinBox_2->setMinimum(r.minimum());
    spinBox_2->setMaximum(r.maximum());
    labelSpinbox_2->setText(QString(gainsList[1].c_str()));
    spinBox_2->show();
    labelSpinbox_2->show();
    connect(spinBox_2, SIGNAL(valueChanged (int)),
            this, SLOT(handle_spinBox_2 (int)));
  }

  //frequencies
  std::vector<std::string> freqsList = device->listFrequencies(dir, chan);
  const std::string name = freqsList[0];
  ss << "  " << name << " freq range: " << toString(device->getFrequencyRange(dir, chan, name), 1e6) << " MHz" << std::endl;

  //rates
  SoapySDR::RangeList rangelist = device->getSampleRateRange(dir, chan);
  ss << "  Sample rates: " << toString(rangelist, 1e6) << " MSps" << std::endl;
  int selectedRate = findDesiredRange(rangelist);
  //selectedRate = 2400000;
  samplerateLabel->setText(QString::number(selectedRate));
  device->setSampleRate(dir, chan, selectedRate);

   //bandwidths
  const auto bws = device->getBandwidthRange(dir, chan);
  if (!bws.empty()) ss << "  Filter bandwidths: " << toString(bws, 1e6) << " MHz" << std::endl;
  i32 selectedWidth = 0;
  std::vector<f64> bandwidths = device->listBandwidths(dir, 0);
  if (bandwidths.size() > 0)
  {
    i32 minDist = 10000000;
    for (u32 i = 0; i < bandwidths.size(); i++)
    {
      if ((bandwidths[i] >= 1536000) && (bandwidths[i] - 1536000 < minDist))
      {
        minDist = bandwidths[i] - 1536000;
        selectedWidth = bandwidths[i];
      }
    }
    ss << "  Selected bandwidth: " <<  selectedWidth << " Hz" << std::endl;
    device->setBandwidth(dir, chan, selectedWidth);
  }
  ss << "  Selected samplerate: " <<  selectedRate << " Sps" << std::endl;

  std::cout << ss.str();
  device->setFrequency(dir, chan, 220000000.0);
  worker = new SoapyConverter(device, selectedRate);
  statusLabel->setText("running");
}

void SoapyHandler::setVFOFrequency(i32 f)
{
  if (worker == nullptr)
    return;
  device->setFrequency(SOAPY_SDR_RX, 0, f);
}

i32 SoapyHandler::getVFOFrequency(void)
{
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

void SoapyHandler::handle_spinBox_1(i32 v)
{
  device->setGain(SOAPY_SDR_RX, 0, gainsList[0], (f32)v);
}

void SoapyHandler::handle_spinBox_2(i32 v)
{
  device->setGain(SOAPY_SDR_RX, 0, gainsList[1], (f32)v);
}

void SoapyHandler::set_agcControl(i32 v)
{
  (void)v;
  if (agcControl->isChecked())
    device->setGainMode(SOAPY_SDR_RX, 0, 1);
  else
    device->setGainMode(SOAPY_SDR_RX, 0, 0);
}

void SoapyHandler::handleAntenna(const QString & s)
{
  if (worker == nullptr)
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

int SoapyHandler::findDesiredRange(const SoapySDR::RangeList &range)
{
  for (size_t i = 0; i < range.size(); i++)
  {
    if ((range[i].minimum() <= 2048000) &&
        (2048000 <= range[i].maximum()))
      return 2048000;
  }
  // No exact match, do try something
  for (size_t i = 0; i < range.size(); i++)
    if ((2048000 < range[i].minimum()) &&
        (range[i].minimum() - 2048000 < 5000000))
      return range[i].minimum();

  for (size_t i = 0; i < range.size(); i++)
    if ((2048000 > range[i].maximum()) &&
        (2048000 - range[i].maximum() < 100000))
      return range[i].minimum();
  return -1;
}
