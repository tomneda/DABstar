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

#include  "wav-reader.h"
#include  "wavfiles.h"
#include  "device-exceptions.h"
#include  <sys/time.h>
#include  <cinttypes>

#define  BUFFERSIZE  32768

static inline i64 getMyTime()
{
  struct timeval tv;

  gettimeofday(&tv, nullptr);
  return ((i64)tv.tv_sec * 1000000 + (i64)tv.tv_usec);
}

WavReader::WavReader(WavFileHandler * mr, SNDFILE * filePointer, RingBuffer<cf32> * theBuffer)
{
  this->parent = mr;
  this->filePointer = filePointer;
  this->theBuffer = theBuffer;
  fileLength = sf_seek(filePointer, 0, SEEK_END);
  fprintf(stderr, "fileLength = %d\n", (i32)fileLength);
  sf_seek(filePointer, 0, SEEK_SET);
  period = (32768 * 1000) / (2048);  // full IQś read
  fprintf(stderr, "Period = %" PRIu64 "\n", period);
  running.store(false);
  continuous.store(mr->cbLoopFile->isChecked());
  start();
}

WavReader::~WavReader()
{
  stopReader();
}

void WavReader::stopReader()
{
  if (running.load())
  {
    running.store(false);
    while (isRunning())
    {
      usleep(200);
    }
  }
}

void WavReader::run()
{
  i32 bufferSize = 32768;
  i64 nextStop;
  i32 teller = 0;
  auto * const bi  = make_vla(cf32, bufferSize);

  connect(this, SIGNAL (setProgress(int, float)), parent, SLOT (setProgress(int, float)));
  sf_seek(filePointer, 0, SEEK_SET);

  running.store(true);

  nextStop = getMyTime();
  try
  {
    while (running.load())
    {
      while (theBuffer->get_ring_buffer_write_available() < bufferSize)
      {
        if (!running.load())
        {
          throw (33);
        }
        usleep(100);
      }

      if (++teller >= 20)
      {
        i32 xx = sf_seek(filePointer, 0, SEEK_CUR);
        f32 progress = (f32)xx / fileLength;
        setProgress((i32)(progress * 100), (f32)xx / 2048000);
        teller = 0;
      }

      nextStop += period;
      i32 n = sf_readf_float(filePointer, (f32 *)bi, bufferSize);
      if (n < bufferSize)
      {
        fprintf(stderr, "End of file reached\n");
        sf_seek(filePointer, 0, SEEK_SET);
        for (i32 i = n; i < bufferSize; i++)
        {
          bi[i] = std::complex<f32>(0, 0);
        }
        if (!continuous.load())
	      break;
      }
      theBuffer->put_data_into_ring_buffer(bi, bufferSize);
      if (nextStop - getMyTime() > 0)
      {
        usleep(nextStop - getMyTime());
      }
    }
  }
  catch (i32 e)
  {
  }
  fprintf(stderr, "taak voor replay eindigt hier\n");
  fflush(stderr);
}

bool WavReader::handle_continuousButton()
{
  continuous.store(!continuous.load());
  return continuous.load();
}

