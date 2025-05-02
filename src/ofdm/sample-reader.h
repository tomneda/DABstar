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

// Note: It was found that enlarging the buffersize to e.g. 8192 cannot be handled properly by the underlying system.
class DabRadio;

class SampleReader : public QObject
{
Q_OBJECT
public:
  SampleReader(const DabRadio * mr, IDeviceHandler * iTheRig, RingBuffer<cmplx> * iSpectrumBuffer = nullptr);
  ~SampleReader() override = default;

  void setRunning(bool b);
  [[nodiscard]] float get_sLevel() const;
  float get_linear_peak_level_and_clear();
  cmplx getSample(int32_t);
  void getSamples(TArrayTn & oV, const int32_t iStartIdx, int32_t iNoSamples, const int32_t iFreqOffsetBBHz, bool iShowSpec);
  void startDumping(SNDFILE *);
  void stop_dumping();
  void set_dc_removal(bool iRemoveDC);
  void set_cir_buffer(RingBuffer<cmplx> * iCirBuffer);

  [[nodiscard]] inline cmplx get_dc_offset() const { return { meanI, meanQ }; }

private:
  static constexpr uint16_t DUMP_SIZE = 4096;
  static constexpr int32_t SPEC_BUFF_SIZE = 2048;
  static constexpr int32_t CIR_BUFF_SIZE = 2048*97;

  const DabRadio * const myRadioInterface;
  IDeviceHandler * const theRig;
  RingBuffer<cmplx> * spectrumBuffer;
  RingBuffer<cmplx> * cirBuffer = nullptr;
  std::array<cmplx, SPEC_BUFF_SIZE> specBuff;
  std::array<cmplx, INPUT_RATE> oscillatorTable{};
  TArrayTn mSampleBuffer;

  int32_t specBuffIdx = 0;
  int32_t currentPhase = 0;
  std::atomic<bool> running;
  float sLevel = 0.0f;
  int32_t sampleCount = 0;
  bool dumping;
  int16_t dumpIndex = 0;
  int16_t dumpScale;
  std::array<int16_t, DUMP_SIZE> dumpBuffer{};
  std::atomic<SNDFILE *> dumpfilePointer;
  float peakLevel = -1.0e6;
  float meanI = 0.0f;
  float meanQ = 0.0f;
  float meanII = 1.0f;
  float meanQQ = 1.0f;
  float meanIQ = 0.0f;
  bool dcRemovalActive = false;
  int32_t mWholeFrameIndex = 0;
  int32_t mWholeFrameCount = 0;
  cmplx	mWholeFrameBuff[CIR_BUFF_SIZE];

  void _dump_samples_to_file(const cmplx * const ipV, const int32_t iNoSamples);

signals:
  void signal_show_spectrum(int);
  void signal_show_cir(int);
};

#endif
