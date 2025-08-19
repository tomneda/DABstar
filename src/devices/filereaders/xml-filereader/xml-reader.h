/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2019
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
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
#ifndef  XML_READER_H
#define  XML_READER_H

#include "dab-constants.h"
#include  <QThread>
#include  <QMessageBox>
#include  <cstdio>
#include  "ringbuffer.h"
#include  <stdint.h>
#include  <vector>
#include  <atomic>

class XmlFileReader;
class XmlDescriptor;

class XmlReader : public QThread
{
Q_OBJECT
public:
  XmlReader(XmlFileReader * mr, FILE * f, XmlDescriptor * fd, u32 filePointer, RingBuffer<cf32> * b);
  ~XmlReader() override;
  void stopReader();
  bool handle_continuousButton();

private:
  union UCnv // to avoid warnings "dereferencing type-punned pointer will break strict-aliasing rules"
  {
    i32 int32Data;
    f32   floatData;
  };

  std::atomic<bool> continuous;
  FILE * file;
  XmlDescriptor * fd;
  u32 filePointer;
  RingBuffer<cf32> * sampleBuffer;
  XmlFileReader * parent;
  i32 nrElements;
  i64 samplesToRead;
  std::atomic<bool> running;
  void run();
  i64 compute_nrSamples(FILE * f, i32 blockNumber);
  i32 readSamples(FILE * f, void(XmlReader::*)(FILE *, cf32 *, i32));
  void readElements_IQ(FILE * f, cf32 *, i32 amount);
  void readElements_QI(FILE * f, cf32 *, i32 amount);
  void readElements_I(FILE * f, cf32 *, i32 amount);
  void readElements_Q(FILE * f, cf32 *, i32 amount);
  //
  //	for the conversion - if any
  i16 convBufferSize;
  i16 convIndex;
  std::vector<cf32> convBuffer;
  i16 mapTable_int[2048];
  f32 mapTable_float[2048];
  f32 mapTable[256];

signals:
  void signal_set_progress(i64, i64);
};

#endif
