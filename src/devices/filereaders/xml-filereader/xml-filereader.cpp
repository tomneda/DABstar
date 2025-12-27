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
#include  "openfiledialog.h"
#include  "setting-helper.h"

#include  <cstdio>
#include  <cstdlib>
#include  <fcntl.h>
#include  <ctime>

#ifdef _WIN32
#else

  #include  <unistd.h>
  #include  <sys/time.h>

#endif

#include  "xml-descriptor.h"
#include  "xml-reader.h"

constexpr u32 cInputFrameBufferSize = 8 * 32768;

XmlFileReader::XmlFileReader(const QString & iFilename)
  : Ui_xmlfile_widget(), myFrame(nullptr)
  , _I_Buffer(cInputFrameBufferSize)
{
  fileName = iFilename;

  setupUi(&myFrame);

  sliderFilePos->setFixedHeight(31);
  sliderFilePos->setStyleSheet("QSlider::handle:horizontal { width:  16px; margin-top: -9px;  margin-bottom: -9px; border-radius: 5px; }");

  Settings::FileReaderXml::posAndSize.read_widget_geometry(&myFrame);

  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  myFrame.show();
  theFile = OpenFileDialog::open_file(iFilename, "rb");

  if (theFile == nullptr)
  {
    const QString val = QString("Cannot open file '%1' (consider avoiding 'umlauts')").arg(iFilename);
    throw std::runtime_error(val.toUtf8().data());
  }

  bool ok = false;
  filenameLabel->setText(iFilename);
  theDescriptor = new XmlDescriptor(theFile, &ok);
  if (!ok)
  {
    const QString val = QString("'%1' is probably not a xml file").arg(iFilename);
    throw std::runtime_error(val.toUtf8().data());
  }

  sliderFilePos->setValue(0);
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

  i64 nrElements = 0;
  samplesToRead = 0;
  for (i32 blocks = 0; blocks < theDescriptor->nrBlocks; blocks++)
  {
    samplesToRead += compute_nrSamples(theDescriptor, blocks);
    nrElements += theDescriptor->blockList[blocks].nrElements;
  }
  nrElementsDisplay->display((double)nrElements);
  totalTime->display(QString("%1").arg((float)samplesToRead / theDescriptor->sampleRate, 0, 'f', 1));

  fseek(theFile, 0, SEEK_END);
  mFileLength = ftell(theFile);
  i32 bytecount = 0;
  if (theDescriptor->container == "int8" || theDescriptor->container == "uint8")
    bytecount = 1;
  else if (theDescriptor->container == "int16")
    bytecount = 2;
  else if (theDescriptor->container == "int24")
    bytecount = 3;
  else if (theDescriptor->container == "float32" || theDescriptor->container == "int32")
    bytecount = 4;
  startPoint = mFileLength - (nrElements * bytecount);
  if(startPoint < 2048 || startPoint > 1000000)
    startPoint = 2048;
  qDebug() << "startPoint =" << startPoint;

  connect(cbLoopFile, &QCheckBox::clicked, this, &XmlFileReader::slot_handle_cb_loop_file);
  connect(sliderFilePos, &QSlider::sliderPressed, this, &XmlFileReader::slot_slider_pressed);
  connect(sliderFilePos, &QSlider::sliderReleased, this, &XmlFileReader::slot_slider_released);
  connect(sliderFilePos, &QSlider::sliderMoved, this, &XmlFileReader::slot_slider_moved);

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

  Settings::FileReaderXml::posAndSize.write_widget_geometry(&myFrame);
}

bool XmlFileReader::restartReader(i32 freq)
{
  (void)freq;
  if (running.load())
  {
    return true;
  }
  theReader = new XmlReader(this, theFile, theDescriptor, startPoint, &_I_Buffer);
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

//  size is in "samples"
i32 XmlFileReader::getSamples(cf32 * V, i32 size)
{

  if (theFile == nullptr)
  {    // should not happen
    return 0;
  }

  while ((i32)(_I_Buffer.get_ring_buffer_read_available()) < size)
  {
    usleep(1000);
  }

  return _I_Buffer.get_data_from_ring_buffer(V, size);
}

i32 XmlFileReader::Samples()
{
  if (theFile == nullptr)
  {
    return 0;
  }
  return _I_Buffer.get_ring_buffer_read_available();
}

void XmlFileReader::slot_set_progress(i64 samplesRead, i64 samplesToRead)
{
  if (mSliderMovementPos < 0) // suppress slider update while mouse move on slider
  {
    sliderFilePos->setValue((f32)samplesRead / (f32)samplesToRead * 1000.0f);
  }
  currentTime->display(QString("%1").arg((float)samplesRead / theDescriptor->sampleRate, 0, 'f', 1));
}

i32 XmlFileReader::getVFOFrequency()
{
  return theDescriptor->blockList[0].frequency;
}

void XmlFileReader::slot_handle_cb_loop_file(const bool /*iChecked*/)
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

void XmlFileReader::setVFOFrequency(i32)
{
}

void XmlFileReader::resetBuffer()
{
}

i16 XmlFileReader::bitDepth()
{
  return static_cast<i16>(theDescriptor->bitsperChannel);
}

QString XmlFileReader::deviceName()
{
  return "XmlFile";
}

void XmlFileReader::slot_slider_pressed()
{
  mSliderMovementPos = sliderFilePos->value();
}

void XmlFileReader::slot_slider_released()
{
  if (mSliderMovementPos >= 0)
  {
    mSliderMovementPos = -1;
  }
}

void XmlFileReader::slot_slider_moved(const i32 iPos)
{
  if (theReader == nullptr)
    return;
  mSliderMovementPos = iPos; // iPos = [0; 1000]
  theReader->jump_to_relative_position(iPos);
}

i64 XmlFileReader::compute_nrSamples(XmlDescriptor *fd, i32 blockNumber)
{
  i64 nrElements = fd->blockList.at(blockNumber).nrElements;
  i64 samplesToRead = 0;

  if (fd->blockList.at(blockNumber).typeofUnit == "Channel")
  {
    if ((fd->iqOrder == "IQ") || (fd->iqOrder == "QI"))
    {
      samplesToRead = nrElements / 2;
    }
    else
    {
      samplesToRead = nrElements;
    }
  }
  else
  {  // typeofUnit = "sample"
    samplesToRead = nrElements;
  }

  qDebug() << samplesToRead << "samples to read, order is" << fd->iqOrder.toLatin1();
  return samplesToRead;
}
