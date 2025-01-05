/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2020
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
 *	Use the fdk-aac library.
 */
#include  "mp4processor.h"
#include  "radio.h"
#include  <cstring>
#include  "charsets.h"
#include  "fdk-aac.h"

/**
  *	\class mp4Processor is the main handler for the aac frames
  *	the class proper processes input and extracts the aac frames
  *	that are processed by the "faadDecoder" class
  */
FdkAAC::FdkAAC(RadioInterface * mr, RingBuffer<int16_t> * iipBuffer)
  : mpAudioBuffer(iipBuffer)
{
  handle = aacDecoder_Open(TT_MP4_LOAS, 1);

  if (handle == nullptr)
  {
    return;
  }

  connect(this, &FdkAAC::signal_new_audio, mr, &RadioInterface::slot_new_audio);

  mIsWorking = true;
}

FdkAAC::~FdkAAC()
{
  if (mIsWorking)
  {
    aacDecoder_Close(handle);
  }
}

int16_t FdkAAC::convert_mp4_to_pcm(const stream_parms * iSP, const uint8_t * const ipBuffer, const int16_t iPacketLength)
{
  static_assert(sizeof(int16_t) == sizeof(INT_PCM));
  uint32_t packet_size;
  uint32_t valid;
  AAC_DECODER_ERROR err;
  INT_PCM decode_buf[8 * sizeof(INT_PCM) * 2048];
  INT_PCM * bufp = &decode_buf[0];
  int output_size = 8 * 2048;

  if (!mIsWorking)
  {
    return -1;
  }

  if ((ipBuffer[0] != 0x56) || ((ipBuffer[1] >> 5) != 7))
  {
    return -2;
  }

  packet_size = ((ipBuffer[1] & 0x1F) << 8) | (ipBuffer[2] + 3);
  if ((signed)packet_size != iPacketLength)
  {
    return -3;
  }

  valid = packet_size;
  err = aacDecoder_Fill(handle, const_cast<uint8_t **>(&ipBuffer), &packet_size, &valid);
  if (err != AAC_DEC_OK)
  {
    return -4;
  }

  err = aacDecoder_DecodeFrame(handle, bufp, output_size, 0);
  if (err == AAC_DEC_NOT_ENOUGH_BITS)
  {
    return -5;
  }

  if (err != AAC_DEC_OK)
  {
    return -6;
  }

  CStreamInfo * info = aacDecoder_GetStreamInfo(handle);
  if (!info || info->sampleRate <= 0)
  {
    return -7;
  }

  if (info->numChannels == 2)
  {
    mpAudioBuffer->put_data_into_ring_buffer(bufp, info->frameSize * 2);
    if (mpAudioBuffer->get_ring_buffer_read_available() > (int)info->sampleRate / 8)
    {
      emit signal_new_audio(info->frameSize, info->sampleRate,  (iSP->psFlag ? RadioInterface::AFL_PS_USED : 0) | (iSP->sbrFlag ? RadioInterface::AFL_SBR_USED : 0));
    }
  }
  else if (info->numChannels == 1)
  {
    auto * const buffer = make_vla(int16_t, 2 * info->frameSize);

    for (int32_t i = 0; i < info->frameSize; i++)
    {
      buffer[2 * i + 0] = buffer[2 * i + 1] = bufp[i];
    }
    mpAudioBuffer->put_data_into_ring_buffer(buffer, info->frameSize * 2);
    if (mpAudioBuffer->get_ring_buffer_read_available() > info->sampleRate / 8)
    {
      emit signal_new_audio(info->frameSize, info->sampleRate, (iSP->psFlag ? RadioInterface::AFL_PS_USED : 0) | (iSP->sbrFlag ? RadioInterface::AFL_SBR_USED : 0));
    }
  }
  else
  {
    fprintf(stderr, "Cannot handle these channels\n");
  }

  return info->numChannels;
}

