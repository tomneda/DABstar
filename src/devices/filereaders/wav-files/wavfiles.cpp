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


constexpr i32 cBufferSize = 8 * 32768;

WavFileHandler::WavFileHandler(const QString & iFilename)
  : mFrame(nullptr)
  , mRingBuffer(cBufferSize)
{
  mFileName = iFilename;
  setupUi(&mFrame);

  Settings::FileReaderWav::posAndSize.read_widget_geometry(&mFrame);

  mFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  mFrame.show();

  SF_INFO sf_info;
  sf_info.format = 0;
  mpFile = OpenFileDialog::open_snd_file(iFilename.toUtf8().data(), SFM_READ, &sf_info);

  if (mpFile == nullptr)
  {
    const QString val = QString("File '%1' is no valid sound file").arg(iFilename);
    throw std::runtime_error(val.toUtf8().data());
  }

  mSampleRate = sf_info.samplerate;
  if (mSampleRate < 1'536'000 || mSampleRate > 2'500'000 || sf_info.channels != 2) // SR 1536/1792/2000/2048/2500 Sps are tested (even 1536 kSps worked somehow fine)
  {
    sf_close(mpFile);
    QString val = QString("Sample rate %1 (SR=[1536..2500]) kSps or channel number %2 (ChNo=2) is not supported.").arg(mSampleRate/1000).arg(sf_info.channels);
    if (mSampleRate <= 192000) val += "\n\nCould this be an audio WAV file?";
    throw std::runtime_error(val.toUtf8().data());
  }

  switch(sf_info.format & 0xffff)
  {
    case SF_FORMAT_PCM_S8:
      lblFormat->setText("i8"); break;
    case SF_FORMAT_PCM_16:
      lblFormat->setText("i16"); break;
    case SF_FORMAT_PCM_24:
      lblFormat->setText("i24"); break;
    case SF_FORMAT_PCM_32:
      lblFormat->setText("i32"); break;
    case SF_FORMAT_PCM_U8:
      lblFormat->setText("u8"); break;
    case SF_FORMAT_FLOAT:
      lblFormat->setText("f32"); break;
    default:
      sf_close(mpFile);
      const QString val = QString("Format %x is not supported").arg(sf_info.format & 0xffff);
      throw std::runtime_error(val.toUtf8().data());
  }

  lcdSampleRate->display(mSampleRate);
  lblFileName->setText(iFilename);
  sliderFilePos->setValue(0);
  lcdCurrTime->display(0);
  const i64 fileLength = sf_seek(mpFile, 0, SEEK_END);
  sf_seek(mpFile, 0, SEEK_SET);
  lcdTotalTime->display(QString("%1").arg((f32)fileLength / (f32)mSampleRate, 0, 'f', 1));

  connect(cbLoopFile, &QCheckBox::clicked, this, &WavFileHandler::slot_handle_cb_loop_file);
  connect(sliderFilePos, &QSlider::sliderPressed, this, &WavFileHandler::slot_slider_pressed);
  connect(sliderFilePos, &QSlider::sliderReleased, this, &WavFileHandler::slot_slider_released);
  connect(sliderFilePos, &QSlider::sliderMoved, this, &WavFileHandler::slot_slider_moved);

  mIsRunning.store(false);
}

WavFileHandler::~WavFileHandler()
{
  if (mIsRunning.load())
  {
    mpWavReader->stop_reader();
    while (mpWavReader->isRunning())
    {
      usleep(500);
    }
  }
  if (mpFile != nullptr)
  {
    sf_close(mpFile);
  }
  Settings::FileReaderWav::posAndSize.write_widget_geometry(&mFrame);
}

bool WavFileHandler::restartReader(const i32 freq)
{
  (void)freq;
  if (mIsRunning.load())
  {
    return true;
  }
  mpWavReader = std::make_unique<WavReader>(this, mpFile, &mRingBuffer, mSampleRate);
  mpWavReader->start_reader();
  mIsRunning.store(true);
  return true;
}

void WavFileHandler::stopReader()
{
  if (mIsRunning.load())
  {
    mpWavReader->stop_reader();
    while (mpWavReader->isRunning())
    {
      usleep(100);
    }
    mpWavReader.reset();
  }
  mIsRunning.store(false);
}

//	size is in I/Q pairs
i32 WavFileHandler::getSamples(cf32 * V, const i32 size)
{
  if (mpFile == nullptr)
  {
    return 0;
  }

  while (mRingBuffer.get_ring_buffer_read_available() < size)
  {
    usleep(100);
  }

  return mRingBuffer.get_data_from_ring_buffer(V, size);
}

i32 WavFileHandler::Samples()
{
  return mRingBuffer.get_ring_buffer_read_available();
}

void WavFileHandler::show()
{
  mFrame.show();
}

void WavFileHandler::hide()
{
  mFrame.hide();
}

bool WavFileHandler::isHidden()
{
  return mFrame.isHidden();
}

bool WavFileHandler::isFileInput()
{
  return true;
}

void WavFileHandler::setVFOFrequency(i32)
{
}

i32 WavFileHandler::getVFOFrequency()
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
  (void)iChecked;
  if (mpFile == nullptr)
  {
    return;
  }

  cbLoopFile->setChecked(mpWavReader->handle_continuous_button());
}

void WavFileHandler::slot_set_progress(const i32 progress, const f32 timelength) const
{
  if (mSliderMovementPos < 0) // suppress slider update while mouse move on slider
  {
    sliderFilePos->setValue(progress);
  }
  lcdCurrTime->display(QString("%1").arg(timelength, 0, 'f', 1));
}

void WavFileHandler::slot_slider_pressed()
{
  mSliderMovementPos = sliderFilePos->value();
}

void WavFileHandler::slot_slider_released()
{
  if (mSliderMovementPos >= 0)
  {
    // mpWavReader->jump_to_relative_position_per_mill(mSliderMovementPos); // restart from current mouse release position
    mSliderMovementPos = -1;
  }
}

void WavFileHandler::slot_slider_moved(const i32 iPos)
{
  mSliderMovementPos = iPos; // iPos = [0; 1000]
  mpWavReader->jump_to_relative_position_per_mill(iPos);
}
