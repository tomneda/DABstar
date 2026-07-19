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
#include <algorithm>
#include <cmath>

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
    // _apply_exit_crossfade() extrapolates the concealment from the *previous* good frame,
    // still held in mLastGoodFrame, so it must run before that buffer is overwritten below.
    if (mConcealActive)
    {
      std::vector<i16> pcm(outBuffer, outBuffer + samples);
      _apply_exit_crossfade(pcm.data(), samples);
      mpAudioBuffer->put_data_into_ring_buffer(pcm.data(), samples);
    }
    else
    {
      mpAudioBuffer->put_data_into_ring_buffer(outBuffer, samples);
    }
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

    if (mConcealActive)
    {
      _apply_exit_crossfade(buffer, stereoSamples);
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

  // A good frame ended any concealment run.
  mConcealActive = false;
  mConcealPhase = 0;
  mConcealDecay = 1.0f;
  mLastAudioBufferFillSize = audioBufferFillSize;
  mLastSampleRate = sampleRate;
  mLastAudioFlags = audioFlags;
  return aacInfo.channels;
}

i32 FaadDecoder::_estimate_pitch_period() const
{
  // Estimate the per-channel pitch period of the last good frame by normalised
  // autocorrelation on the left channel. Returns 0 when the audio is essentially
  // unvoiced/noise (no clear periodicity), so the caller repeats the whole frame instead of
  // imposing a false tone.
  const i32 histPerCh = (i32)mLastGoodFrame.size() / 2;

  if (histPerCh <= 0 || mLastSampleRate == 0)
  {
    return 0;
  }

  const i32 pmin = std::max<i32>(1, (i32)(mLastSampleRate / 400)); // up to 400 Hz
  i32 pmax = (i32)(mLastSampleRate / 70);                          // down to 70 Hz

  if (pmax >= histPerCh)
  {
    pmax = histPerCh - 1;
  }
  if (pmax <= pmin)
  {
    return 0;
  }

  const i32 win = std::min<i32>(histPerCh - pmax, (i32)(mLastSampleRate / 100)); // ~10 ms reference window

  if (win <= 0)
  {
    return 0;
  }

  const i16 * const x = mLastGoodFrame.data(); // interleaved; left channel at even indices
  const i32 base = histPerCh - 1;              // per-channel index of the last sample

  f64 refEnergy = 0.0;
  for (i32 i = 0; i < win; ++i)
  {
    const f64 v = x[2 * (base - i)];
    refEnergy += v * v;
  }
  if (refEnergy <= 0.0)
  {
    return 0;
  }

  f64 bestScore = 0.0;
  i32 bestLag = 0;

  for (i32 lag = pmin; lag <= pmax; ++lag)
  {
    f64 corr = 0.0;
    f64 lagEnergy = 0.0;

    for (i32 i = 0; i < win; ++i)
    {
      const f64 a = x[2 * (base - i)];
      const f64 b = x[2 * (base - i - lag)];
      corr += a * b;
      lagEnergy += b * b;
    }

    if (lagEnergy <= 0.0)
    {
      continue;
    }

    const f64 score = corr / std::sqrt(refEnergy * lagEnergy);

    if (score > bestScore)
    {
      bestScore = score;
      bestLag = lag;
    }
  }

  return (bestScore >= 0.30) ? bestLag : 0;
}

void FaadDecoder::_apply_exit_crossfade(i16 * const ioPcm, const i32 iNumSamples) const
{
  // Smooth the transition from concealed audio back to real audio with a short linear
  // cross-fade so the boundary does not click. The concealment is extrapolated a few more
  // samples (continuing its pitch phase from the previous good frame still held in
  // mLastGoodFrame) and faded out while the fresh good frame is faded in.
  const i32 prevHistSize = (i32)mLastGoodFrame.size();

  if (prevHistSize == 0 || prevHistSize != iNumSamples)
  {
    return;
  }

  const i32 perCh = iNumSamples / 2;
  const i32 histPerCh = prevHistSize / 2;
  const i32 period = (mPitchPeriod > 0) ? mPitchPeriod : histPerCh;

  i32 xf = (mLastSampleRate > 0) ? (i32)(mLastSampleRate / 200) : 240; // ~5 ms
  xf = std::min(xf, perCh);

  for (i32 n = 0; n < xf; ++n)
  {
    const f32 good = (f32)(n + 1) / (f32)(xf + 1); // good-signal weight ramps 0 -> 1
    const i32 srcPerCh = histPerCh - period + ((mConcealPhase + n) % period);

    for (i32 ch = 0; ch < 2; ++ch)
    {
      const f32 conceal = (f32)mLastGoodFrame[srcPerCh * 2 + ch] * mConcealDecay;
      ioPcm[n * 2 + ch] = (i16)((1.0f - good) * conceal + good * (f32)ioPcm[n * 2 + ch]);
    }
  }
}

void FaadDecoder::conceal_lost_frame(const i32 iNumSamples)
{
  // Pitch-synchronous packet-loss concealment. Instead of repeating the whole last frame
  // (which makes the filler periodic at the 20/40 ms frame rate - an audible buzz), we
  // repeat a single pitch period taken from the last good frame. The filler is then periodic
  // at the voice/instrument pitch, which is far less objectionable. Starting one period back
  // (histPerCh - period) makes the filler phase-continuous with the last good sample, and the
  // running phase (mConcealPhase) keeps it continuous across successive lost frames. An
  // exponential amplitude decay fades sustained loss to silence so it does not drone.
  const i32 histSize = (i32)mLastGoodFrame.size();

  // No usable history yet, or an unexpected size -> plain silence keeps the timeline intact.
  if (histSize == 0 || histSize != iNumSamples)
  {
    const std::vector<i16> silence(iNumSamples, 0);
    mpAudioBuffer->put_data_into_ring_buffer(silence.data(), iNumSamples);
    return;
  }

  const i32 perCh = iNumSamples / 2;
  const i32 histPerCh = histSize / 2;

  if (!mConcealActive)
  {
    // First lost frame of a run: analyse the pitch of the audio we are about to extend.
    mPitchPeriod = _estimate_pitch_period();
    mConcealPhase = 0;
    mConcealActive = true;
  }

  // When no clear pitch was found (unvoiced/noise) fall back to repeating the whole frame:
  // for noise-like content that is indistinguishable and avoids inventing a tone.
  const i32 period = (mPitchPeriod > 0) ? mPitchPeriod : histPerCh;

  std::vector<i16> concealed(iNumSamples);

  for (i32 n = 0; n < perCh; ++n)
  {
    const i32 srcPerCh = histPerCh - period + ((mConcealPhase + n) % period);

    for (i32 ch = 0; ch < 2; ++ch)
    {
      concealed[n * 2 + ch] = (i16)((f32)mLastGoodFrame[srcPerCh * 2 + ch] * mConcealDecay);
    }
  }

  mConcealPhase += perCh;
  mConcealDecay *= cConcealDecayFactor;

  mpAudioBuffer->put_data_into_ring_buffer(concealed.data(), iNumSamples);

  // Trigger the audio pipeline the same way convert_mp4_to_pcm() does, so the concealed
  // samples are not stranded in the decoder ring buffer during a sustained error run.
  if (mLastSampleRate > 0 && mpAudioBuffer->get_ring_buffer_read_available() > mLastAudioBufferFillSize)
  {
    emit signal_new_audio(mLastAudioBufferFillSize, mLastSampleRate, mLastAudioFlags);
  }
}
