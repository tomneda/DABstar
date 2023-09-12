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
/*
 *	Reading the samples from the input device. Since it has its own
 *	"state", we embed it into its own class
 */
#include  "dab-constants.h"
#include  <QObject>
#include  <sndfile.h>
#include  <cstdint>
#include  <atomic>
#include  <vector>
#include  "device-handler.h"
#include  "ringbuffer.h"
//
//      Note:
//      It was found that enlarging the buffersize to e.g. 8192
//      cannot be handled properly by the underlying system.
class RadioInterface;

class SampleReader : public QObject
{
Q_OBJECT
public:
  SampleReader(const RadioInterface * mr, deviceHandler * iTheRig, RingBuffer<cmplx> * iSpectrumBuffer = nullptr);
  ~SampleReader() override = default;

  void setRunning(bool b);
  float get_sLevel() const;
  cmplx getSample(int32_t);
  void getSamples(std::vector<cmplx> & oV, int32_t index, int32_t n, int32_t phase);
  void startDumping(SNDFILE *);
  void stopDumping();

private:
  static constexpr uint16_t DUMPSIZE = 4096;

  RadioInterface * myRadioInterface;
  deviceHandler * theRig;
  RingBuffer<cmplx> * spectrumBuffer;
  std::vector<cmplx> localBuffer;
  std::array<cmplx, INPUT_RATE> oscillatorTable{};
  std::vector<cmplx> oneSampleBuffer;

  int32_t localCounter = 0;
  const int32_t bufferSize  = 32768;
  int32_t currentPhase = 0;
  std::atomic<bool> running;
  int32_t bufferContent = 0;
  float sLevel = 0.0f;
  int32_t sampleCount = 0;
  int32_t corrector = 0;
  bool dumping;
  int16_t dumpIndex = 0;
  int16_t dumpScale;
  std::array<int16_t, DUMPSIZE> dumpBuffer{};
  std::atomic<SNDFILE *> dumpfilePointer;

  void _dump_sample_to_file(const cmplx & v);
  void _wait_for_sample_buffer_filled(int32_t n);

signals:
  void show_Spectrum(int);
  void show_Corrector(int);
};

#endif
