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
  XmlReader(XmlFileReader * mr, FILE * f, XmlDescriptor * fd, uint32_t filePointer, RingBuffer<cmplx> * b);
  ~XmlReader() override;
  void stopReader();
  bool handle_continuousButton();

private:
  union UCnv // to avoid warnings "dereferencing type-punned pointer will break strict-aliasing rules"
  {
    int32_t int32Data;
    float   floatData;
  };

  std::atomic<bool> continuous;
  FILE * file;
  XmlDescriptor * fd;
  uint32_t filePointer;
  RingBuffer<cmplx> * sampleBuffer;
  XmlFileReader * parent;
  int nrElements;
  int samplesToRead;
  std::atomic<bool> running;
  void run();
  int compute_nrSamples(FILE * f, int blockNumber);
  int readSamples(FILE * f, void(XmlReader::*)(FILE *, cmplx *, int));
  void readElements_IQ(FILE * f, cmplx *, int amount);
  void readElements_QI(FILE * f, cmplx *, int amount);
  void readElements_I(FILE * f, cmplx *, int amount);
  void readElements_Q(FILE * f, cmplx *, int amount);
  //
  //	for the conversion - if any
  int16_t convBufferSize;
  int16_t convIndex;
  std::vector<cmplx> convBuffer;
  int16_t mapTable_int[2048];
  float mapTable_float[2048];

signals:
  void signal_set_progress(int, int);
};

#endif
