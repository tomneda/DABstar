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
#include	<QMessageBox>
#include	"soapy-handler.h"
#include	<cstdio> //printf
#include	<stdlib.h> //free
#include	<complex.h>
#include	"soapy_CS8.h"
#include	"soapy_CS16.h"
#include	"soapy_CF32.h"

soapyHandler::soapyHandler(QSettings * soapySettings)
  : myFrame(nullptr)
{
  this->soapySettings = soapySettings;
  setupUi(&myFrame);
  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  myFrame.show();
  deviceLineEdit = new QLineEdit(nullptr);
  deviceLineEdit->show();
  connect(deviceLineEdit, SIGNAL(returnPressed (void)),
          this, SLOT(createDevice (void)));

  worker = nullptr;
  comboBox_1->hide();
  spinBox_1->hide();
  spinBox_2->hide();
  labelSpinbox_1->hide();
  labelSpinbox_2->hide();
  agcControl->hide();
}

soapyHandler::~soapyHandler(void)
{
  if (worker != nullptr)
    delete worker;
  delete deviceLineEdit;
}

bool contains(std::vector<std::string> s, std::string key)
{
  for (i32 i = 0; i < s.size(); i++)
    if (s.at(i) == key)
      return true;
  return false;
}

void soapyHandler::createDevice(void)
{
  QString s = deviceLineEdit->text();
  QString handlerName = "driver=";

  handlerName.append(s);

  device = SoapySDR::Device::make(handlerName.toLatin1().data());

  if (device == nullptr)
  {
    QMessageBox::warning(&myFrame, tr("Warning"),
                         tr("could not find soapy support\n"));
    return;
  }

  deviceNameLabel->setText(device->getHardwareKey().c_str());
  fprintf(stderr, "channels = %d\n",
          device->getNumChannels(SOAPY_SDR_RX));
  std::vector<std::string> streamFormats =
    device->getStreamFormats(SOAPY_SDR_RX, 0);

  std::vector<std::string> antennas =
    device->listAntennas(SOAPY_SDR_RX, 0);
  if (antennas.size() > 1)
  {
    for (i32 i = 0; i < antennas.size(); i++)
      comboBox_1->addItem(antennas[i].c_str());
    connect(comboBox_1, SIGNAL(textActivated (const QString &)),
            this, SLOT(handleAntenna (const QString &)));
    comboBox_1->show();
  }

  for (i32 i = 0; i < antennas.size(); i++)
    fprintf(stderr, "antenna %d = %s\n", i, antennas[i].c_str());

  if (device->hasGainMode(SOAPY_SDR_RX, 0))
  {
    connect(agcControl, SIGNAL(stateChanged (int)),
            this, SLOT(set_agcControl (int)));
    agcControl->show();
  }
  else
    agcControl->hide();

  spinBox_1->hide();
  labelSpinbox_1->hide();
  spinBox_2->hide();
  labelSpinbox_2->hide();

  gains = device->listGains(SOAPY_SDR_RX, 0);

  if (gains.size() > 0)
  {
    SoapySDR::Range r = device->getGainRange(SOAPY_SDR_RX,
                                             0,
                                             gains[0]);
    spinBox_1->setMinimum(r.minimum());
    spinBox_1->setMaximum(r.maximum());
    labelSpinbox_1->setText(QString(gains[0].c_str()));
    spinBox_1->show();
    labelSpinbox_1->show();
    connect(spinBox_1, SIGNAL(valueChanged (int)),
            this, SLOT(handle_spinBox_1 (int)));
  }

  if (gains.size() > 1)
  {
    SoapySDR::Range r = device->getGainRange(SOAPY_SDR_RX,
                                             0,
                                             gains[1]);
    spinBox_2->setMinimum(r.minimum());
    spinBox_2->setMaximum(r.maximum());
    labelSpinbox_2->setText(QString(gains[1].c_str()));
    spinBox_2->show();
    labelSpinbox_2->show();
    connect(spinBox_2, SIGNAL(valueChanged (int)),
            this, SLOT(handle_spinBox_2 (int)));
  }

  SoapySDR::Range r = device->getGainRange(SOAPY_SDR_RX, 0, gains[0]);
  fprintf(stderr, "range totaal = %f, %f\n",
          r.minimum(), r.maximum());

  std::vector<SoapySDR::Range> freqs =
    device->getFrequencyRange(SOAPY_SDR_RX, 0);
  for (i32 i = 0; i < freqs.size(); i++)
    fprintf(stderr, "freqs %f %f\n", freqs[i].minimum(),
            freqs[i].maximum());


  std::vector<f64> bandwidths =
    device->listBandwidths(SOAPY_SDR_RX, 0);

  i32 minDist = 10000000;
  i32 selectedWidth = 0;
  for (i32 i = 0; i < bandwidths.size(); i++)
    if ((bandwidths[i] >= 1536000) &&
        (bandwidths[i] - 1536000 < minDist))
    {
      minDist = bandwidths[i] - 1536000;
      selectedWidth = bandwidths[i];
    }

  bool validRate = false;
  std::vector<f64> samplerates =
    device->listSampleRates(SOAPY_SDR_RX, 0);
  for (i32 i = 0; i < samplerates.size(); i++)
    if ((i32)(samplerates[i]) == 2048000)
    {
      validRate = true;
      break;
    }

  device->setBandwidth(SOAPY_SDR_RX, 0, selectedWidth);
  device->setSampleRate(SOAPY_SDR_RX, 0, 2048000.0);
  device->setFrequency(SOAPY_SDR_RX, 0, 220000000.0);
  if (contains(streamFormats, std::string("CF32")))
    worker = new soapy_CF32(device);
  else if (contains(streamFormats, "CS16"))
    worker = new soapy_CS16(device);
  else if (contains(streamFormats, "CS8"))
    worker = new soapy_CS8(device);
  statusLabel->setText("OK");
}

