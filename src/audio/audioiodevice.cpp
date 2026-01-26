/*
 * The idea of This file was original taken from AbracaDABra and is adapted by Thomas Neder
 * (https://github.com/tomneda)
 * The original copyright information is preserved below and is acknowledged.
 */

#include "audioiodevice.h"
#include "techdata.h"
#include "audiofifo.h"
#include <QLoggingCategory>

// Q_LOGGING_CATEGORY(sLogAudioIODevice, "AudioIODevice", QtDebugMsg)
Q_LOGGING_CATEGORY(sLogAudioIODevice, "AudioIODevice", QtInfoMsg)

AudioIODevice::AudioIODevice(QObject * const iParent)
  : QIODevice(iParent)
  , mpTechDataBuffer(sRingBufferFactoryInt16.get_ringbuffer(RingBufferFactory<i16>::EId::TechDataBuffer).get())
{
  mTimerPeakLevel.setSingleShot(true);
  mTimerPeakLevel.start(50);
}

void AudioIODevice::set_buffer(SAudioFifo * const iBuffer)
{
  qCDebug(sLogAudioIODevice) << "Audio sample rate:" << iBuffer->sampleRate;
  mpInFifo = iBuffer;

  mSampleRateKHz = iBuffer->sampleRate / 1000;
  mPeakLevelSampleCntBothChannels = iBuffer->sampleRate / cNumPeakLevelPerSecond; // 20 "peaks" per second
}

void AudioIODevice::start()
{
  mStopFlag = false;
  mDoStop = false;
  mPlaybackState = EPlaybackState::Muted;
  open(QIODevice::ReadOnly);

  // for testing of error
  // QTimer::singleShot(10000, this, [this](){ mDoStop = true; } );
}

void AudioIODevice::stop()
{
  mStopFlag = true;     // set stop bit
}

void AudioIODevice::_fade(const i32 iNumStereoSamples, const f32 coe, f32 gain, i16 * dataPtr) const
{
  for (int_fast32_t n = 0; n < iNumStereoSamples; ++n)
  {
    gain *= coe; // we do not distinguish between fade-in and fade-out here for simplicity
    // printf("%.1f ", 20 * std::log10(gain));
    for (uint_fast8_t c = 0; c < 2 /*channels*/; ++c) // apply to both channels the same
    {
      *dataPtr = (i16)qRound(gain * (f32)(*dataPtr));
      dataPtr++;
    }
  }
  // printf("\n");
}

void AudioIODevice::_fade_in_audio_samples(i16 * const opData, const i32 iNumStereoSamples) const
{
  qCDebug(sLogAudioIODevice) << "Unmuting audio";
  const i32 numFadedStereoSamples = std::min<i32>(iNumStereoSamples, (i32)(cFadeTimeMs * (f32)mSampleRateKHz));
  const f32 coe = 2.0f - powf(10.0f, cFadeMinDb / (20.0f * (f32)numFadedStereoSamples));
  qCDebug(sLogAudioIODevice) << "numFadedStereoSamples" << numFadedStereoSamples << "coe" << coe;

  _fade(numFadedStereoSamples, coe, cFadeMinLin, opData);
}

void AudioIODevice::_fade_out_audio_samples(i16 * const opData, const i32 iNumStereoSamples) const
{
  qCDebug(sLogAudioIODevice, "Muting... [available %u samples]", static_cast<u32>(iNumStereoSamples));
  const i32 numFadedStereoSamples = std::min<i32>(iNumStereoSamples, (i32)(cFadeTimeMs * (f32)mSampleRateKHz));
  const f32 coe = powf(10.0f, cFadeMinDb / (20.0f * (f32)numFadedStereoSamples));
  qCDebug(sLogAudioIODevice) << "numFadedStereoSamples" << numFadedStereoSamples << "coe" << coe;

  _fade(numFadedStereoSamples, coe, 1.0f, opData);

  // are there remaining samples to overwrite to zero?
  if (iNumStereoSamples > numFadedStereoSamples)
  {
    memset(opData + numFadedStereoSamples * 2 /*channels*/, 0, (iNumStereoSamples - numFadedStereoSamples) * (2 /*channels*/ * sizeof(i16)));
  }
}

