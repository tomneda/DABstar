/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2019
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
#include  "xml-reader.h"
#include  "xml-descriptor.h"
#include  "xml-filereader.h"
#include  <cstdio>
#include  <sys/time.h>

// this is a wrapper to avoid "ignoring return value of ... declared with attribute ‘warn_unused_result’"
static size_t fread_chk(void * iPtr, size_t iSize, size_t iN, FILE * iStream)
{
  return fread(iPtr, iSize, iN, iStream); // TODO: some check should be done here?!
}

static i32 shift(i32 a)
{
  i32 r = 1;
  while (--a > 0)
  {
    r <<= 1;
  }
  return r;
}

static inline cf32 compmul(cf32 a, f32 b)
{
  return cf32(real(a) * b, imag(a) * b);
}

static inline u64 currentTime()
{
  struct timeval tv;

  gettimeofday(&tv, nullptr);
  return (u64)(tv.tv_sec * 1000000 + (u64)tv.tv_usec);
}

XmlReader::XmlReader(XmlFileReader * mr, FILE * f, XmlDescriptor * fd, u32 filePointer, RingBuffer<cf32> * b)
{
  this->parent = mr;
  this->file = f;
  this->fd = fd;
  this->filePointer = filePointer;
  sampleBuffer = b;
  //
  //	convBufferSize is a little confusing since the actual
  //	buffer is one larger
  convBufferSize = fd->sampleRate / 1000;
  continuous.store(mr->cbLoopFile->isChecked());

  for (i32 i = 0; i < 2048; i++)
  {
    const f32 inVal = f32(fd->sampleRate / 1000);
    mapTable_int[i] = (i16)(floor(i * (inVal / 2048.0)));
    mapTable_float[i] = i * (inVal / 2048.0f) - mapTable_int[i];
  }

  for(i32 i = 0; i < 256; i++)
  {
    mapTable[i] = ((f32)i - 127.38f) / 128.0f;
  }

  convIndex = 0;
  convBuffer.resize(convBufferSize + 1);
  nrElements = fd->blockList[0].nrElements;

  connect(this, &XmlReader::signal_set_progress, parent, &XmlFileReader::slot_set_progress);

  // fprintf(stderr, "reader task wordt gestart\n");
  mSetNewFilePos = -1;
  fseek(f, 0, SEEK_END);
  mFileLength = ftell(f);
  fseek(f, 0, SEEK_SET);

  start();
}

XmlReader::~XmlReader()
{
  running.store(false);
  while (this->isRunning())
  {
    usleep(1000);
  }
  mSetNewFilePos = -1;
}

void XmlReader::stopReader()
{
  if (!running.load())
  {
    return;
  }
  running.store(false);
  while (isRunning())
  {
    usleep(1000);
  }
}

void XmlReader::jump_to_relative_position(i32 iPerMill)
{
  if (running.load())
  {
    mSetNewFilePos = samplesToRead * iPerMill / 1000LL;
  }
}

void XmlReader::run()
{
  i64 samplesRead = 0;
  i64 samplesReadToUpdate = 0;
  u64 nextStop;
  i32 startPoint = filePointer;

  running.store(true);
  fseek(file, filePointer, SEEK_SET);
  nextStop = currentTime();
  for (i32 blocks = 0; blocks < fd->nrBlocks; blocks++)
  {
    samplesToRead = compute_nrSamples(file, blocks);
    qDebug() << "samples to read" << samplesToRead;
    samplesRead = 0;

    do
    {
      samplesReadToUpdate = 0;
      while ((samplesRead <= samplesToRead) && running.load())
      {

        if (fd->iqOrder == "IQ")
        {
          samplesRead += readSamples(file, &XmlReader::readElements_IQ);
        }
        else if (fd->iqOrder == "QI")
        {
          samplesRead += readSamples(file, &XmlReader::readElements_QI);
        }
        else if (fd->iqOrder == "I_Only")
        {
          samplesRead += readSamples(file, &XmlReader::readElements_I);
        }
        else
        {
          samplesRead += readSamples(file, &XmlReader::readElements_Q);
        }

        if (mSetNewFilePos >= 0)
        {
          // ensure begin of I/Q position
          i64 pos = startPoint + (((mFileLength - startPoint) * mSetNewFilePos / samplesToRead));
          pos = (pos - (pos % 24)); // assume also 24bit (2*3Bytes) and 32-bit (2*4Bytes) format files (LCM of 6 and 8 = 24)
          fseek(file, pos, SEEK_SET);
          samplesRead = mSetNewFilePos;
          mSetNewFilePos = -1 ;
          samplesReadToUpdate = 0; // retrigger emit below
        }

        if (samplesRead >= samplesReadToUpdate)
        {
          emit signal_set_progress(samplesRead, samplesToRead);
          samplesReadToUpdate = samplesRead + fd->sampleRate / 6; // for updating 6 times per second (similar to WAV and RAW files)
        }

        // the readSamples function returns 1 msec of data, we assume taking this data does not take time
        nextStop = nextStop + (u64)1000;
        if (nextStop > currentTime())
        {
          usleep(nextStop - currentTime());
        }
      }
      emit signal_set_progress(0, samplesToRead);
      filePointer = startPoint;
      fseek(file, filePointer, SEEK_SET);
      samplesRead = 0;
    }
    while (running.load() && continuous.load());
  }
}

