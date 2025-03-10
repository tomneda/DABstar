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
class RadioInterface;

class SampleReader : public QObject
{
Q_OBJECT
public:
  SampleReader(const RadioInterface * mr, IDeviceHandler * iTheRig, RingBuffer<cmplx> * iSpectrumBuffer = nullptr);
  ~SampleReader() override = default;

  void setRunning(bool b);
  [[nodiscard]] float get_sLevel() const;
  float get_linear_peak_level_and_clear();
  cmplx getSample(int32_t);
  void getSamples(std::vector<cmplx> & oV, const int32_t iStartIdx, int32_t iNoSamples, const int32_t iFreqOffsetBBHz, bool iShowSpec);
  void startDumping(SNDFILE *);
  void stop_dumping();
  bool check_clipped_and_clear();
  void set_dc_removal(bool iRemoveDC);

  [[nodiscard]] inline cmplx get_dc_offset() const { return { meanI, meanQ }; }

private:
  static constexpr uint16_t DUMP_SIZE = 4096;
  static constexpr int32_t SPEC_BUFF_SIZE = 2048;

  RadioInterface * myRadioInterface;
  IDeviceHandler * theRig;
  RingBuffer<cmplx> * spectrumBuffer;
  std::array<cmplx, SPEC_BUFF_SIZE> specBuff;
  std::array<cmplx, INPUT_RATE> oscillatorTable{};
  std::vector<cmplx> oneSampleBuffer;

  int32_t specBuffIdx = 0;
  int32_t currentPhase = 0;
  std::atomic<bool> running;
  int32_t bufferContent = 0;
  float sLevel = 0.0f;
  int32_t sampleCount = 0;
  bool dumping; 
  int16_t dumpIndex = 0;
  int16_t dumpScale;
  std::array<int16_t, DUMP_SIZE> dumpBuffer{};
  std::atomic<SNDFILE *> dumpfilePointer;
  bool clippingOccurred = false;
  float peakLevel = -1.0e6;
  float meanI = 0.0f;
  float meanQ = 0.0f;
  float meanII = 1.0f;
  float meanQQ = 1.0f;
  float meanIQ = 0.0f;
  bool dcRemovalActive = false;

  void _dump_samples_to_file(const cmplx * const ipV, const int32_t iNoSamples);
  void _wait_for_sample_buffer_filled(int32_t n);

signals:
  void signal_show_spectrum(int);
};

#endif
