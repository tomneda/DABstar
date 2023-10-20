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

#include  <cstdio>
#include  "Qt-audiodevice.h"
#include  "Qt-audio.h"

QtAudio::QtAudio()
{
  mpBuffer = new RingBuffer<float>(8 * 32768);
  mOutputRate = 48000;  // default
  mpAudioDevice = new QtAudioDevice(mpBuffer, this);
  mpAudioOutput = nullptr;
  setParams(mOutputRate);
}

QtAudio::~QtAudio()
{
  delete mpAudioOutput; // theAudioOutput==nullptr is allowed
  delete mpAudioDevice;
  delete mpBuffer;
}

//
//	Note that audioBase functions have - if needed - the rate
//	converted.  This functions overrides the one in audioBase
void QtAudio::audioOutput(float * fragment, int32_t size)
{
  if (mpAudioDevice != nullptr)
  {
    mpBuffer->putDataIntoBuffer(fragment, 2 * size);
  }
}

void QtAudio::setParams(int outputRate)
{
  if (mpAudioOutput != nullptr)
  {
    disconnect(mpAudioOutput, &QAudioOutput::stateChanged, this, &QtAudio::handleStateChanged);
    delete mpAudioOutput;
    mpAudioOutput = nullptr;
  }

  mAudioFormat.setSampleRate(outputRate);
  mAudioFormat.setChannelCount(2);
  mAudioFormat.setSampleSize(sizeof(float) * 8);
  mAudioFormat.setCodec("audio/pcm");
  mAudioFormat.setByteOrder(QAudioFormat::LittleEndian);
  mAudioFormat.setSampleType(QAudioFormat::Float);

  if (QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
      !info.isFormatSupported(mAudioFormat))
  {
    fprintf(stderr, "Audio: Sorry, format cannot be handled");
    return;
  }

  mpAudioOutput = new QAudioOutput(mAudioFormat, this);
  connect(mpAudioOutput, &QAudioOutput::stateChanged, this, &QtAudio::handleStateChanged);

  restart();
  mCurrentState = mpAudioOutput->state();
}

void QtAudio::stop(void)
{
  if (mpAudioDevice == nullptr)
  {
    return;
  }
  mpAudioDevice->stop();
  mpAudioOutput->stop();
}

void QtAudio::restart(void)
{
  if (mpAudioDevice == nullptr)
  {
    return;
  }
  mpAudioDevice->start();
  mpAudioOutput->start(mpAudioDevice);
}

void QtAudio::handleStateChanged(QAudio::State newState)
{
  mCurrentState = newState;

  switch (mCurrentState)
  {
  case QAudio::IdleState: mpAudioOutput->stop();
    break;

  default: ;
  }
}
