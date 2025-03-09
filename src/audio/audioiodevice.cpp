/*
 * The idea of this file original taken from AbracaDABra and is adapted by Thomas Neder
 * (https://github.com/tomneda)
 * The original copyright information is preserved below and is acknowledged.
 */

#include "audioiodevice.h"
#include "radio.h"
#include "techdata.h"

// Q_LOGGING_CATEGORY(sLogAudioIODevice, "AudioIODevice", QtDebugMsg)
Q_LOGGING_CATEGORY(sLogAudioIODevice, "AudioIODevice", QtInfoMsg)

AudioIODevice::AudioIODevice(RadioInterface * const ipRI, QObject * const iParent)
  : QIODevice(iParent)
  , mpTechDataBuffer(sRingBufferFactoryInt16.get_ringbuffer(RingBufferFactory<int16_t>::EId::TechDataBuffer).get())
{
  connect(this, &AudioIODevice::signal_show_audio_peak_level, ipRI, &RadioInterface::slot_show_audio_peak_level, Qt::QueuedConnection);
  connect(this, &AudioIODevice::signal_audio_data_available, ipRI->get_techdata_widget(), &TechData::slot_audio_data_available, Qt::QueuedConnection);

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

void AudioIODevice::_fade(const int32_t iNumStereoSamples, const float coe, float gain, int16_t * dataPtr) const
{
  for (int_fast32_t n = 0; n < iNumStereoSamples; ++n)
  {
    gain *= coe; // we do not distinguish between fade-in and fade-out here for simplicity
    // printf("%.1f ", 20 * std::log10(gain));
    for (uint_fast8_t c = 0; c < 2 /*channels*/; ++c) // apply to both channels the same
    {
      *dataPtr = (int16_t)qRound(gain * (float)(*dataPtr));
      dataPtr++;
    }
  }
  // printf("\n");
}

void AudioIODevice::_fade_in_audio_samples(int16_t * const opData, const int32_t iNumStereoSamples) const
{
  qCDebug(sLogAudioIODevice) << "Unmuting audio";
  const int32_t numFadedStereoSamples = std::min<int32_t>(iNumStereoSamples, (int32_t)(cFadeTimeMs * (float)mSampleRateKHz));
  const float coe = 2.0f - powf(10.0f, cFadeMinDb / (20.0f * (float)numFadedStereoSamples));
  qCDebug(sLogAudioIODevice) << "numFadedStereoSamples" << numFadedStereoSamples << "coe" << coe;

  _fade(numFadedStereoSamples, coe, cFadeMinLin, opData);
}

void AudioIODevice::_fade_out_audio_samples(int16_t * const opData, const int32_t iNumStereoSamples) const
{
  qCDebug(sLogAudioIODevice, "Muting... [available %u samples]", static_cast<unsigned int>(iNumStereoSamples));
  const int32_t numFadedStereoSamples = std::min<int32_t>(iNumStereoSamples, (int32_t)(cFadeTimeMs * (float)mSampleRateKHz));
  const float coe = powf(10.0f, cFadeMinDb / (20.0f * (float)numFadedStereoSamples));
  qCDebug(sLogAudioIODevice) << "numFadedStereoSamples" << numFadedStereoSamples << "coe" << coe;

  _fade(numFadedStereoSamples, coe, 1.0f, opData);

  // are there remaining samples to overwrite to zero?
  if (iNumStereoSamples > numFadedStereoSamples)
  {
    memset(opData + numFadedStereoSamples * 2 /*channels*/, 0, (iNumStereoSamples - numFadedStereoSamples) * (2 /*channels*/ * sizeof(int16_t)));
  }
}

qint64 AudioIODevice::readData(char * const opDataBytes, const qint64 iMaxWantedBytesBothChannels) // this is called periodically by QtAudio subsystem
{
  // qDebug() << Q_FUNC_INFO << iMaxWantedSizeOfReadDataBytes;
  if (mDoStop || iMaxWantedBytesBothChannels == 0)
  {
    return 0;
  }

  RingBuffer<int16_t> * const rb = mpInFifo->pRingbuffer;
  int16_t * const opDataSamplesBothChannels = reinterpret_cast<int16_t *>(opDataBytes); // left channel [2n + 0]: right channel [2n + 1]

  assert(iMaxWantedBytesBothChannels <= INT32_MAX); // would be strange but better check it, we go on using only int32_t length further
  const int32_t maxWantedBytesBothChannels = static_cast<int32_t>(iMaxWantedBytesBothChannels);
  assert((maxWantedBytesBothChannels & 0x3) == 0); // stereo with 2 byte sized samples (divisible by 4)
  const int32_t maxWantedSamplesBothChannels = maxWantedBytesBothChannels >> 1; // sizeof(int16_t)
  const int32_t maxWantedSamplesStereoPair = maxWantedBytesBothChannels >> 2;
  int32_t maxWantedSamplesStereoPairForFading = maxWantedSamplesStereoPair;

  const int32_t availableSamplesBothChannels = rb->get_ring_buffer_read_available();
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
      qCDebug(sLogAudioIODevice, "Muted: Inserting silence [%u ms]", static_cast<unsigned int>(maxWantedSamplesBothChannels / (2 /*channels*/ * mSampleRateKHz)));

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
      if (availableSamplesBothChannels < (int32_t)(mSampleRateKHz * (2 /*channels*/ * sizeof(int16_t))))
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
      memset(opDataSamplesBothChannels + availableSamplesBothChannels, 0, maxWantedBytesBothChannels - (availableSamplesBothChannels * sizeof(int16_t))); // ... and set rest of the samples to be 0

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
#ifdef _WIN32
  return 16384; // the minimum block size in windows seems to be 16768 Bytes, make it short to have a fluid peak level meter
#else
  const qint64 avail_bf = QIODevice::bytesAvailable();
  const qint64 avail_rb = mpInFifo->pRingbuffer->get_ring_buffer_read_available() * sizeof(int16_t);
  quint64 avail_sum = avail_bf + avail_rb;
  if (avail_sum == 0) avail_sum = 0x100; // this is a workaround as readData() will never get read if 0 is given back
  return avail_sum;
#endif
}

qint64 AudioIODevice::size() const
{
#ifdef _WIN32
  return SAudioFifo::cAudioFifoSizeSamplesBothChannels >> 1;
#else
  const qint64 avail_bf = QIODevice::size();
  return avail_bf;
#endif
}

void AudioIODevice::set_mute_state(bool iMuteActive)
{
  mMuteFlag = iMuteActive;
}

void AudioIODevice::_eval_peak_audio_level(const int16_t * const ipData, const int32_t iNumSamples)
{
  assert(iNumSamples % 2 == 0);
  mpTechDataBuffer->put_data_into_ring_buffer(ipData, (int32_t)iNumSamples);
  emit signal_audio_data_available((int)iNumSamples, (int)mSampleRateKHz * 1000);

  for (int32_t idx = 0; idx < iNumSamples; idx+=2)
  {
    const int16_t absLeft  = (int16_t)std::abs(ipData[idx + 0]);
    const int16_t absRight = (int16_t)std::abs(ipData[idx + 1]);

    if (absLeft  > mAbsPeakLeft)  mAbsPeakLeft  = absLeft;
    if (absRight > mAbsPeakRight) mAbsPeakRight = absRight;

    mPeakLevelCurSampleCnt++;

    if (mPeakLevelCurSampleCnt > mPeakLevelSampleCntBothChannels) // collect much enough samples? (also over more blocks)
    {
      constexpr float cOffs_dB = 20 * std::log10((float) INT16_MAX); // in the assumption that subtraction is faster than dividing (but not sure with float)
      const float left_dB =  (mAbsPeakLeft >  0 ? 20.0f * std::log10((float) mAbsPeakLeft)  - cOffs_dB : -40.0f);
      const float right_dB = (mAbsPeakRight > 0 ? 20.0f * std::log10((float) mAbsPeakRight) - cOffs_dB : -40.0f);

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
//   float toneFreqHz = 1000.0f;
//   float level = 0.9f;
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
//     const float smpl = sin(testTone.CurPhase);
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
