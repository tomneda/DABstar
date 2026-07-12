/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013 .. 2019
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
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
#include "faad_decoder.h"
#include "neaacdec.h"
#include "dabradio.h"

FaadDecoder::FaadDecoder(const DabRadio * const ipDR, RingBuffer<i16> * ipBuffer)
{
  mpAudioBuffer = ipBuffer;
  mAacCap = NeAACDecGetCapabilities();
  mAacHandle = NeAACDecOpen();
  mAacConf = NeAACDecGetCurrentConfiguration(mAacHandle);

  connect(this, &FaadDecoder::signal_new_audio, ipDR, &DabRadio::slot_new_audio);
}

FaadDecoder::~FaadDecoder()
{
  NeAACDecClose(mAacHandle);
}

i32 get_aac_channel_configuration(i16 m_mpeg_surround_config, u8 aacChannelMode)
{

  switch (m_mpeg_surround_config)
  {
  case 0:     // no surround
    return aacChannelMode ? 2 : 1;
  case 1:     // 5.1
    return 6;
  case 2:     // 7.1
    return 7;
  default:return -1;
  }
}

bool FaadDecoder::_initialize(const SStreamParms * iSP)
{
  /* AudioSpecificConfig structure (the only way to select 960 transform here!)
   *
   *  00010 = AudioObjectType 2 (AAC LC)
   *  xxxx  = (core) sample rate index
   *  xxxx  = (core) channel config
   *  100   = GASpecificConfig with 960 transform
   *
   * SBR: implicit signaling sufficient - libfaad2
   * automatically assumes SBR on sample rates <= 24 kHz
   * => explicit signaling works, too, but is not necessary here
   *
   * PS:  implicit signaling sufficient - libfaad2
   * therefore always uses stereo output (if PS support was enabled)
   * => explicit signaling not possible, as libfaad2 does not
   * support AudioObjectType 29 (PS)
   */

  const i32 core_sr_index = iSP->dacRate ? (iSP->sbrFlag ? 6 : 3) : (iSP->sbrFlag ? 8 : 5);   // 24/48/16/32 kHz
  const i32 core_ch_config = get_aac_channel_configuration(iSP->mpegSurround, iSP->aacChannelMode);

  if (core_ch_config == -1)
  {
    printf("Unrecognized mpeg surround config (ignored): %d\n", iSP->mpegSurround);
    return false;
  }

  u8 asc[2];
  asc[0] = 0b00010 << 3 | core_sr_index >> 1;
  asc[1] = (core_sr_index & 0x01) << 7 | core_ch_config << 3 | 0b100;
  long unsigned int sample_rate;  // keep this as Type "long" seems to be 32bit on Win, but 64bit on Linux
  u8 channels;
  const i32 init_result = NeAACDecInit2(mAacHandle, asc, sizeof(asc), &sample_rate, &channels);

  if (init_result != 0)
  {
    /*      If some error initializing occured, skip the file */
    printf("Error initializing decoder library: %s\n", NeAACDecGetErrorMessage(-init_result));
    NeAACDecClose(mAacHandle);
    return false;
  }
  return true;
}

i16 FaadDecoder::convert_mp4_to_pcm(const SStreamParms * const iSP, const u8 * const ipBuffer, const i16 iBufferLength)
{
  if (!mAacInitialized)
  {
    if (!_initialize(iSP))
    {
      return 0;
    }
    mAacInitialized = true;
  }

  NeAACDecFrameInfo aacInfo;
  const i16 * const outBuffer = (i16 *)NeAACDecDecode(mAacHandle, &aacInfo, const_cast<u8*>(ipBuffer), iBufferLength);
  const u32 sampleRate = aacInfo.samplerate;
  const i16 samples = aacInfo.samples;
  const i32 audioBufferFillSize = sampleRate / 8; // 125ms

  if (aacInfo.error != 0)
  {
    qWarning() << "faacDecGetErrorMessage: " << faacDecGetErrorMessage(aacInfo.error);
    return -1;
  }

  const u32 audioFlags = (aacInfo.ps  ? DabRadio::AFL_PS_USED  : DabRadio::AFL_NONE) |
                         (aacInfo.sbr ? DabRadio::AFL_SBR_USED : DabRadio::AFL_NONE);

  if (aacInfo.channels == 2)
  {
    mpAudioBuffer->put_data_into_ring_buffer(outBuffer, samples);
    mLastGoodFrame.assign(outBuffer, outBuffer + samples);

    if (mpAudioBuffer->get_ring_buffer_read_available() > audioBufferFillSize) // 125ms
    {
      emit signal_new_audio(audioBufferFillSize, sampleRate, audioFlags);
    }
  }
  else if (aacInfo.channels == 1) // TODO: this is not called with a mono service but the stereo above
  {
    const i32 stereoSamples = 2 * samples; // we nevertheless send "stereo" samples
    auto * const buffer = make_vla(i16, stereoSamples);

    for (i16 i = 0; i < samples; ++i)
    {
      buffer[2 * i + 0] = buffer[2 * i + 1] = outBuffer[i];
    }

    mpAudioBuffer->put_data_into_ring_buffer(buffer, stereoSamples);
    mLastGoodFrame.assign(buffer, buffer + stereoSamples);

    if (mpAudioBuffer->get_ring_buffer_read_available() > audioBufferFillSize)
    {
      emit signal_new_audio(audioBufferFillSize, sampleRate, audioFlags);
    }
  }
  else
  {
    fprintf(stderr, "Cannot handle these channels\n");
  }

  mConcealDecay = 1.0f;
  mLastAudioBufferFillSize = audioBufferFillSize;
  mLastSampleRate = sampleRate;
  mLastAudioFlags = audioFlags;
  return aacInfo.channels;
}

void FaadDecoder::conceal_lost_frame(const i32 iNumSamples)
{
  // Packet-loss concealment: repeat the last good frame with exponential amplitude decay.
  // Each successive call attenuates by cConcealDecayFactor, fading to silence over ~200 ms
  // (10 frames × 20 ms/frame at 48 kHz core, or ~400 ms with SBR).
  // Falls back to true silence when no good frame has been stored yet.
  if ((i32)mLastGoodFrame.size() != iNumSamples)
  {
    const std::vector<i16> silence(iNumSamples, 0);
    mpAudioBuffer->put_data_into_ring_buffer(silence.data(), iNumSamples);
    return;
  }

  std::vector<i16> concealed(iNumSamples);
  for (i32 i = 0; i < iNumSamples; ++i)
  {
    concealed[i] = (i16)((f32)mLastGoodFrame[i] * mConcealDecay);
  }
  mConcealDecay *= cConcealDecayFactor;

  mpAudioBuffer->put_data_into_ring_buffer(concealed.data(), iNumSamples);

  // Trigger the audio pipeline the same way convert_mp4_to_pcm() does, so the concealed
  // samples are not stranded in the decoder ring buffer during a sustained error run.
  if (mLastSampleRate > 0 && mpAudioBuffer->get_ring_buffer_read_available() > mLastAudioBufferFillSize)
  {
    emit signal_new_audio(mLastAudioBufferFillSize, mLastSampleRate, mLastAudioFlags);
  }
}
