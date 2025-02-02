#
/*
 *    Copyright (C) 2013 .. 2017
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
 */
#

#ifndef  __MSC_HANDLER__
#define  __MSC_HANDLER__

#include <QMutex>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "dab-constants.h"
#include "dab-params.h"
#include "ringbuffer.h"
#include "phasetable.h"
#include "freq-interleaver.h"

class RadioInterface;
class Backend;

class MscHandler
{
public:
  MscHandler(RadioInterface *, uint8_t, RingBuffer<uint8_t> *);
  ~MscHandler();

  void process_block(const std::vector<int16_t> & iSoftBits, int16_t iBlockNr);
  bool set_channel(DescriptorType *, RingBuffer<int16_t> *, RingBuffer<uint8_t> *, FILE *, int);
  void reset_channel();
  void stop_service(const DescriptorType *, int);
  void stop_service(int, int);

private:
  RadioInterface * const mpRadioInterface;
  const DabParams::SDabPar mDabPar;
  RingBuffer<uint8_t> * const mpFrameBuffer;

  QMutex mMutex;
  QVector<QSharedPointer<Backend>> mBackendList;
  std::vector<int16_t> mCifVector;
  int16_t mCifCount = 0;
  int16_t mBlkCount = 0;
  int16_t mBitsPerBlock= 0;
  int16_t mNumberOfBlocksPerCif = 0;
  int16_t mBlockCount = 0;

  void processMsc(int32_t n);
};

#endif