void soapyHandler::setVFOFrequency(i32 f)
{
  if (worker == nullptr)
    return;
  device->setFrequency(SOAPY_SDR_RX, 0, f);
}

i32 soapyHandler::getVFOFrequency(void)
{
  return device->getFrequency(SOAPY_SDR_RX, 0);
}

i32 soapyHandler::defaultFrequency(void) { return 220000000; }

bool soapyHandler::restartReader(i32 freq)
{
  setVFOFrequency(freq);
  return true;
}

void soapyHandler::stopReader(void) {}

i32 soapyHandler::getSamples(cf32 * v,
                             i32 a)
{
  if (worker == nullptr)
    return 0;
  return worker->getSamples(v, a);
}

i32 soapyHandler::Samples(void)
{
  if (worker == nullptr)
    return 0;
  return worker->Samples();
}

void soapyHandler::resetBuffer(void) {}
i16 soapyHandler::bitDepth(void) { return 12; }

void soapyHandler::handle_spinBox_1(i32 v)
{
  device->setGain(SOAPY_SDR_RX, 0, gains[0], (f32)v);
}

void soapyHandler::handle_spinBox_2(i32 v)
{
  device->setGain(SOAPY_SDR_RX, 0, gains[1], (f32)v);
}

void soapyHandler::set_agcControl(i32 v)
{
  if (agcControl->isChecked())
    device->setGainMode(SOAPY_SDR_RX, 0, 1);
  else
    device->setGainMode(SOAPY_SDR_RX, 0, 0);
}

void soapyHandler::handleAntenna(const QString & s)
{
  if (worker == nullptr)
    return;
  device->setAntenna(SOAPY_SDR_RX, 0, s.toLatin1().data());
}

void soapyHandler::show()
{
  myFrame.show();
}

void soapyHandler::hide()
{
  myFrame.hide();
}

bool soapyHandler::isHidden()
{
  return myFrame.isHidden();
}
