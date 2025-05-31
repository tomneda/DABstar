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
#include        "faad-decoder.h"
#include        "neaacdec.h"
#include        "dabradio.h"

faadDecoder::faadDecoder(DabRadio * mr, RingBuffer<i16> * buffer)
{
  this->audioBuffer = buffer;
  aacCap = NeAACDecGetCapabilities();
  aacHandle = NeAACDecOpen();
  aacConf = NeAACDecGetCurrentConfiguration(aacHandle);
  aacInitialized = false;
  baudRate = 48000;
  connect(this, &faadDecoder::signal_new_audio, mr, &DabRadio::slot_new_audio);
}

faadDecoder::~faadDecoder()
{
  NeAACDecClose(aacHandle);
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

bool faadDecoder::initialize(const stream_parms * iSP)
{
  u64 sample_rate;
  u8 channels;
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

  i32 core_sr_index = iSP->dacRate ? (iSP->sbrFlag ? 6 : 3) : (iSP->sbrFlag ? 8 : 5);   // 24/48/16/32 kHz
  i32 core_ch_config = get_aac_channel_configuration(iSP->mpegSurround, iSP->aacChannelMode);
  if (core_ch_config == -1)
  {
    printf("Unrecognized mpeg surround config (ignored): %d\n", iSP->mpegSurround);
    return false;
  }

  u8 asc[2];
  asc[0] = 0b00010 << 3 | core_sr_index >> 1;
  asc[1] = (core_sr_index & 0x01) << 7 | core_ch_config << 3 | 0b100;
  const i32 init_result = NeAACDecInit2(aacHandle, asc, sizeof(asc), &sample_rate, &channels);
  if (init_result != 0)
  {
    /*      If some error initializing occured, skip the file */
    printf("Error initializing decoder library: %s\n", NeAACDecGetErrorMessage(-init_result));
    NeAACDecClose(aacHandle);
    return false;
  }
  return true;
}

i16 faadDecoder::convert_mp4_to_pcm(const stream_parms * const iSP, const u8 * const ipBuffer, const i16 iBufferLength)
{
  NeAACDecFrameInfo hInfo;

  if (!aacInitialized)
  {
    if (!initialize(iSP))
    {
      return 0;
    }
    aacInitialized = true;
  }

  const i16 * const outBuffer = (i16 *)NeAACDecDecode(aacHandle, &hInfo, const_cast<u8*>(ipBuffer), iBufferLength);
  const u64 sampleRate = hInfo.samplerate;

  const i16 samples = hInfo.samples;
  if ((sampleRate == 24000) || (sampleRate == 32000) || (sampleRate == 48000) || (sampleRate != (u64)baudRate))
  {
    baudRate = sampleRate;
  }

  //fprintf (stdout, "bytes consumed %d\n", (i32)(hInfo. bytesconsumed));
  //fprintf (stdout, "samplerate = %lu, samples = %d, channels = %d, error = %d, sbr = %d\n", sampleRate, samples, hInfo. channels, hInfo.error, hInfo.sbr);
  //fprintf (stdout, "header = %d\n", hInfo. header_type);

  const u8 channels = hInfo.channels;
  if (hInfo.error != 0)
  {
    fprintf(stderr, "Warning: %s\n", faacDecGetErrorMessage(hInfo.error));
    return -1;
  }

  if (channels == 2)
  {
    audioBuffer->put_data_into_ring_buffer(outBuffer, samples);
    if (audioBuffer->get_ring_buffer_read_available() > (i32)sampleRate / 10)
    {
      emit signal_new_audio((i32)sampleRate / 10, sampleRate,
                            (hInfo.ps ? DabRadio::AFL_PS_USED : DabRadio::AFL_NONE) |
                            (hInfo.sbr ? DabRadio::AFL_SBR_USED : DabRadio::AFL_NONE));
    }
  }
  else if (channels == 1) // TODO: this is not called with a mono service but the stereo above
  {
    auto * const buffer = make_vla(i16, 2 * samples);
    for (i16 i = 0; i < samples; i++)
    {
      buffer[2 * i + 0] = buffer[2 * i + 1] = outBuffer[i];
    }
    audioBuffer->put_data_into_ring_buffer(buffer, 2 * samples);
    if (audioBuffer->get_ring_buffer_read_available() > (i32)sampleRate / 10)
    {
      emit signal_new_audio((i32)sampleRate / 10, sampleRate,
                            (hInfo.ps ? DabRadio::AFL_PS_USED : DabRadio::AFL_NONE) |
                            (hInfo.sbr ? DabRadio::AFL_SBR_USED : DabRadio::AFL_NONE));
    }
  }
  else
  {
    fprintf(stderr, "Cannot handle these channels\n");
  }

  return channels;
}
