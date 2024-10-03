/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB 
 *    Many of the ideas as implemented in Qt-DAB are derived from
 *    other work, made available through the GNU general Public License.
 *    All copyrights of the original authors are acknowledged.
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
 */

#include <stdio.h>
#include "Qt-audio.h"
#include <QMediaDevices>
#include <QDebug>
//#include	"settingNames.h"

Qt_Audio::Qt_Audio()
  : tempBuffer(8 * 32768)
{
  //audioSettings = settings;
  outputRate = 48000;	// default
  working.store(false);
  isInitialized.store(false);
  newDeviceIndex = -1;
  initialize_deviceList();
  initializeAudio(QMediaDevices::defaultAudioOutput());
}

void Qt_Audio::initialize_deviceList()
{
  const QAudioDevice & defaultDeviceInfo = QMediaDevices::defaultAudioOutput();
  theList.push_back(defaultDeviceInfo);
  for (auto & deviceInfo : QMediaDevices::audioOutputs())
  {
    if (deviceInfo != defaultDeviceInfo)
    {
      theList.push_back(deviceInfo);
    }
  }

  qDebug() << "Found " << theList.size() << " output devices:";

  for (auto & listEl : theList)
  {
    qDebug() << "  " << listEl.description();
  }

  if ((defaultDeviceInfo.description().isEmpty()) && (theList.size() < 2))
  {
    throw 22;
  }
}

QStringList Qt_Audio::streams()
{
  QStringList nameList;
  for (auto & listEl : theList)
  {
    nameList << listEl.description();
  }
  return nameList;
}

//
//	Note that AudioBase functions have - if needed - the rate
//	converted.  This functions overrides the one in AudioBase
void Qt_Audio::audioOutput(float * fragment, int32_t size)
{
  if (!working.load())
    return;
  int aa = tempBuffer.get_ring_buffer_write_available();
  aa = std::min((int)(size * sizeof(float)), aa);
  aa &= ~03;
  tempBuffer.put_data_into_ring_buffer(fragment, aa);
  // int periodSize = m_audioOutput->periodSize();
  constexpr int32_t periodSize = 4096;
  char buffer[periodSize];
  while ((m_audioOutput->bytesFree() >= periodSize) &&
         (tempBuffer.get_ring_buffer_read_available() >= periodSize))
  {
    tempBuffer.get_data_from_ring_buffer(buffer, periodSize);
    theWorker->write(buffer, periodSize);
  }
}

void Qt_Audio::initializeAudio(const QAudioDevice & deviceInfo)
{
  audioFormat.setSampleRate(outputRate);
  audioFormat.setChannelCount(2);
  //audioFormat.setSampleSize(sizeof(float) * 8);
  //audioFormat.setCodec("audio/pcm");
  //audioFormat.setByteOrder(QAudioFormat::LittleEndian);
  audioFormat.setSampleFormat(QAudioFormat::SampleFormat::Float);

  //QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();

  // QAudioFormat supportedFormat = deviceInfo.nearestFormat(desiredFormat);
  //
  // if (!deviceInfo.isFormatSupported(audioFormat))
  // {
  //   audioFormat = deviceInfo.nearestFormat(audioFormat);
  // }
  isInitialized.store(false);
  if (deviceInfo.isFormatSupported(audioFormat))
  {
    m_audioOutput.reset(new QAudioSink(audioFormat));
    if (m_audioOutput->error() == QAudio::NoError)
    {
      isInitialized.store(true);
      qDebug() << "Audio initialization went OK";
    }
    else
    {
      qCritical() << "Audio initialization failed";
    }
  }
}

void Qt_Audio::stop()
{
  m_audioOutput->stop();
  working.store(false);
  isInitialized.store(false);
}

void Qt_Audio::restart()
{
//	fprintf (stderr, "Going to restart with %d\n", newDeviceIndex);
  if (newDeviceIndex < 0)
    return;
  initializeAudio(theList.at(newDeviceIndex));
  if (!isInitialized.load())
  {
    fprintf(stderr, "Init failed for device %d\n", newDeviceIndex);
    return;
  }
  theWorker = m_audioOutput->start();
  if (m_audioOutput->error() == QAudio::NoError)
  {
    working.store(true);
//	   fprintf (stderr, "Device reports: no error\n");
  }
  else
    fprintf(stderr, "restart gaat niet door\n");

  //m_audioOutput->setVolume((float)vol / 100);
}

bool Qt_Audio::selectDevice(int16_t index)
{
  newDeviceIndex = index;
  stop();
  restart();
  return working.load();
}

void Qt_Audio::suspend()
{
  if (!working.load())
    return;
  m_audioOutput->suspend();
}

void Qt_Audio::resume()
{
  if (!working.load())
    return;
  m_audioOutput->resume();
}

void Qt_Audio::setVolume(int v)
{
  //audioSettings->setValue(QT_AUDIO_VOLUME, v);
  m_audioOutput->setVolume((float)v / 100);
}
