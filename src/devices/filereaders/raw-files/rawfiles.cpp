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
 *
 * 	File reader:
 *	For the (former) files with 8 bit raw data from the
 *	dabsticks
 */
#include  "rawfiles.h"
#include  "raw-reader.h"
#include  "openfiledialog.h"
#include  "setting-helper.h"
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

constexpr u32 cInputFrameBufferSize = 8 * 32768;

RawFileHandler::RawFileHandler(const QString & iFilename)
  : mFrame(nullptr)
  , mRingBuffer(cInputFrameBufferSize)
{
  mFileName = iFilename;

  setupUi(&mFrame);

  Settings::FileReaderRaw::posAndSize.read_widget_geometry(&mFrame);

  mFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  mFrame.show();

  mpFile = OpenFileDialog::open_file(iFilename, "rb");

  if (mpFile == nullptr)
  {
    const QString val = QString("Cannot open file '%1'").arg(iFilename);
    throw std::runtime_error(val.toUtf8().data());
  }

  lcdSampleRate->display(INPUT_RATE);
  lblFileName->setText(iFilename);
  sliderFilePos->setValue(0);
  lcdCurrTime->display(0);
  fseek(mpFile, 0, SEEK_END);
  const i64 fileLength = ftell(mpFile); // 2 * 1Byte per sample
  fseek(mpFile, 0, SEEK_SET);
  lcdTotalTime->display(QString("%1").arg((f32)fileLength / (INPUT_RATE * 2), 0, 'f', 1));
  lblFormat->setText("u8");

  connect(cbLoopFile, &QCheckBox::clicked, this, &RawFileHandler::slot_handle_cb_loop_file);
  connect(sliderFilePos, &QSlider::sliderPressed, this, &RawFileHandler::slot_slider_pressed);
  connect(sliderFilePos, &QSlider::sliderReleased, this, &RawFileHandler::slot_slider_released);
  connect(sliderFilePos, &QSlider::sliderMoved, this, &RawFileHandler::slot_slider_moved);

  mIsRunning.store(false);
}

RawFileHandler::~RawFileHandler()
{
  if (mIsRunning.load())
  {
    mpRawReader->stop_reader();
    while (mpRawReader->isRunning())
    {
      usleep(100);
    }
  }
  if (mpFile != nullptr)
  {
    fclose(mpFile);
  }
  Settings::FileReaderRaw::posAndSize.write_widget_geometry(&mFrame);
}

bool RawFileHandler::restartReader(const i32 freq)
{
  (void)freq;
  if (mIsRunning.load())
  {
    return true;
  }
  mpRawReader = std::make_unique<RawReader>(this, mpFile, &mRingBuffer);
  mpRawReader->start_reader();
  mIsRunning.store(true);
  return true;
}

void RawFileHandler::stopReader()
{
  if (mIsRunning.load())
  {
    mpRawReader->stop_reader();
    while (mpRawReader->isRunning())
    {
      usleep(100);
    }
    mpRawReader.reset();
  }
  mIsRunning.store(false);
}

//	size is in I/Q pairs, file contains 8 bits values
i32 RawFileHandler::getSamples(cf32 * V, const i32 size)
{
  if (mpFile == nullptr)
  {
    return 0;
  }

  while ((i32)(mRingBuffer.get_ring_buffer_read_available()) < size)
  {
    usleep(100);
  }

  return mRingBuffer.get_data_from_ring_buffer(V, size);
}

i32 RawFileHandler::Samples()
{
  return mRingBuffer.get_ring_buffer_read_available();
}

void RawFileHandler::show()
{
  mFrame.show();
}

void RawFileHandler::hide()
{
  mFrame.hide();
}

bool RawFileHandler::isHidden()
{
  return mFrame.isHidden();
}

bool RawFileHandler::isFileInput()
{
  return true;
}

void RawFileHandler::setVFOFrequency(i32)
{
}

i32 RawFileHandler::getVFOFrequency()
{
  return 0;
}

void RawFileHandler::resetBuffer()
{
}

i16 RawFileHandler::bitDepth()
{
  return 10; // TODO: taken from former default interface, is it correct?
}

QString RawFileHandler::deviceName()
{
  return "RawFile";
}

void RawFileHandler::slot_handle_cb_loop_file(const bool iChecked)
{
  (void)iChecked;
  if (mpFile == nullptr)
  {
    return;
  }

  cbLoopFile->setChecked(mpRawReader->handle_continuous_button());
}

void RawFileHandler::slot_set_progress(const i32 progress, const f32 timelength)
{
  if (mSliderMovementPos < 0) // suppress slider update while mouse move on slider
  {
    sliderFilePos->setValue(progress);
  }
  lcdCurrTime->display(QString("%1").arg(timelength, 0, 'f', 1));
}

void RawFileHandler::slot_slider_pressed()
{
  mSliderMovementPos = sliderFilePos->value();
}

void RawFileHandler::slot_slider_released()
{
  if (mSliderMovementPos >= 0)
  {
    // mpRawReader->jump_to_relative_position_per_mill(mSliderMovementPos); // restart from current mouse release position
    mSliderMovementPos = -1;
  }
}

void RawFileHandler::slot_slider_moved(const i32 iPos)
{
  mSliderMovementPos = iPos; // iPos = [0; 1000]
  mpRawReader->jump_to_relative_position_per_mill(iPos);
}

