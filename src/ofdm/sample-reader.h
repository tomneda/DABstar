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
 *    This file is part of the Qt-DAB program
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
#ifndef  SAMPLE_READER_H
#define  SAMPLE_READER_H

#include "dab-constants.h"
#include <QObject>
#include <sndfile.h>
#include <cstdint>
#include <atomic>
#include <vector>
#include <array>
#include "device-handler.h"
#include "ringbuffer.h"
#include <random>


// #define USE_IQ_COMPENSATION  // not well tested

class DabRadio;

class SampleReader : public QObject
{
Q_OBJECT
public:
  SampleReader(const DabRadio * mr, IDeviceHandler * iTheRig, RingBuffer<cf32> * iSpectrumBuffer = nullptr);
  ~SampleReader() override = default;

  void setRunning(bool b);
  [[nodiscard]] f32 get_sLevel() const;
  f32 get_linear_peak_level_and_clear();
  cf32 getSample(i32);
  void getSamples(TArrayTn & oV, const i32 iStartIdx, i32 iNoSamples, const i32 iFreqOffsetBBHz, bool iShowSpec);
  void startDumping(SNDFILE *);
  void stop_dumping();
  void set_dc_removal(bool iRemoveDC);
  void set_cir_buffer(RingBuffer<cf32> * iCirBuffer);

  [[nodiscard]] inline cf32 get_dc_offset() const { return { meanI, meanQ }; }

private:
  static constexpr u16 DUMP_SIZE = 4096;
  static constexpr i32 SPEC_BUFF_SIZE = 2048;
  static constexpr i32 CIR_BUFF_SIZE = 2048*97;

  const DabRadio * const myRadioInterface;
  IDeviceHandler * const theRig;
  RingBuffer<cf32> * spectrumBuffer;
  RingBuffer<cf32> * cirBuffer = nullptr;
  std::array<cf32, SPEC_BUFF_SIZE> specBuff;
  std::array<cf32, INPUT_RATE> oscillatorTable{};
  TArrayTn mSampleBuffer;

  i32 specBuffIdx = 0;
  i32 currentPhase = 0;
  std::atomic<bool> running;
  f32 sLevel = 0.0f;
  i32 sampleCount = 0;
  bool dumping;
  i16 dumpIndex = 0;
  i16 dumpScale;
  std::array<i16, DUMP_SIZE> dumpBuffer{};
  std::atomic<SNDFILE *> dumpfilePointer;
  f32 peakLevel = -1.0e6;
  f32 meanI = 0.0f;
  f32 meanQ = 0.0f;
#ifdef USE_IQ_COMPENSATION
  f32 meanII = 1.0f;
  f32 meanQQ = 1.0f;
  f32 meanIQ = 0.0f;
#endif
  bool dcRemovalActive = false;
  i32 mWholeFrameIndex = 0;
  i32 mWholeFrameCount = 0;
  cf32	mWholeFrameBuff[CIR_BUFF_SIZE];

  void _dump_samples_to_file(const cf32 * const ipV, const i32 iNoSamples);

signals:
  void signal_show_spectrum(int);
  void signal_show_cir(int);
};

#endif
