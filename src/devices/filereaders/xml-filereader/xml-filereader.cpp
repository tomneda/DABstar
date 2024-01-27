/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013 .. 2017
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

#include  "xml-filereader.h"
#include  <cstdio>
#include  <unistd.h>
#include  <cstdlib>
#include  <fcntl.h>
#include  <sys/time.h>
#include  <ctime>

#include  "xml-descriptor.h"
#include  "xml-reader.h"

constexpr uint32_t INPUT_FRAMEBUFFERSIZE = 8 * 32768;

XmlFileReader::XmlFileReader(const QString & f) :   
  myFrame(nullptr),
  _I_Buffer(INPUT_FRAMEBUFFERSIZE)
{
  fileName = f;

  setupUi(&myFrame);

  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  myFrame.show();
  theFile = fopen(f.toUtf8().data(), "rb");

  if (theFile == nullptr)
  {
    fprintf(stderr, "file %s cannot open\n", f.toUtf8().data());
    perror("file ?");
    throw (31);
  }

  bool ok = false;
  filenameLabel->setText(f);
  theDescriptor = new XmlDescriptor(theFile, &ok);
  if (!ok)
  {
    fprintf(stderr, "%s probably not an xml file\n", f.toUtf8().data());
    throw (32);
  }

  fileProgress->setValue(0);
  currentTime->display(0);
  samplerateDisplay->display(theDescriptor->sampleRate);
  nrBitsDisplay->display(theDescriptor->bitsperChannel);
  containerLabel->setText(theDescriptor->container);
  iqOrderLabel->setText(theDescriptor->iqOrder);
  byteOrderLabel->setText(theDescriptor->byteOrder);
  frequencyDisplay->display(theDescriptor->blockList[0].frequency / 1000.0);
  typeofUnitLabel->setText(theDescriptor->blockList[0].typeofUnit);
  modulationtypeLabel->setText(theDescriptor->blockList[0].modType);

  deviceModelName->setText(theDescriptor->deviceName);
  deviceModel->setText(theDescriptor->deviceModel);
  recorderName->setText(theDescriptor->recorderName);
  recorderVersion->setText(theDescriptor->recorderVersion);
  recordingTime->setText(theDescriptor->recordingTime);

  nrElementsDisplay->display(theDescriptor->blockList[0].nrElements);
  fprintf(stderr, "nrElements = %d\n", theDescriptor->blockList[0].nrElements);
  //connect(continuousButton, SIGNAL (clicked()), this, SLOT (slot_handle_cb_loop_file(0)));
  connect(cbLoopFile, &QCheckBox::clicked, this, &XmlFileReader::slot_handle_cb_loop_file);
  running.store(false);
}

XmlFileReader::~XmlFileReader()
{
  if (running.load())
  {
    theReader->stopReader();
    while (theReader->isRunning())
    {
      usleep(100);
    }
    delete theReader;
  }
  if (theFile != nullptr)
  {
    fclose(theFile);
  }

  delete theDescriptor;
}

bool XmlFileReader::restartReader(int32_t freq)
{
  (void)freq;
  if (running.load())
  {
    return true;
  }
  theReader = new XmlReader(this, theFile, theDescriptor, 5000, &_I_Buffer);
  running.store(true);
  return true;
}

void XmlFileReader::stopReader()
{
  if (running.load())
  {
    theReader->stopReader();
    while (theReader->isRunning())
    {
      usleep(100);
    }
    delete theReader;
    theReader = nullptr;
  }
  running.store(false);
}

//	size is in "samples"
int32_t XmlFileReader::getSamples(cmplx * V, int32_t size)
{

  if (theFile == nullptr)
  {    // should not happen
    return 0;
  }

  while ((int32_t)(_I_Buffer.get_ring_buffer_read_available()) < size)
  {
    usleep(1000);
  }

  return _I_Buffer.get_data_from_ring_buffer(V, size);
}

int32_t XmlFileReader::Samples()
{
  if (theFile == nullptr)
  {
    return 0;
  }
  return _I_Buffer.get_ring_buffer_read_available();
}

void XmlFileReader::slot_set_progress(int samplesRead, int samplesToRead)
{
  fileProgress->setValue((float)samplesRead / samplesToRead * 100);
  currentTime->display(QString("%1").arg(samplesRead / 2048000.0, 0, 'f', 1));
  totalTime->display(QString("%1").arg(samplesToRead / 2048000.0, 0, 'f', 1));
}

int XmlFileReader::getVFOFrequency()
{
  return theDescriptor->blockList[0].frequency;
}

void XmlFileReader::slot_handle_cb_loop_file(const bool iChecked)
{
  if (theReader == nullptr)
  {
    return;
  }

  cbLoopFile->setChecked(theReader->handle_continuousButton());
}

void XmlFileReader::show()
{
  myFrame.show();
}

void XmlFileReader::hide()
{
  myFrame.hide();
}

bool XmlFileReader::isHidden()
{
  return myFrame.isHidden();
}

bool XmlFileReader::isFileInput()
{
  return true;
}

void XmlFileReader::setVFOFrequency(int32_t)
{
}

void XmlFileReader::resetBuffer()
{
}

int16_t XmlFileReader::bitDepth()
{
  return static_cast<int16_t>(theDescriptor->bitsperChannel);
}

QString XmlFileReader::deviceName()
{
  return "XmlFile";
}

