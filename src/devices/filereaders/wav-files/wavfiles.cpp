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
#include  "wavfiles.h"
#include  "openfiledialog.h"
#include  <cstdio>
#include  <cstdlib>
#include  <fcntl.h>
#include  <ctime>
#include  <QString>

#ifdef _WIN32
#else

  #include  <unistd.h>
  #include  <sys/time.h>

#endif


#define  __BUFFERSIZE__  8 * 32768

WavFileHandler::WavFileHandler(const QString & iFilename)
  : myFrame(nullptr)
  , _I_Buffer(__BUFFERSIZE__)
{
  SF_INFO * sf_info;

  fileName = iFilename;
  setupUi(&myFrame);
  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  myFrame.show();

  sf_info = (SF_INFO *)alloca (sizeof(SF_INFO));
  sf_info->format = 0;
  filePointer = OpenFileDialog::open_snd_file(iFilename.toUtf8().data(), SFM_READ, sf_info);
  if (filePointer == nullptr)
  {
    const QString val = QString("File '%1' is no valid sound file").arg(iFilename);
    throw std::runtime_error(val.toUtf8().data());
  }
  if ((sf_info->samplerate != 2048000) || (sf_info->channels != 2))
  {
    sf_close(filePointer);
    const QString val = QString("Sample rate or channel number does not fit");
    throw std::runtime_error(val.toUtf8().data());
  }
  nameofFile->setText(iFilename);
  fileProgress->setValue(0);
  currentTime->display(0);
  i64 fileLength = sf_seek(filePointer, 0, SEEK_END);
  totalTime->display(QString("%1").arg((f32)fileLength / 2048000, 0, 'f', 1));

  connect(cbLoopFile, &QCheckBox::clicked, this, &WavFileHandler::slot_handle_cb_loop_file);
  running.store(false);
}
//
//	Note that running == true <==> readerTask has value assigned

WavFileHandler::~WavFileHandler()
{
  if (running.load())
  {
    readerTask->stopReader();
    while (readerTask->isRunning())
    {
      usleep(500);
    }
    delete readerTask;
  }
  if (filePointer != nullptr)
  {
    sf_close(filePointer);
  }
}

bool WavFileHandler::restartReader(i32 freq)
{
  (void)freq;
  if (running.load())
  {
    return true;
  }
  readerTask = new WavReader(this, filePointer, &_I_Buffer);
  running.store(true);
  return true;
}

void WavFileHandler::stopReader()
{
  if (running.load())
  {
    readerTask->stopReader();
    while (readerTask->isRunning())
    {
      usleep(100);
    }
    delete readerTask;
  }
  running.store(false);
}

//	size is in I/Q pairs
i32 WavFileHandler::getSamples(cf32 * V, i32 size)
{
  i32 amount;

  if (filePointer == nullptr)
  {
    return 0;
  }

  while (_I_Buffer.get_ring_buffer_read_available() < size)
  {
    usleep(100);
  }

  amount = _I_Buffer.get_data_from_ring_buffer(V, size);

  return amount;
}

i32 WavFileHandler::Samples()
{
  return _I_Buffer.get_ring_buffer_read_available();
}

void WavFileHandler::setProgress(int progress, f32 timelength)
{
  fileProgress->setValue(progress);
  currentTime->display(QString("%1").arg(timelength, 0, 'f', 1));
}

void WavFileHandler::show()
{
  myFrame.show();
}

void WavFileHandler::hide()
{
  myFrame.hide();
}

bool WavFileHandler::isHidden()
{
  return myFrame.isHidden();
}

bool WavFileHandler::isFileInput()
{
  return true;
}

void WavFileHandler::setVFOFrequency(i32)
{
}

int WavFileHandler::getVFOFrequency()
{
  return 0;
}

void WavFileHandler::resetBuffer()
{
}

i16 WavFileHandler::bitDepth()
{
  return 10; // TODO: taken from former default interface, is it correct?
}

QString WavFileHandler::deviceName()
{
  return "WavFile";
}

void WavFileHandler::slot_handle_cb_loop_file(const bool iChecked)
{
  if (filePointer == nullptr)
  {
    return;
  }

  cbLoopFile->setChecked(readerTask->handle_continuousButton());
}