bool XmlReader::handle_continuousButton()
{
  continuous.store(!continuous.load());
  return continuous.load();
}

i64 XmlReader::compute_nrSamples(FILE * f, i32 blockNumber)
{
  i64 nrElements = fd->blockList.at(blockNumber).nrElements;
  i64 samplesToRead = 0;

  (void)f;
  if (fd->blockList.at(blockNumber).typeofUnit == "Channel")
  {
    if ((fd->iqOrder == "IQ") || (fd->iqOrder == "QI"))
    {
      samplesToRead = nrElements / 2;
    }
    else
    {
      samplesToRead = nrElements;
    }
  }
  else
  {  // typeofUnit = "sample"
    samplesToRead = nrElements;
  }

  qDebug() << samplesToRead << "samples to read, order is" << fd->iqOrder.toLatin1();
  return samplesToRead;
}

i32 XmlReader::readSamples(FILE * theFile, void(XmlReader::*r)(FILE * theFile, cf32 *, i32))
{
  cf32 temp[2048];

  (*this.*r)(theFile, &convBuffer[1], convBufferSize);
  for (i32 i = 0; i < 2048; i++)
  {
    i16 inpBase = mapTable_int[i];
    f32 inpRatio = mapTable_float[i];
    temp[i] = compmul(convBuffer[inpBase + 1], inpRatio) + compmul(convBuffer[inpBase], 1 - inpRatio);
  }
  convBuffer[0] = convBuffer[convBufferSize];
  convIndex = 1;
  sampleBuffer->put_data_into_ring_buffer(temp, 2048);
  return 2048;
}

