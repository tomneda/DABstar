/*
 * The idea of This file was original taken from AbracaDABra and is adapted by Thomas Neder
 * (https://github.com/tomneda)
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
  , mpLevelMeterBuffer(sRingBufferFactoryFloat.get_ringbuffer(RingBufferFactory<f32>::EId::LevelMeterBuffer).get())
{
  mpTimerPeakLevel = new QTimer(this); // must be a Qt child so it moves with AudioIODevice when QAudioSink relocates it to the audio thread
  mpTimerPeakLevel->setSingleShot(false);
  mpTimerPeakLevel->setInterval(1000 / cNumPeakLevelPerSecond);
  connect(mpTimerPeakLevel, &QTimer::timeout, this, &AudioIODevice::_slot_timer_level_meter);
  mpTimerPeakLevel->start();
}

void AudioIODevice::set_buffer(SAudioFifo * const iBuffer)
{
  qCDebug(sLogAudioIODevice) << "Audio sample rate:" << iBuffer->sampleRate;
  mpInFifo = iBuffer;

  mSampleRateKHz = iBuffer->sampleRate / 1000;
  mPeakLevelSampleCntBothChannels = iBuffer->sampleRate / cNumPeakLevelPerSecond; // 20 "peaks" per second
  mRmsAlpha = 5.0f / (f32)iBuffer->sampleRate;
  mTestTone.set_sample_rate(mSampleRateKHz);
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

      if (!muteRequest)
      {
        // done
        mTestTone.process(opDataSamplesBothChannels, maxWantedSamplesBothChannels);
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

void AudioIODevice::set_test_tone(const bool iActive)
{
  mTestTone.activate(iActive);
}

void AudioIODevice::set_peak_level_delay(const i32 iDelaySteps)
{
  mDelayLine.set_delay_steps(iDelaySteps);
}

void AudioIODevice::_eval_peak_audio_level(const i16 * const ipData, const i32 iNumSamples)
{
  assert(iNumSamples % 2 == 0);
  mpTechDataBuffer->put_data_into_ring_buffer(ipData, (i32)iNumSamples);  // for audio spectrum analyzer
  emit signal_audio_data_available((i32)iNumSamples, (i32)mSampleRateKHz * 1000);

  for (i32 idx = 0; idx < iNumSamples; idx+=2)
  {
    const i16 absLeft  = (i16)std::abs(ipData[idx + 0]);
    const i16 absRight = (i16)std::abs(ipData[idx + 1]);

    if (absLeft  > mAbsPeakLeft)  mAbsPeakLeft  = absLeft;
    if (absRight > mAbsPeakRight) mAbsPeakRight = absRight;
    
    const f32 meanSqLeft  = (f32)absLeft  * (f32)absLeft;
    const f32 meanSqRight = (f32)absRight * (f32)absRight;
    
    mMeanSqLeft  += mRmsAlpha * (meanSqLeft - mMeanSqLeft);
    mMeanSqRight += mRmsAlpha * (meanSqRight - mMeanSqRight);

    ++mPeakLevelCurSampleCnt;

    if (mPeakLevelCurSampleCnt > mPeakLevelSampleCntBothChannels) // collect many enough samples? (also over more blocks)
    {
      static const f32 cOffs_dB = 20 * std::log10((f32)std::numeric_limits<i16>::max()); // do not use constexpr as it is not supported by all compilers for std::log10()
      SStereoPeakLevel spl;
      spl.peakLeft  = (mAbsPeakLeft  > 0 ? 20.0f * std::log10((f32) mAbsPeakLeft)  - cOffs_dB : -40.0f);
      spl.peakRight = (mAbsPeakRight > 0 ? 20.0f * std::log10((f32) mAbsPeakRight) - cOffs_dB : -40.0f);
      spl.rmsLeft   = (mMeanSqLeft   > 0 ? 10.0f * std::log10(mMeanSqLeft)  - cOffs_dB : -40.0f);
      spl.rmsRight  = (mMeanSqRight  > 0 ? 10.0f * std::log10(mMeanSqRight) - cOffs_dB : -40.0f);

      // Peak-level packets are produced every 1 / cNumPeakLevelPerSecond.
      // Since the ring buffer is more than twice that interval, use this small buffer to read each packet back at a constant rate.
      if (mpLevelMeterBuffer->put_data_into_ring_buffer(spl.buffer.data(), spl.buffer.size()) != spl.buffer.size())
      {
        // qDebug() << "Level meter ring buffer full -> flush buffer";
        mpLevelMeterBuffer->flush_ring_buffer();
      }

      mPeakLevelCurSampleCnt = 0;

      // decay the peak level each 1 / cNumPeakLevelPerSecond (50 ms) about 1dB
      mAbsPeakLeft  *= cDecayFactor;
      mAbsPeakRight *= cDecayFactor;
      // mMeanSqLeft   *= cDecayFactor;
      // mMeanSqRight  *= cDecayFactor;
    }
  }
}

void AudioIODevice::_slot_timer_level_meter()
{
  const i32 numFloats = mpLevelMeterBuffer->get_ring_buffer_read_available();

  if ((numFloats % 4) != 0)
  {
    qWarning() << "buffer not correct aligned";
    mpLevelMeterBuffer->flush_ring_buffer();
    return;
  }

  if (numFloats >= 4)
  {
#ifdef _WIN32
    // On Windows: timer resolution is coarse (~15ms) and audio blocks are large, so multiple
    // packets may accumulate. Skip stale ones and keep only the latest.
    const i32 numPackets = numFloats / 4;
    if (numPackets > 1)
    {
      mpLevelMeterBuffer->advance_ring_buffer_read_index((numPackets - 1) * 4);
    }
#endif

    mpLevelMeterBuffer->get_data_from_ring_buffer(mLastSpl.buffer.data(), mLastSpl.buffer.size());
  }
  // else: buffer underflow — no new packet since last timer fire (can happen on Windows when the
  // audio block period exceeds 50ms). Re-use the last known level so the meter does not freeze.

  const SStereoPeakLevel delayed = mDelayLine.get_set_value(mLastSpl);
  // qDebug() << "emit signal_show_audio_peak_level";
  emit signal_show_audio_peak_level(delayed.peakLeft, delayed.peakRight, delayed.rmsLeft, delayed.rmsRight);
}

