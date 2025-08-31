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

#include  "viterbi-spiral.h"
#include  "fib-decoder_if.h"
#include  <QObject>
#include  <vector>
#include  <atomic>
#include  <cstdio>
#include  <cstdint>

class DabRadio;
class DabParams;

class FicDecoder : public QObject
{
  Q_OBJECT
public:
  FicDecoder(DabRadio * iMr);
  ~FicDecoder() override = default;

  void process_block(const std::vector<i16> & iOfdmSoftBits, const i32 iOfdmSymbIdx);
  void stop();
  void restart();
  void get_fib_bits(u8 *, bool *);
  i32  get_fic_decode_ratio_percent();
  void reset_fic_decode_success_ratio() { mFicDecodeSuccessRatio = 0; };
  void start_fic_dump(FILE *);
  void stop_fic_dump();

  IFibDecoder * get_fib_decoder() { return mpFibDecoder.get(); };

private:
  std::unique_ptr<IFibDecoder> mpFibDecoder;
  static constexpr i32 cViterbiBlockSize = 3072 + 24; // with punctation data
  ViterbiSpiral mViterbi{ cFicSizeVitOut, true };
  std::array<std::byte, cFicPerFrame * cFicSizeVitOut> mFibBitsEntireFrame;
  std::array<std::byte, cFicSizeVitOut> mPRBS;
  std::array<i16, cFicSizeVitIn> mFicViterbiSoftInput;
  std::array<u8, cViterbiBlockSize> mPunctureTable{false};
  std::array<bool, 4> mFicValid{false};
  i16 mIndex = 0;
  i16 mFicIdx = 0;
  i32 mFicBlock = 0;
  i32 mFicErrors = 0;
  i32 mFicBits = 0;
  i32 mFicDecodeSuccessRatio = 0;   // Saturating up/down-counter in range [0, 10] corresponding to the number of FICs with correct CRC
  std::atomic<bool> mIsRunning{false};
  std::atomic<FILE *> mpFicDump{nullptr};

  void _process_fic_input(i16 iFicIdx, bool & oFicValid);
  void _dump_fib_to_file(const std::byte * ipOneFibBits);

signals:
  void signal_fic_status(i32, f32);
};

#endif