//	the readers
void XmlReader::readElements_IQ(FILE * theFile, cf32 * buffer, i32 amount)
{
  i32 nrBits = fd->bitsperChannel;
  f32 scaler = f32(shift(nrBits));

  if (fd->container == "int8")
  {
    auto * const lbuf = make_vla(u8, 2 * amount);
    fread_chk(lbuf, 1, 2 * amount, theFile);
    for (i32 i = 0; i < amount; i++)
    {
      buffer[i] = cf32(((i8)lbuf[2 * i]) / 127.0f, ((i8)lbuf[2 * i + 1]) / 127.0f);
    }
    return;
  }

  if (fd->container == "uint8")
  {
    auto * const lbuf = make_vla(u8, 2 * amount);
    fread_chk(lbuf, 1, 2 * amount, theFile);
    for (i32 i = 0; i < amount; i++)
    {
      buffer[i] = cf32(mapTable[lbuf[2 * i]], mapTable[lbuf[2 * i + 1]]);
    }
    return;
  }

  if (fd->container == "int16")
  {
    auto * const lbuf = make_vla(u8, 4 * amount);
    fread_chk(lbuf, 2, 2 * amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        i16 temp_16_1 = (lbuf[4 * i] << 8) | lbuf[4 * i + 1];
        i16 temp_16_2 = (lbuf[4 * i + 2] << 8) | lbuf[4 * i + 3];
        buffer[i] = cf32((f32)temp_16_1 / scaler, (f32)temp_16_2 / scaler);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        i16 temp_16_1 = (lbuf[4 * i + 1] << 8) | lbuf[4 * i];
        i16 temp_16_2 = (lbuf[4 * i + 3] << 8) | lbuf[4 * i + 2];
        buffer[i] = cf32((f32)temp_16_1 / scaler, (f32)temp_16_2 / scaler);
      }
    }
    return;
  }

  if (fd->container == "int24")
  {
    auto * const lbuf = make_vla(u8, 6 * amount);
    fread_chk(lbuf, 3, 2 * amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[6 * i] << 16) | (lbuf[6 * i + 1] << 8) | lbuf[6 * i + 2];
        i32 temp_32_2 = (lbuf[6 * i + 3] << 16) | (lbuf[4 * i + 4] << 8) | lbuf[6 * i + 5];
        if (temp_32_1 & 0x800000)
        {
          temp_32_1 |= 0xFF000000;
        }
        if (temp_32_2 & 0x800000)
        {
          temp_32_2 |= 0xFF000000;
        }
        buffer[i] = cf32((f32)temp_32_1 / scaler, (f32)temp_32_2 / scaler);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[6 * i + 2] << 16) | (lbuf[6 * i + 1] << 8) | lbuf[6 * i];
        i32 temp_32_2 = (lbuf[6 * i + 5] << 16) | (lbuf[6 * i + 4] << 8) | lbuf[6 * i + 3];
        if (temp_32_1 & 0x800000)
        {
          temp_32_1 |= 0xFF000000;
        }
        if (temp_32_2 & 0x800000)
        {
          temp_32_2 |= 0xFF000000;
        }
        buffer[i] = cf32((f32)temp_32_1 / scaler, (f32)temp_32_2 / scaler);
      }
    }
    return;
  }

  if (fd->container == "int32")
  {
    auto * const lbuf = make_vla(u8, 8 * amount);
    fread_chk(lbuf, 4, 2 * amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[8 * i] << 24) | (lbuf[8 * i + 1] << 16) | (lbuf[8 * i + 2] << 8) | lbuf[8 * i + 3];
        i32 temp_32_2 = (lbuf[8 * i + 4] << 24) | (lbuf[8 * i + 5] << 16) | (lbuf[8 * i + 6] << 8) | lbuf[8 * i + 7];
        buffer[i] = cf32((f32)temp_32_1 / scaler, (f32)temp_32_2 / scaler);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[8 * i + 3] << 24) | (lbuf[8 * i + 2] << 16) | (lbuf[8 * i + 1] << 8) | lbuf[8 * i];
        i32 temp_32_2 = (lbuf[8 * i + 7] << 24) | (lbuf[8 * i + 6] << 16) | (lbuf[8 * i + 5] << 8) | lbuf[8 * i + 4];
        buffer[i] = cf32((f32)temp_32_1 / scaler, (f32)temp_32_2 / scaler);
      }
    }
    return;
  }

  if (fd->container == "float32")
  {
    auto * const lbuf = make_vla(u8, 8 * amount);
    fread_chk(lbuf, 4, 2 * amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        UCnv c1, c2;
        c1.int32Data = (lbuf[8 * i] << 24) | (lbuf[8 * i + 1] << 16) | (lbuf[8 * i + 2] << 8) | lbuf[8 * i + 3];
        c2.int32Data = (lbuf[8 * i + 4] << 24) | (lbuf[8 * i + 5] << 16) | (lbuf[8 * i + 6] << 8) | lbuf[8 * i + 7];
        buffer[i] = cf32(c1.floatData, c2.floatData);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        UCnv c1, c2;
        c1.int32Data = (lbuf[8 * i + 3] << 24) | (lbuf[8 * i + 2] << 16) | (lbuf[8 * i + 1] << 8) | lbuf[8 * i];
        c2.int32Data = (lbuf[8 * i + 7] << 24) | (lbuf[8 * i + 6] << 16) | (lbuf[8 * i + 5] << 8) | lbuf[8 * i + 4];
        buffer[i] = cf32(c1.floatData, c2.floatData);
      }
    }
    return;
  }
}