qint64 AudioIODevice::readData(char * const opDataBytes, const qint64 iMaxWantedBytesBothChannels) // this is called periodically by QtAudio subsystem
{
  // qDebug() << Q_FUNC_INFO << iMaxWantedSizeOfReadDataBytes;
  if (mDoStop || iMaxWantedBytesBothChannels == 0)
  {
    return 0;
  }

  RingBuffer<i16> * const rb = mpInFifo->pRingbuffer;
  i16 * const opDataSamplesBothChannels = reinterpret_cast<i16 *>(opDataBytes); // left channel [2n + 0]: right channel [2n + 1]

  assert(iMaxWantedBytesBothChannels <= INT32_MAX); // would be strange but better check it, we go on using only i32 length further
  const i32 maxWantedBytesBothChannels = static_cast<i32>(iMaxWantedBytesBothChannels);
  assert((maxWantedBytesBothChannels & 0x3) == 0); // stereo with 2 byte sized samples (divisible by 4)
  const i32 maxWantedSamplesBothChannels = maxWantedBytesBothChannels >> 1; // sizeof(i16)
  const i32 maxWantedSamplesStereoPair = maxWantedBytesBothChannels >> 2;
  i32 maxWantedSamplesStereoPairForFading = maxWantedSamplesStereoPair;

  const i32 availableSamplesBothChannels = rb->get_ring_buffer_read_available();
  assert((availableSamplesBothChannels & 0x1) == 0); // as both stereo samples are given, it must be even!

  bool muteRequest = mMuteFlag || mStopFlag;
  mDoStop = mStopFlag;

  if (mPlaybackState == EPlaybackState::Muted)
  {
    // muted
    // condition to unmute is enough samples
    //if (numSamplesAvailBothChannels > 500 * mSampleRateKHz * 2 /*channels*/)    // 500ms of signal
    if (availableSamplesBothChannels > SAudioFifo::cAudioFifoSizeSamplesBothChannels >> 1) // wait for half buffer is filled
    {
      assert(maxWantedSamplesBothChannels <= availableSamplesBothChannels); // numSamplesWantedBothChannels is usually 16384 bytes so this should be fulfilled!? (if happened there is no memory issue)

      // enough samples => reading data from input fifo
      if (muteRequest)
      {
        // staying muted -> setting output buffer to 0
        rb->advance_ring_buffer_read_index(maxWantedSamplesBothChannels);
        memset(opDataSamplesBothChannels, 0, maxWantedBytesBothChannels);

        _eval_peak_audio_level(opDataSamplesBothChannels, maxWantedSamplesBothChannels);
        return iMaxWantedBytesBothChannels;
      }

      // at this point we have enough sample to unmute and there is no request => preparing data
      //_extract_audio_data_from_fifo(opData, bytesToRead);
      rb->get_data_from_ring_buffer(opDataSamplesBothChannels, maxWantedSamplesBothChannels);

      // unmute request
      muteRequest = false;
    }
    else
    {   // not enough samples ==> inserting silence
      qCDebug(sLogAudioIODevice, "Muted: Inserting silence [%u ms]", static_cast<u32>(maxWantedSamplesBothChannels / (2 /*channels*/ * mSampleRateKHz)));

      memset(opDataSamplesBothChannels, 0, maxWantedBytesBothChannels);

      // done
      _eval_peak_audio_level(opDataSamplesBothChannels, maxWantedSamplesBothChannels);
      return iMaxWantedBytesBothChannels;
    }
  }
  else
  {
    // (AudioOutputPlaybackState::Muted != m_playbackState)
    // cannot be anything else than Muted or Playing ==> playing
    // condition to mute is not enough samples || muteFlag
    if (availableSamplesBothChannels < maxWantedSamplesBothChannels)
    {
      // not enough samples -> reading what we have and filling rest with zeros
      // minimum mute time is 1ms (m_sampleRate_kHz samples) , if less then hard mute
      if (availableSamplesBothChannels < (i32)(mSampleRateKHz * (2 /*channels*/ * sizeof(i16))))
      {
        // nothing to play
        qCDebug(sLogAudioIODevice, "Hard mute [no samples available]");
        memset(opDataSamplesBothChannels, 0, maxWantedBytesBothChannels);
        _eval_peak_audio_level(opDataSamplesBothChannels, maxWantedSamplesBothChannels);
        mPlaybackState = EPlaybackState::Muted;
        return iMaxWantedBytesBothChannels;
      }

      //_extract_audio_data_from_fifo(opData, numSamplesAvailBothChannels);
      rb->get_data_from_ring_buffer(opDataSamplesBothChannels, availableSamplesBothChannels); // take what we have ...
      memset(opDataSamplesBothChannels + availableSamplesBothChannels, 0, maxWantedBytesBothChannels - (availableSamplesBothChannels * sizeof(i16))); // ... and set rest of the samples to be 0

      maxWantedSamplesStereoPairForFading = availableSamplesBothChannels / 2 /*channels*/;

      // request to apply mute ramp
      muteRequest = true;  // mute
    }
    else
    {
      // enough sample available -> reading samples -> this is the normal running operation
      rb->get_data_from_ring_buffer(opDataSamplesBothChannels, maxWantedSamplesBothChannels);
      //_extract_audio_data_from_fifo(opData, bytesToRead);

      if (!muteRequest)
      {
        // done
        _eval_peak_audio_level(opDataSamplesBothChannels, maxWantedSamplesBothChannels);
        return iMaxWantedBytesBothChannels;
      }
    }
  }

  // at this point we have buffer that needs to be muted or unmuted
  // it is indicated by playbackState variable

  // unmute
  if (!muteRequest)
  {
    // unmute can be requested only when there is enough samples
    _fade_in_audio_samples(opDataSamplesBothChannels, maxWantedSamplesStereoPairForFading);
    mPlaybackState = EPlaybackState::Playing; // playing
  }
  else
  {
    // mute can be requested when there is not enough samples or from HMI
    _fade_out_audio_samples(opDataSamplesBothChannels, maxWantedSamplesStereoPairForFading);
    mPlaybackState = EPlaybackState::Muted; // muted
  }

  _eval_peak_audio_level(opDataSamplesBothChannels, maxWantedSamplesBothChannels);
  return iMaxWantedBytesBothChannels;
}

