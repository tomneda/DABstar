/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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

#include  <sys/time.h>
#include  <cinttypes>
#include  "raw-reader.h"
#include  "rawfiles.h"

#ifdef _WIN32
#else
  #include  <unistd.h>
#endif

#define  BUFFERSIZE  32768

static inline int64_t getMyTime()
{
  struct timeval tv;

  gettimeofday(&tv, nullptr);
  return ((int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec);
}

rawReader::rawReader(RawFileHandler * mr, FILE * filePointer, RingBuffer<cmplx> * _I_Buffer)
{
  this->parent = mr;
  this->filePointer = filePointer;
  this->_I_Buffer = _I_Buffer;

  for(int i = 0; i < 256; i++)
  {
    mapTable[i] = ((float)i - 127.38f) / 128.0f;
  }

  fseek(filePointer, 0, SEEK_END);
  fileLength = ftell(filePointer);
  fprintf(stderr, "fileLength = %d\n", (int)fileLength);
  fseek(filePointer, 0, SEEK_SET);
  period = (32768 * 1000) / (2 * 2048);  // full IQś read
  fprintf(stderr, "Period = %" PRIu64 "\n", period);
  bi = new uint8_t[BUFFERSIZE];
  running.store(false);
  start();
}

rawReader::~rawReader()
{
  stopReader();
  delete[] bi;
}

void rawReader::stopReader()
{
  if (running)
  {
    running = false;
    while (isRunning())
    {
      usleep(200);
    }
  }
}

void rawReader::run()
{
  int64_t nextStop;
  int i;
  int teller = 0;
  cmplx localBuffer[BUFFERSIZE / 2];

  connect(this, SIGNAL (setProgress(int, float)), parent, SLOT   (setProgress(int, float)));
  fseek(filePointer, 0, SEEK_SET);
  running.store(true);
  nextStop = getMyTime();
  try
  {
    while (running.load())
    {
      while (_I_Buffer->get_ring_buffer_write_available() < BUFFERSIZE + 10)
      {
        if (!running.load())
        {
          throw (32);
        }
        usleep(100);
      }

      if (++teller >= 20)
      {
        int xx = ftell(filePointer);
        float progress = (float)xx / fileLength;
        setProgress((int)(progress * 100), (float)xx / (2 * 2048000));
        teller = 0;
      }

      nextStop += period;
      int n = fread(bi, sizeof(uint8_t), BUFFERSIZE, filePointer);
      if (n < BUFFERSIZE)
      {
        fprintf(stderr, "eof gehad\n");
        fseek(filePointer, 0, SEEK_SET);
        for (i = n; i < BUFFERSIZE; i++)
        {
          bi[i] = 0;
        }
      }

      for (i = 0; i < BUFFERSIZE / 2; i++)
      {
        localBuffer[i] = cmplx(4 * mapTable[bi[2 * i]], 4 * mapTable[bi[2 * i + 1]]);
      }
      _I_Buffer->put_data_into_ring_buffer(localBuffer, BUFFERSIZE / 2);
      if (nextStop - getMyTime() > 0)
      {
        usleep(nextStop - getMyTime());
      }
    }
  }
  catch (int e)
  {
  }
  fprintf(stderr, "taak voor replay eindigt hier\n");
}