void XmlReader::readElements_QI(FILE * theFile, cf32 * buffer, i32 amount)
{

  i32 nrBits = fd->bitsperChannel;
  f32 scaler = f32(shift(nrBits));

  if (fd->container == "int8")
  {
    auto * const lbuf = make_vla(u8, 2 * amount);
    fread_chk(lbuf, 1, 2 * amount, theFile);
    for (i32 i = 0; i < amount; i++)
    {
      buffer[i] = cf32(((i8)lbuf[2 * i + 1]) / 127.0, ((i8)lbuf[2 * i]) / 127.0);
    }
    return;
  }

  if (fd->container == "uint8")
  {
    auto * const lbuf = make_vla(u8, 2 * amount);
    fread_chk(lbuf, 1, 2 * amount, theFile);
    for (i32 i = 0; i < amount; i++)
    {
      buffer[i] = cf32(mapTable[2 * i + 1], mapTable[2 * i]);
    }
    return;
  }

  if (fd->container == "int16")
  {
    auto * const lbuf = make_vla(u8, 4 * amount);
    fread_chk(lbuf, 2, 2 * amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        i16 temp_16_1 = (lbuf[4 * i] << 8) | lbuf[4 * i + 1];
        i16 temp_16_2 = (lbuf[4 * i + 2] << 8) | lbuf[4 * i + 3];
        buffer[i] = cf32((f32)temp_16_2 / scaler, (f32)temp_16_1 / scaler);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        i16 temp_16_1 = (lbuf[4 * i + 1] << 8) | lbuf[4 * i];
        i16 temp_16_2 = (lbuf[4 * i + 3] << 8) | lbuf[4 * i + 2];
        buffer[i] = cf32((f32)temp_16_2 / scaler, (f32)temp_16_1 / scaler);
      }
    }
    return;
  }

  if (fd->container == "int24")
  {
    auto * const lbuf = make_vla(u8, 6 * amount);
    fread_chk(lbuf, 3, 2 * amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[6 * i] << 16) | (lbuf[6 * i + 1] << 8) | lbuf[6 * i + 2];
        i32 temp_32_2 = (lbuf[6 * i + 3] << 16) | (lbuf[4 * i + 4] << 8) | lbuf[6 * i + 5];
        if (temp_32_1 & 0x800000)
        {
          temp_32_1 |= 0x7F000000;
        }
        if (temp_32_2 & 0x800000)
        {
          temp_32_2 |= 0x7F000000;
        }
        buffer[i] = cf32((f32)temp_32_2 / scaler, (f32)temp_32_1 / scaler);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[6 * i + 2] << 16) | (lbuf[6 * i + 1] << 8) | lbuf[6 * i];
        i32 temp_32_2 = (lbuf[6 * i + 5] << 16) | (lbuf[6 * i + 4] << 8) | lbuf[6 * i + 3];
        if (temp_32_1 & 0x800000)
        {
          temp_32_1 |= 0xFF000000;
        }
        if (temp_32_2 & 0x800000)
        {
          temp_32_2 |= 0xFF000000;
        }
        buffer[i] = cf32((f32)temp_32_2 / scaler, (f32)temp_32_1 / scaler);
      }
    }
    return;
  }

  if (fd->container == "int32")
  {
    auto * const lbuf = make_vla(u8, 8 * amount);
    fread_chk(lbuf, 4, 2 * amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[8 * i] << 24) | (lbuf[8 * i + 1] << 16) | (lbuf[8 * i + 2] << 8) | lbuf[8 * i + 3];
        i32 temp_32_2 = (lbuf[8 * i + 4] << 24) | (lbuf[8 * i + 5] << 16) | (lbuf[8 * i + 6] << 8) | lbuf[8 * i + 7];
        buffer[i] = cf32((f32)temp_32_2 / scaler, (f32)temp_32_1 / scaler);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[8 * i + 3] << 24) | (lbuf[8 * i + 2] << 16) | (lbuf[8 * i + 1] << 8) | lbuf[8 * i];
        i32 temp_32_2 = (lbuf[8 * i + 7] << 24) | (lbuf[8 * i + 6] << 16) | (lbuf[8 * i + 5] << 8) | lbuf[8 * i + 4];
        buffer[i] = cf32((f32)temp_32_2 / scaler, (f32)temp_32_1 / scaler);
      }
    }
    return;
  }

  if (fd->container == "float32")
  {
    auto * const lbuf = make_vla(u8, 8 * amount);
    fread_chk(lbuf, 4, 2 * amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        UCnv c1, c2;
        c1.int32Data = (lbuf[8 * i] << 24) | (lbuf[8 * i + 1] << 16) | (lbuf[8 * i + 2] << 8) | lbuf[8 * i + 3];
        c2.int32Data = (lbuf[8 * i + 4] << 24) | (lbuf[8 * i + 5] << 16) | (lbuf[8 * i + 6] << 8) | lbuf[8 * i + 7];
        buffer[i] = cf32(c1.floatData, c2.floatData);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        UCnv c1, c2;
        c1.int32Data = (lbuf[8 * i + 3] << 24) | (lbuf[8 * i + 2] << 16) | (lbuf[8 * i + 1] << 8) | lbuf[8 * i];
        c2.int32Data = (lbuf[8 * i + 7] << 24) | (lbuf[8 * i + 6] << 16) | (lbuf[8 * i + 5] << 8) | lbuf[8 * i + 4];
        buffer[i] = cf32(c1.floatData, c2.floatData);
      }
    }
    return;
  }
}

