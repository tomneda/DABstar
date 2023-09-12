/*
 *    Copyright (C) 2013 .. 2017
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
#include  "sample-reader.h"
#include  "radio.h"
#include  <time.h>

static inline int16_t value_for_bit_pos(int16_t b)
{
  assert(b > 0);
  int16_t res = 1;
  while (--b > 0)
  {
    res <<= 1;
  }
  return res;
}

SampleReader::SampleReader(const RadioInterface * mr, deviceHandler * iTheRig, RingBuffer<cmplx> * iSpectrumBuffer) :
  theRig(iTheRig),
  spectrumBuffer(iSpectrumBuffer),
  oneSampleBuffer(1)
{
  localBuffer.resize(bufferSize);
  dumpfilePointer.store(nullptr);
  dumpScale = value_for_bit_pos(theRig->bitDepth());
  running.store(true);

  for (int i = 0; i < INPUT_RATE; i++)
  {
    oscillatorTable[i] = cmplx((float)cos(2.0 * M_PI * i / INPUT_RATE),
                               (float)sin(2.0 * M_PI * i / INPUT_RATE));
  }

  connect(this, SIGNAL (show_Spectrum(int)), mr, SLOT (showSpectrum(int)));
  connect(this, SIGNAL (show_Corrector(int)), mr, SLOT (set_CorrectorDisplay(int)));
}

void SampleReader::setRunning(bool b)
{
  running.store(b);
}

float SampleReader::get_sLevel() const
{
  return sLevel;
}

cmplx SampleReader::getSample(int32_t phaseOffset)
{
  getSamples(oneSampleBuffer, 0, 1, phaseOffset);
  return oneSampleBuffer[0];
}

void SampleReader::getSamples(std::vector<cmplx> & oV, int32_t index, int32_t n, int32_t phaseOffset)
{
  assert((signed)oV.size() >= index + n);
  
  std::vector<cmplx> buffer(n);

  corrector = phaseOffset;
  if (!running.load()) throw 21;

  if (n > bufferContent)
  {
    _wait_for_sample_buffer_filled(n);
  }

  if (!running.load()) throw 20;

  n = theRig->getSamples(buffer.data(), n);
  bufferContent -= n;

  if (dumpfilePointer.load() != nullptr)
  {
    for (int32_t i = 0; i < n; i++)
    {
      _dump_sample_to_file(oV[i]);
    }
  }

  //	OK, we have samples!!
  //	first: adjust frequency. We need Hz accuracy
  for (int32_t i = 0; i < n; i++)
  {
    currentPhase -= phaseOffset;
    //
    //	Note that "phase" itself might be negative
    currentPhase = (currentPhase + INPUT_RATE) % INPUT_RATE;
    if (localCounter < bufferSize)
    {
      localBuffer[localCounter] = oV[i];
      ++localCounter;
    }
    oV[index + i] = buffer[i] * oscillatorTable[currentPhase];
    sLevel += 0.00001f * (jan_abs(oV[i]) - sLevel);
  }

  sampleCount += n;

  if (sampleCount > INPUT_RATE / 4)
  {
    show_Corrector(corrector);
    if (spectrumBuffer != nullptr)
    {
      spectrumBuffer->putDataIntoBuffer(localBuffer.data(), bufferSize);
      emit show_Spectrum(bufferSize);
    }
    localCounter = 0;
    sampleCount = 0;
  }
}

void SampleReader::_wait_for_sample_buffer_filled(int32_t n)
{
  bufferContent = theRig->Samples();
  while ((bufferContent < n) && running.load())
  {
    constexpr timespec ts{ 0, 10'000L };
    while (nanosleep(&ts, nullptr)); // while is very likely obsolete
    bufferContent = theRig->Samples();
  }
}

void SampleReader::_dump_sample_to_file(const cmplx & v)
{
  dumpBuffer[2 * dumpIndex + 0] = fixround<int16_t>(real(v) * dumpScale);
  dumpBuffer[2 * dumpIndex + 1] = fixround<int16_t>(imag(v) * dumpScale);

  if (++dumpIndex >= DUMPSIZE / 2)
  {
    sf_writef_short(dumpfilePointer.load(), dumpBuffer.data(), dumpIndex);
    dumpIndex = 0;
  }
}

void SampleReader::startDumping(SNDFILE * f)
{
  dumpfilePointer.store(f);
}

void SampleReader::stopDumping()
{
  dumpfilePointer.store(nullptr);
}

