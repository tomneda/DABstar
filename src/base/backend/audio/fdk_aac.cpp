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
 *  Use the fdk-aac library.
 */
#include "mp4processor.h"
#include "dabradio.h"
#include "fdk_aac.h"

/**
  * \class mp4Processor is the main handler for the aac frames
  * the class proper processes input and extracts the aac frames
  * that are processed by the "faadDecoder" class
  */
FdkAAC::FdkAAC(const DabRadio * const ipDR, RingBuffer<i16> * const ipBuffer)
  : mpAudioBuffer(ipBuffer)
{
  mAacHandle = aacDecoder_Open(TT_MP4_LOAS, 1);

  if (mAacHandle == nullptr)
  {
    return;
  }

  // Enable the decoder's built-in error concealment. On a lost frame we ask the decoder to
  // synthesise a substitute frame in the spectral domain (see conceal_lost_frame()), which
  // avoids the boundary clicks and frame-rate buzz of PCM-domain repetition.
  //   1 = noise substitution (delay-free, chosen here)
  //   2 = energy interpolation: sounds best but adds one frame of latency and supports only
  //       some AOTs (Audio Object Type), so it would require reworking the good-frame path - not used.
  aacDecoder_SetParam(mAacHandle, AAC_CONCEAL_METHOD, 1);

  connect(this, &FdkAAC::signal_new_audio, ipDR, &DabRadio::slot_new_audio);

  mIsWorking = true;
}

FdkAAC::~FdkAAC()
{
  if (mIsWorking)
  {
    aacDecoder_Close(mAacHandle);
  }
}

i16 FdkAAC::convert_mp4_to_pcm(const SStreamParms * const iSP, const u8 * const ipBuffer, const i16 iPacketLength)
{
  static_assert(sizeof(i16) == sizeof(INT_PCM));
  INT_PCM decode_buf[8 * sizeof(INT_PCM) * 2048];
  INT_PCM * bufp = &decode_buf[0];
  constexpr i32 cOutputSize = 8 * 2048;

  if (!mIsWorking)
  {
    return -1;
  }

  if ((ipBuffer[0] != 0x56) || ((ipBuffer[1] >> 5) != 7))
  {
    return -2;
  }

  const u32 packet_size = (((ipBuffer[1] & 0x1F) << 8) | ipBuffer[2]) + 3;

  if ((signed)packet_size != iPacketLength)
  {
    return -3;
  }

  u32 validBytes = packet_size; // to remove const-ness
  AAC_DECODER_ERROR err = aacDecoder_Fill(mAacHandle, const_cast<u8 **>(&ipBuffer), &packet_size, &validBytes);

  if (err != AAC_DEC_OK)
  {
    return -4;
  }

  err = aacDecoder_DecodeFrame(mAacHandle, bufp, cOutputSize, 0);

  if (err == AAC_DEC_NOT_ENOUGH_BITS)
  {
    return -5;
  }

  if (err != AAC_DEC_OK)
  {
    return -6;
  }

  const CStreamInfo * info = aacDecoder_GetStreamInfo(mAacHandle);
  if (!info || info->sampleRate <= 0)
  {
    return -7;
  }

  const i32 audioBufferFillSize = info->sampleRate / 8; // 6000S@48000Sps -> 125ms
  const i32 stereoFrameSize = info->frameSize * 2;
  const u32 audioFlags = (iSP->psFlag  ? DabRadio::AFL_PS_USED  : DabRadio::AFL_NONE) |
                         (iSP->sbrFlag ? DabRadio::AFL_SBR_USED : DabRadio::AFL_NONE);

  if (info->numChannels == 2)
  {
    mpAudioBuffer->put_data_into_ring_buffer(bufp, stereoFrameSize);

    if (mpAudioBuffer->get_ring_buffer_read_available() > audioBufferFillSize)
    {
      emit signal_new_audio(audioBufferFillSize, info->sampleRate, audioFlags);
    }
  }
  else if (info->numChannels == 1)
  {
    auto * const buffer = make_vla(i16, stereoFrameSize);

    for (i32 i = 0; i < info->frameSize; i++)
    {
      buffer[2 * i + 0] = buffer[2 * i + 1] = bufp[i];
    }

    mpAudioBuffer->put_data_into_ring_buffer(buffer, stereoFrameSize);

    if (mpAudioBuffer->get_ring_buffer_read_available() > audioBufferFillSize)
    {
      emit signal_new_audio(audioBufferFillSize, info->sampleRate, audioFlags);
    }
  }
  else
  {
    fprintf(stderr, "Cannot handle these channels\n");
  }

  mLastAudioBufferFillSize = audioBufferFillSize;
  mLastSampleRate = info->sampleRate;
  mLastAudioFlags = audioFlags;
  return info->numChannels;
}

void FdkAAC::conceal_lost_frame(const i32 iNumSamples)
{
  // Packet-loss concealment via the decoder's built-in concealment module (configured in
  // the constructor). Calling aacDecoder_DecodeFrame() with AACDEC_CONCEAL and no new input
  // makes the decoder synthesise one substitute frame from its own history in the spectral
  // domain, with correct windowing/overlap-add. This has no boundary clicks and no
  // frame-rate buzz, and it keeps the decoder's overlap buffers consistent so the return to
  // good audio is seamless - all clearly better than repeating the last PCM frame.
  if (mIsWorking && mLastSampleRate > 0)
  {
    static_assert(sizeof(i16) == sizeof(INT_PCM));
    std::array<INT_PCM, 8 * 2048> decode_buf;
    constexpr i32 cOutputSize = 8 * 2048;

    const AAC_DECODER_ERROR err = aacDecoder_DecodeFrame(mAacHandle, decode_buf.data(), cOutputSize, AACDEC_CONCEAL);

    if (err == AAC_DEC_OK)
    {
      const CStreamInfo * const info = aacDecoder_GetStreamInfo(mAacHandle);

      if (info != nullptr && info->sampleRate > 0 && info->frameSize > 0)
      {
        const i32 stereoFrameSize = info->frameSize * 2;

        if (info->numChannels == 2)
        {
          mpAudioBuffer->put_data_into_ring_buffer(decode_buf.data(), stereoFrameSize);
        }
        else if (info->numChannels == 1)
        {
          auto * const buffer = make_vla(i16, stereoFrameSize);

          for (i32 i = 0; i < info->frameSize; ++i)
          {
            buffer[2 * i + 0] = buffer[2 * i + 1] = decode_buf[i];
          }

          mpAudioBuffer->put_data_into_ring_buffer(buffer, stereoFrameSize);
        }

        if (mpAudioBuffer->get_ring_buffer_read_available() > mLastAudioBufferFillSize)
        {
          emit signal_new_audio(mLastAudioBufferFillSize, info->sampleRate, mLastAudioFlags);
        }
        return;
      }
    }
  }

  // Fallback: no usable decoder state yet (e.g. loss right after tuning) or the concealment
  // decode failed -> emit plain silence to keep the audio timeline continuous.
  const std::vector<i16> silence(iNumSamples, 0);
  mpAudioBuffer->put_data_into_ring_buffer(silence.data(), iNumSamples);

  if (mLastSampleRate > 0 && mpAudioBuffer->get_ring_buffer_read_available() > mLastAudioBufferFillSize)
  {
    emit signal_new_audio(mLastAudioBufferFillSize, mLastSampleRate, mLastAudioFlags);
  }
}

