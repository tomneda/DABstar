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
#

#ifndef  BACKEND_H
#define  BACKEND_H

#include  <QSemaphore>
#include  <vector>

#ifdef  __THREADED_BACKEND__
#include	<QThread>
#include	<atomic>
#endif

#include  "ringbuffer.h"
#include  <cstdio>
#include  "backend-driver.h"
#include  "backend-deconvolver.h"

#define  NUMBER_SLOTS  25

class DabRadio;

#ifdef  __THREADED_BACKEND__
class Backend : public QThread
{
#else

class Backend
{
#endif

public:
  Backend(DabRadio * ipRI, const DescriptorType * ipDescType, RingBuffer<i16> * ipoAudiobuffer, RingBuffer<u8> * ipoDatabuffer, RingBuffer<u8> * frameBuffer, FILE * dump, int flag);
  ~Backend();

  i32 process(const i16 * iV, i16 cnt);
  void stopRunning();

  // we need sometimes to access the key parameters for decoding
  int serviceId;
  int startAddr;
  int Length;
  bool shortForm;
  int protLevel;
  i16 bitRate;
  i16 subChId;
  QString serviceName;
  int borf;
  FILE * dump;

private:
  BackendDeconvolver deconvolver;
  std::vector<u8> outV;
  BackendDriver driver;
#ifdef  __THREADED_BACKEND__
  void run();
  std::atomic<bool> running;
  QSemaphore freeSlots;
  QSemaphore usedSlots;
  std::vector<i16> theData[NUMBER_SLOTS];
  i16 nextIn;
  i16 nextOut;
#endif
  void processSegment(const i16 * iData);
  DabRadio * radioInterface;

  i16 fragmentSize;
  std::vector<std::vector<i16>> interleaveData;
  std::vector<i16> tempX;
  i16 countforInterleaver;
  i16 interleaverIndex;
  std::vector<u8> disperseVector;
};

#endif
