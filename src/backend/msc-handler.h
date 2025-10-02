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

#include "dab-constants.h"
#include "ringbuffer.h"
#include <QVector>
#include <QSharedPointer>
#include <QMutex>
#include <vector>

class DabRadio;
class Backend;

class MscHandler
{
public:
  MscHandler(DabRadio *, RingBuffer<u8> *);
  ~MscHandler();

  void process_block(const std::vector<i16> & iSoftBits, i16 iBlockNr);
  bool set_channel(const SDescriptorType * d, RingBuffer<i16> * ipoAudioBuffer, RingBuffer<u8> * ipoDataBuffer, EProcessFlag iProcessFlag);
  void reset_channel();
  void stop_service(i32 iSubChId, EProcessFlag iProcessFlag);
  bool is_service_running(i32 iSubChId, EProcessFlag iProcessFlag) const;

private:
  DabRadio * const mpRadioInterface;
  RingBuffer<u8> * const mpFrameBuffer;

  mutable QMutex mMutex;
  QVector<QSharedPointer<Backend>> mBackendList;
  std::vector<i16> mCifVector;
  i16 mCifCount = 0;
  i16 mBlkCount = 0;
  i16 mBitsPerBlock= 0;
  i16 mNumberOfBlocksPerCif = 0;
  i16 mBlockCount = 0;

  void processMsc(i32 n);
};

#endif


