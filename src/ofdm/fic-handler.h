/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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
#include  "fib-decoder.h"

class DabRadio;

class DabParams;

class FicHandler : public FibDecoder
{
Q_OBJECT
public:
  FicHandler(DabRadio * const iMr);
  ~FicHandler() override = default;

  void process_block(const std::vector<i16> & iData, const i32 iBlkNo);
  void stop();
  void restart();
  void start_fic_dump(FILE *);
  void stop_fic_dump();
  void get_fib_bits(u8 *, bool *);
  i32  get_fic_decode_ratio_percent();
  void reset_fic_decode_success_ratio() { fic_decode_success_ratio = 0; };

private:
  ViterbiSpiral myViterbi{ 768, true };
  std::array<std::byte, 768> bitBuffer_out;
  std::array<std::byte, 4 * 768> fibBits;
  std::array<std::byte, 768> PRBS;
  std::array<std::byte, 256> ficBuffer;
  std::array<i16, 2304> ofdm_input;
  std::array<u8, 3072 + 24> punctureTable{};
  std::array<bool, 4> ficValid{ false };
  i16 index = 0;
  i16 BitsperBlock = 0;
  i16 ficno = 0;
  FILE * ficDumpPointer = nullptr;
  QMutex ficLocker;
  i32 fic_block = 0;
  i32 fic_errors = 0;
  i32 fic_bits = 0;
  std::atomic<bool> running;
  void _process_fic_input(i16 iFicNo, bool * oValid);

  // Saturating up/down-counter in range [0, 10] corresponding
  // to the number of FICs with correct CRC
  i32 fic_decode_success_ratio = 0;

signals:
  void show_fic_success(bool);
  void show_fic_BER(f32);
};

#endif