qint64 AudioIODevice::writeData(const char * data, qint64 len)
{
  Q_UNUSED(data);
  Q_UNUSED(len);

  return 0;
}

qint64 AudioIODevice::bytesAvailable() const
{
  return 16384; // the minimum block size in windows seems to be 16768 Bytes, make it short to have a fluid peak level meter
}

qint64 AudioIODevice::size() const
{
  return SAudioFifo::cAudioFifoSizeSamplesBothChannels >> 1;
}

void AudioIODevice::set_mute_state(bool iMuteActive)
{
  mMuteFlag = iMuteActive;
}

void AudioIODevice::_eval_peak_audio_level(const i16 * const ipData, const i32 iNumSamples)
{
  assert(iNumSamples % 2 == 0);
  mpTechDataBuffer->put_data_into_ring_buffer(ipData, (i32)iNumSamples);
  emit signal_audio_data_available((i32)iNumSamples, (i32)mSampleRateKHz * 1000);

  for (i32 idx = 0; idx < iNumSamples; idx+=2)
  {
    const i16 absLeft  = (i16)std::abs(ipData[idx + 0]);
    const i16 absRight = (i16)std::abs(ipData[idx + 1]);

    if (absLeft  > mAbsPeakLeft)  mAbsPeakLeft  = absLeft;
    if (absRight > mAbsPeakRight) mAbsPeakRight = absRight;

    mPeakLevelCurSampleCnt++;

    if (mPeakLevelCurSampleCnt > mPeakLevelSampleCntBothChannels) // collect much enough samples? (also over more blocks)
    {
#ifdef __clang__
      constexpr f32 cOffs_dB = 90.308734f; // see issue https://github.com/tomneda/DABstar/issues/99
#else
      constexpr f32 cOffs_dB = 20 * std::log10((f32) INT16_MAX);
#endif
      const f32 left_dB =  (mAbsPeakLeft >  0 ? 20.0f * std::log10((f32) mAbsPeakLeft)  - cOffs_dB : -40.0f);
      const f32 right_dB = (mAbsPeakRight > 0 ? 20.0f * std::log10((f32) mAbsPeakRight) - cOffs_dB : -40.0f);

      emit signal_show_audio_peak_level(left_dB, right_dB);

      mPeakLevelCurSampleCnt = 0;
      mAbsPeakLeft = 0.0f;
      mAbsPeakRight = 0.0f;
      // break; // send only one time the peak level in the same block (call of this method)
    }
  }
}

// void AudioIODevice::insertTestTone(DSPCOMPLEX & ioS)
// {
//   f32 toneFreqHz = 1000.0f;
//   f32 level = 0.9f;
//
//   if (!testTone.Enabled)
//   {
//     return;
//   }
//
//   ioS *= (1.0f - level);
//   if (testTone.NoSamplRemain > 0)
//   {
//     testTone.NoSamplRemain--;
//     testTone.CurPhase += testTone.PhaseIncr;
//     testTone.CurPhase = PI_Constrain(testTone.CurPhase);
//     const f32 smpl = sin(testTone.CurPhase);
//     ioS += level * DSPCOMPLEX(smpl, smpl);
//   }
//   else if (++testTone.TimePeriodCounter > workingRate * testTone.TimePeriod)
//   {
//     testTone.TimePeriodCounter = 0;
//     testTone.NoSamplRemain = workingRate * testTone.SignalDuration;
//     testTone.CurPhase = 0.0f;
//     testTone.PhaseIncr = 2 * M_PI / workingRate * toneFreqHz;
//   }
// }