void XmlReader::readElements_I(FILE * theFile, cf32 * buffer, i32 amount)
{

  i32 nrBits = fd->bitsperChannel;
  f32 scaler = f32(shift(nrBits));

  if (fd->container == "int8")
  {
    auto * const lbuf = make_vla(u8, amount);
    fread_chk(lbuf, 1, amount, theFile);
    for (i32 i = 0; i < amount; i++)
    {
      buffer[i] = cf32((i8)lbuf[i] / 127.0, 0);
    }
    return;
  }

  if (fd->container == "uint8")
  {
    auto * const lbuf = make_vla(u8, amount);
    fread_chk(lbuf, 1, amount, theFile);
    for (i32 i = 0; i < amount; i++)
    {
      buffer[i] = cf32(mapTable[lbuf[i]], 0);
    }
    return;
  }

  if (fd->container == "int16")
  {
    auto * const lbuf = make_vla(u8, 2 * amount);
    fread_chk(lbuf, 2, amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        i16 temp_16_1 = (lbuf[2 * i] << 8) | lbuf[2 * i + 1];
        buffer[i] = cf32((f32)temp_16_1 / scaler, 0);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        i16 temp_16_1 = (lbuf[2 * i + 1] << 8) | lbuf[2 * i];
        buffer[i] = cf32((f32)temp_16_1 / scaler, 0);
      }
    }
    return;
  }

  if (fd->container == "int24")
  {
    auto * const lbuf = make_vla(u8, 3 * amount);
    fread_chk(lbuf, 3, amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[3 * i] << 16) | (lbuf[3 * i + 1] << 8) | lbuf[3 * i + 2];
        if (temp_32_1 & 0x800000)
        {
          temp_32_1 |= 0xFF000000;
        }
        buffer[i] = cf32((f32)temp_32_1 / scaler, 0);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[3 * i + 2] << 16) | (lbuf[3 * i + 1] << 8) | lbuf[3 * i];
        if (temp_32_1 & 0x800000)
        {
          temp_32_1 |= 0xFF000000;
        }
        buffer[i] = cf32((f32)temp_32_1 / scaler, 0);
      }
    }
    return;
  }

  if (fd->container == "int32")
  {
    auto * const lbuf = make_vla(u8, 4 * amount);
    fread_chk(lbuf, 4, amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[4 * i] << 24) | (lbuf[4 * i + 1] << 16) | (lbuf[4 * i + 2] << 8) | lbuf[4 * i + 3];
        buffer[i] = cf32((f32)temp_32_1 / scaler, 0);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[4 * i + 3] << 24) | (lbuf[4 * i + 2] << 16) | (lbuf[4 * i + 1] << 8) | lbuf[4 * i];
        buffer[i] = cf32((f32)temp_32_1 / scaler, 0);
      }
    }
    return;
  }

  if (fd->container == "float32")
  {
    auto * const lbuf = make_vla(u8, 4 * amount);
    fread_chk(lbuf, 4, amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        UCnv c;
        c.int32Data = (lbuf[4 * i] << 24) | (lbuf[4 * i + 1] << 16) | (lbuf[4 * i + 2] << 8) | lbuf[4 * i + 3];
        buffer[i] = cf32(c.floatData, 0);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        UCnv c;
        c.int32Data = (lbuf[4 * i + 3] << 24) | (lbuf[4 * i + 2] << 16) | (lbuf[4 * i + 1] << 8) | lbuf[4 * i];
        buffer[i] = cf32(c.floatData, 0);
      }
    }
    return;
  }
}

