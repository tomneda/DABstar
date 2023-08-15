/*
 *    Copyright (C) 2013 .. 2020
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

/*
 * 	FIC data
 */
#ifndef  FIC_HANDLER_H
#define  FIC_HANDLER_H

#include  <QObject>
#include  <QMutex>
#include  <cstdio>
#include  <cstdint>
#include  <vector>
#include  <atomic>
#include  "viterbi-spiral.h"
#include  "dab-params.h"
#include  "fib-decoder.h"

class RadioInterface;
class DabParams;

class FicHandler : public FibDecoder
{
Q_OBJECT
public:
  FicHandler(RadioInterface *, uint8_t);
  ~FicHandler();
  void process_ficBlock(const std::vector<int16_t> & data, int32_t blkno);
  void stop();
  void restart();
  void start_ficDump(FILE *);
  void stop_ficDump();
  void get_fibBits(uint8_t *);
private:
  DabParams params;
  ViterbiSpiral myViterbi;
  std::array<uint8_t, 768> bitBuffer_out;
  std::array<int16_t,2304> ofdm_input;
  std::array<bool, 3072 + 24> punctureTable;
  std::array<uint8_t, 4 * 768> fibBits;

  void process_ficInput(int16_t);
  int16_t index;
  int16_t BitsperBlock;
  int16_t ficno;
  int16_t ficBlocks;
  int16_t ficMissed;
  int16_t ficRatio;
  uint16_t convState;
  uint8_t PRBS[768];
  FILE * ficDumpPointer;
  QMutex ficLocker;
  uint8_t ficBuffer[256];
  int ficPointer;
  std::atomic<bool> running;
  //	uint8_t		shiftRegister	[9];
signals:
  void show_ficSuccess(bool);
};

#endif


