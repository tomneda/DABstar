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

#include  "Qt-audiodevice.h"

//	Create a "device"
QtAudioDevice::QtAudioDevice(RingBuffer<float> * ipBuffer, QObject * parent) :
  QIODevice(parent),
  Buffer(ipBuffer)
{
}

void QtAudioDevice::start()
{
  open(QIODevice::ReadOnly);
}

void QtAudioDevice::stop()
{
  Buffer->FlushRingBuffer();
  close();
}

//	we always return "len" bytes
qint64 QtAudioDevice::readData(char * buffer, qint64 maxSize)
{
  //	"maxSize" is the requested size in bytes
  //	"amount" is in floats

  if (const qint64 amount = Buffer->getDataFromBuffer(buffer, (int32_t)(maxSize / sizeof(float)));
     (signed)sizeof(float) * amount < maxSize)
  {
    for (int32_t i = (int32_t)amount * sizeof(float); i < maxSize; i++)
    {
      buffer[i] = 0;
    }
  }

  return maxSize;
}

//	unused here
qint64 QtAudioDevice::writeData(const char * data, qint64 len)
{
  Q_UNUSED(data)
  Q_UNUSED(len)
  return 0;
}