void XmlReader::readElements_Q(FILE * theFile, cf32 * buffer, i32 amount)
{

  i32 nrBits = fd->bitsperChannel;
  f32 scaler = f32(shift(nrBits));

  if (fd->container == "int8")
  {
    auto * const lbuf = make_vla(u8, amount);
    fread_chk(lbuf, 1, amount, theFile);
    for (i32 i = 0; i < amount; i++)
    {
      buffer[i] = cf32(127.0, ((i8)lbuf[i] / 127.0));
    }
    return;
  }

  if (fd->container == "uint8")
  {
    auto * const lbuf = make_vla(u8, amount);
    fread_chk(lbuf, 1, amount, theFile);
    for (i32 i = 0; i < amount; i++)
    {
      buffer[i] = cf32(0, mapTable[lbuf[i]]);
    }
    return;
  }

  if (fd->container == "int16")
  {
    auto * const lbuf = make_vla(u8, 2 * amount);
    fread_chk(lbuf, 2, amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        i16 temp_16_1 = (lbuf[2 * i] << 8) | lbuf[2 * i + 1];
        buffer[i] = cf32(0, (f32)temp_16_1 / scaler);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        i16 temp_16_1 = (lbuf[2 * i + 1] << 8) | lbuf[2 * i];
        buffer[i] = cf32(0, (f32)temp_16_1 / scaler);
      }
    }
    return;
  }

  if (fd->container == "int24")
  {
    auto * const lbuf = make_vla(u8, 3 * amount);
    fread_chk(lbuf, 3, amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[3 * i] << 16) | (lbuf[3 * i + 1] << 8) | lbuf[3 * i + 2];
        if (temp_32_1 & 0x800000)
        {
          temp_32_1 |= 0xFF000000;
        }
        buffer[i] = cf32(0, (f32)temp_32_1 / scaler);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[3 * i + 2] << 16) | (lbuf[3 * i + 1] << 8) | lbuf[3 * i];
        if (temp_32_1 & 0x800000)
        {
          temp_32_1 |= 0xFF000000;
        }
        buffer[i] = cf32(0, (f32)temp_32_1 / scaler);
      }
    }
    return;
  }

  if (fd->container == "int32")
  {
    auto * const lbuf = make_vla(u8, 4 * amount);
    fread_chk(lbuf, 4, amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[4 * i] << 24) | (lbuf[4 * i + 1] << 16) | (lbuf[4 * i + 2] << 8) | lbuf[4 * i + 3];
        buffer[i] = cf32(0, (f32)temp_32_1 / scaler);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        i32 temp_32_1 = (lbuf[4 * i + 3] << 24) | (lbuf[4 * i + 2] << 16) | (lbuf[4 * i + 1] << 8) | lbuf[4 * i];
        buffer[i] = cf32(0, (f32)temp_32_1 / scaler);
      }
    }
    return;
  }

  if (fd->container == "float32")
  {
    auto * const lbuf = make_vla(u8, 4 * amount);
    fread_chk(lbuf, 4, amount, theFile);
    if (fd->byteOrder == "MSB")
    {
      for (i32 i = 0; i < amount; i++)
      {
        UCnv c;
        c.int32Data = (lbuf[4 * i] << 24) | (lbuf[4 * i + 1] << 16) | (lbuf[4 * i + 2] << 8) | lbuf[4 * i + 3];
        buffer[i] = cf32(0, c.floatData);
      }
    }
    else
    {
      for (i32 i = 0; i < amount; i++)
      {
        UCnv c;
        c.int32Data = (lbuf[4 * i + 3] << 24) | (lbuf[4 * i + 2] << 16) | (lbuf[4 * i + 1] << 8) | lbuf[4 * i];
        buffer[i] = cf32(0, c.floatData);
      }
    }
    return;
  }
}
