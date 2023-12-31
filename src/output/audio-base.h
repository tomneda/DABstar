/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C)  2014 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB.
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

#ifndef AUDIO_BASE_H
#define AUDIO_BASE_H

#include  "dab-constants.h"
#include  <cstdio>
#include  <samplerate.h>
#include  <sndfile.h>
#include  <QMutex>
#include  <QObject>
#include  "newconverter.h"
#include  "ringbuffer.h"


class AudioBase : public QObject
{
Q_OBJECT
public:
  AudioBase();
  ~AudioBase() override = default;
  virtual void stop();
  virtual void restart();

  void audioOut(int16_t *, int32_t, int);
  void startDumping(SNDFILE *);
  void stopDumping();
  virtual bool hasMissed();
private:
  void audioOut_16000(int16_t *, int32_t);
  void audioOut_24000(int16_t *, int32_t);
  void audioOut_32000(int16_t *, int32_t);
  void audioOut_48000(int16_t *, int32_t);
  void audioReady(float *, int32_t);

  newConverter converter_16;
  newConverter converter_24;
  newConverter converter_32;
  SNDFILE * dumpFile;
  QMutex myLocker;

protected:
  virtual void audioOutput(float *, int32_t);
};

#endif
