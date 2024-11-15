/*
* This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2023
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
//
//	simple writer to write a "riff/wav" file with
//	arbitrary samplerate,
//	x channels
//	16 bit ints
//	PCM format
//
#include	"wav_writer.h"

bool WavWriter::init(const QString & fileName, const uint32_t iSampleRate, const uint16_t iNumChannels)
{
  static const char * const cRiffStr = "RIFF";
  static const char * const cWaveStr = "WAVE";
  static const char * const cFmtStr  = "fmt ";
  static const char * const cDataStr = "data";

  std::lock_guard lock(mMutex);
  mIsValid = false;
  mFilePointer = fopen(fileName.toUtf8().data(), "wb");
  if (mFilePointer == nullptr)
    return false;

  mNumChannels = iNumChannels;
  mLocationCounter = 0;

  fwrite(cRiffStr, 1, 4, mFilePointer);
  mLocationCounter += 4;

  // the ultimate filesize is written at location 4
  fseek(mFilePointer, 4, SEEK_CUR);
  mLocationCounter += 4;
  fwrite(cWaveStr, 1, 4, mFilePointer);
  mLocationCounter += 4;

  // The default header:
  fwrite(cFmtStr, 1, 4, mFilePointer);
  mLocationCounter += 4;
  const uint32_t fmtSize = 16;
  fwrite(&fmtSize, 1, 4, mFilePointer);
  mLocationCounter += 4;
  const uint16_t formatTag = 1; // == PCM integer
  fwrite(&formatTag, 1, sizeof(uint16_t), mFilePointer);
  mLocationCounter += 2;
  const uint16_t nrChannels = mNumChannels;
  fwrite(&nrChannels, 1, sizeof(uint16_t), mFilePointer);
  mLocationCounter += 2;
  const uint32_t samplingRate = iSampleRate;
  fwrite(&samplingRate, 1, sizeof(uint32_t), mFilePointer);
  mLocationCounter += 4;
  const uint16_t bytesPerBlock = mNumChannels * 2 /*bytesPerSamp*/;
  const uint32_t bytesPerSecond = bytesPerBlock * samplingRate;
  fwrite(&bytesPerSecond, 1, sizeof(uint32_t), mFilePointer);
  mLocationCounter += 4;
  fwrite(&bytesPerBlock, 1, sizeof(uint16_t), mFilePointer);
  mLocationCounter += 2;
  const uint16_t bitsPerSample = 16;
  fwrite(&bitsPerSample, 1, sizeof(uint16_t), mFilePointer);
  mLocationCounter += 2;

  // start of the "data" chunk
  fwrite(cDataStr, 1, 4, mFilePointer);
  mLocationCounter += 4;

  assert(mLocationCounter == 44 - 4);  // minus datasize

  mNrElements = 0;
  mIsValid = true;
  return true;
}

void WavWriter::close()
{
  std::lock_guard lock(mMutex);

  if (!mIsValid)
    return;

  mIsValid = false;
  const uint32_t nrBytes = mNrElements * mNumChannels * sizeof(int16_t);

  // reset the fp to the location where the nr bytes in the data chunk should be written
  fseek(mFilePointer, mLocationCounter, SEEK_SET);
  fwrite(&nrBytes, 1, 4, mFilePointer);

  // compute the overall file size
  fseek(mFilePointer, 0, SEEK_END);
  const uint32_t riffSize = (uint32_t)ftell(mFilePointer) - 8;

  // and record the value at loc 4
  fseek(mFilePointer, 4, SEEK_SET);
  fwrite(&riffSize, 1, 4, mFilePointer);
  fseek(mFilePointer, 0, SEEK_END);
  fclose(mFilePointer);
}

void WavWriter::write(const int16_t * iBuffer, const int32_t iSamples)
{
  std::lock_guard lock(mMutex);

  if (!mIsValid)
    return;

  fwrite(iBuffer, sizeof(int16_t), iSamples, mFilePointer);
  mNrElements += iSamples;
}
