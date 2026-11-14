/*
 * Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "etifiles.h"
#include "eti_reader.h"
#include "openfiledialog.h"
#include "setting_helper.h"
#include <stdexcept>

#ifndef _WIN32
  #include <unistd.h>
#endif

constexpr u32 cRingBufferSize = 65536; // holds 10 ETI frames

EtiFileHandler::EtiFileHandler(const QString& iFilename)
  : mFrame(nullptr)
  , mRingBuffer(cRingBufferSize)
{
  setupUi(&mFrame, "Play ETI File");
  mFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon

  mpFile = OpenFileDialog::open_file(iFilename, "rb");

  if (mpFile == nullptr)
  {
    const QString val = QString("Cannot open file '%1'").arg(iFilename);
    throw std::runtime_error(val.toUtf8().data());
  }

  lblTitle->setText("Playing pre-recorded ETI file");
  lblSampleRateText->setText("Frames:");
  lblFileName->setText(iFilename);
  fseek(mpFile, 0, SEEK_END);
  const i64 fileLength = ftell(mpFile);
  fseek(mpFile, 0, SEEK_SET);
  const i64 numFrames = fileLength / cEtiFrameSize;
  mFileDuration_s = (f32)numFrames * 0.024f;
  lblSampleRate->setText(QString::number(numFrames));
  lblFormat->setText("ETI-NI");

  Settings::FileReaderEti::posAndSize.read_widget_geometry(&mFrame);
}

EtiFileHandler::~EtiFileHandler()
{
  stopReader();

  if (mpFile != nullptr)
  {
    fclose(mpFile);
  }
  Settings::FileReaderEti::posAndSize.write_widget_geometry(&mFrame);
}

bool EtiFileHandler::restartReader(const i32 /*freq*/)
{
  if (mIsRunning.load())
  {
    return true;
  }
  mRingBuffer.flush_ring_buffer();
  mpEtiReader = std::make_unique<EtiReader>(this, mpFile, &mRingBuffer);
  mpEtiReader->set_paused(mPaused);
  mpEtiReader->start_reader();
  mIsRunning.store(true);
  return true;
}

void EtiFileHandler::stopReader()
{
  if (mIsRunning.load())
  {
    mpEtiReader->stop_reader();
    mpEtiReader.reset();
  }
  mIsRunning.store(false);
}

bool EtiFileHandler::get_eti_frame(TEtiFrame& oFrame, const i32 iTimeout_ms)
{
  i32 waited_ms = 0;

  while (mRingBuffer.get_ring_buffer_read_available() < cEtiFrameSize)
  {
    if (waited_ms >= iTimeout_ms)
    {
      return false;
    }
    usleep(1000);  // use minimum 1000us as Windows will ignore smaller values
    ++waited_ms;
  }

  return mRingBuffer.get_data_from_ring_buffer(oFrame.data(), cEtiFrameSize) == cEtiFrameSize;
}

void EtiFileHandler::show()
{
  mFrame.show();
}

void EtiFileHandler::hide()
{
  mFrame.hide();
}

bool EtiFileHandler::isHidden()
{
  return mFrame.isHidden();
}

void EtiFileHandler::resetBuffer()
{
  mRingBuffer.flush_ring_buffer();
}

void EtiFileHandler::jump_to_file_pos_per_mill(const i32 iPerMill)
{
  if (mpEtiReader != nullptr)
  {
    mpEtiReader->jump_to_relative_position_per_mill(iPerMill);
  }
}

void EtiFileHandler::set_file_paused(const bool iPaused)
{
  mPaused = iPaused;
  if (mpEtiReader != nullptr)
  {
    mpEtiReader->set_paused(iPaused);
  }
}
