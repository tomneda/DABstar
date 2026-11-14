/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2020
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
 *    along with Qt-DAB if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "dab_processor.h"
#include "msc_handler.h"
#include "dabradio.h"
#include "process_params.h"
#include "eti_generator.h"
#include "crc.h"

/**
  * \brief DabProcessor
  * The DabProcessor class is the driver of the processing
  * of the samplestream.
  * It is the main interface to the qt-dab program,
  * local are classes OfdmDecoder, FicHandler and mschandler.
  */

// Settle time after a (re-)tune, formerly a sleep in DabRadio::_start_channel(). Counting samples instead of
// wall-clock time has to happen here in the processor thread: the GUI thread must stay responsive, else devices
// which read their samples there (rtl_tcp) deliver nothing at all during the settle time.
static constexpr i32 cSettleSampleCnt = INPUT_RATE / 4; // 250 ms

DabProcessor::DabProcessor(DabRadio * const mr, IDeviceHandler * const inputDevice, ProcessParams * const p)
  : mpRadioInterface(mr)
  , mpEtiSource(inputDevice->get_eti_source())
  , mSampleReader(mr, inputDevice, p->spectrumBuffer)
  , mFicHandler(mr)
  , mpFibDecoder(mFicHandler.get_fib_decoder())
  , mMscHandler(mr, p->frameBuffer)
  , mPhaseReference(mr, p)
  , mOfdmDecoder(mr, p->iqBuffer, p->carrBuffer)
  , mTimeSyncer(&mSampleReader)
  , mcThreshold(p->threshold)
  , mcTiiFramesToCount(p->tiiFramesToCount)
  , mpCirBuffer(p->cirBuffer)
{
  mFftPlan = fftwf_plan_dft_1d(cTu, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_FORWARD, FFTW_MEASURE);

  connect(this, &DabProcessor::signal_show_spectrum, mpRadioInterface, &DabRadio::slot_show_spectrum);
  connect(this, &DabProcessor::signal_show_tii, mpRadioInterface, &DabRadio::slot_show_tii);
  connect(this, &DabProcessor::signal_show_clock_err, mpRadioInterface, &DabRadio::slot_show_clock_error);
  connect(this, &DabProcessor::signal_set_and_show_freq_corr_rf_Hz, mpRadioInterface, &DabRadio::slot_set_and_show_freq_corr_rf_Hz);
  connect(this, &DabProcessor::signal_show_freq_corr_bb_Hz, mpRadioInterface, &DabRadio::slot_show_freq_corr_bb_Hz);
  connect(this, &DabProcessor::signal_linear_peak_and_rms_level, mpRadioInterface, &DabRadio::slot_show_digital_peak_and_rms_level);

  mBits.resize(c2K);
  mTiiDetector.reset();
  mTiiCounter = 0;
}

DabProcessor::~DabProcessor()
{
  mSampleReader.set_running(false);

  if (isRunning())
  {
    wait(); // This blocks the destructor until run() actually returns
  }
  fftwf_destroy_plan(mFftPlan);  // destroy this only after the accessing thread has really finished
  qDebug() << "DabProcessor has stopped";
}

void DabProcessor::start()
{
  mFicHandler.restart();
  if (!mScanMode)
  {
    mMscHandler.reset_channel();
  }
  QThread::start();
}

void DabProcessor::stop()
{
  mSampleReader.set_running(false);
  while (isRunning())
  {
    wait();
  }
  usleep(10000);
  mFicHandler.stop();
}

void DabProcessor::run()  // run QThread
{
  // this method is called with each new set frequency
  f32 syncThreshold = 0;
  i32 sampleCount = 0;
  mRfFreqShiftUsed = false;

  mSampleReader.set_running(true);  // useful after a restart

  if (mpEtiSource != nullptr)
  {
    _run_eti(); // no samples available, the ETI frames contain already decoded data
    return;
  }

  enum class EState
  {
    WAIT_FOR_TIME_SYNC_MARKER, EVAL_SYNC_SYMBOL, PROCESS_REST_OF_FRAME
  };

  _set_rf_freq_offs_Hz(0.0f);
  _set_bb_freq_offs_Hz(0.0f);

  mFicHandler.reset_fic_decode_success_ratio(); // mCorrectionNeeded will be true next call to getFicDecodeRatioPercent()
  mFreqOffsSyncSymb = 0.0f;
  mTimeSyncAttemptCount = 0;
  mTimeSyncerStateLast = TimeSyncer::EState::IDLE;

  EState state = EState::WAIT_FOR_TIME_SYNC_MARKER;

  try // mSampleReader.getSample() can trigger an exception
  {
    mSampleReader.discard_samples(cSettleSampleCnt); // let the tuner settle before anything is evaluated

    // To get some idea of the signal strength
    for (int32_t i = 0; i < 20; i++)
    {
      mSampleReader.get_samples(mOfdmBuffer, 0, cTu, 0, false);
    }

    while (true)
    {
      switch (state)
      {
      case EState::WAIT_FOR_TIME_SYNC_MARKER:
      {
        mTiiDetector.reset();
        mOfdmDecoder.reset();
        mTiiCounter = 0;
        sampleCount = 0;
        syncThreshold = mcThreshold;
        mClockOffsetFrameCount = mClockOffsetTotalSamples = 0;
        const bool ok = _state_wait_for_time_sync_marker();
        state = (ok ? EState::EVAL_SYNC_SYMBOL : EState::WAIT_FOR_TIME_SYNC_MARKER);
        mClockErrHz = 0.0f;
        break;
      }

      case EState::EVAL_SYNC_SYMBOL:
      {
        /**
          * Looking for the first sample of the T_u part of the sync block.
          * Note that we probably already had 30 to 40 samples of the T_g
          * part
          */
        const bool ok = _state_eval_sync_symbol(sampleCount, syncThreshold);
        state = (ok ? EState::PROCESS_REST_OF_FRAME : EState::WAIT_FOR_TIME_SYNC_MARKER);
        break;
      }

      case EState::PROCESS_REST_OF_FRAME:
      {
        _state_process_rest_of_frame(sampleCount);
        state = EState::EVAL_SYNC_SYMBOL;
        syncThreshold = 2 * mcThreshold; // threshold is less sensitive while startup
        break;
      }
      } // switch
    } // while (true)
  }
  catch (i32 e)
  {
    (void)e;
    if (e != 20) qWarning() << "DabProcessor has stopped via exception:" << e; // e == 20 is normal condition
  }
}

// Processing loop for ETI input. The FIC and sub-channel data are fed directly into the FicDecoder and the MscHandler,
// all the OFDM related processing (and its displays) is not used.
void DabProcessor::_run_eti()
{
  constexpr i32 cFrameTimeout_ms = 100;
  constexpr i32 cNoSyncTimeout_ms = 2000;

  TEtiFrame frame;
  std::vector<SEtiSubChannel> subChannels;
  subChannels.reserve(64);
  bool synced = false;
  i32 noSyncTime_ms = 0;

  while (mSampleReader.is_running())
  {
    bool frameOk = false;

    if (mpEtiSource->get_eti_frame(frame, cFrameTimeout_ms))
    {
      frameOk = _process_eti_frame(frame, subChannels);
      if (!frameOk)
      {
        noSyncTime_ms += 24;
      }
    }
    else
    {
      noSyncTime_ms += cFrameTimeout_ms; // no frame available (e.g. file paused), but this is no loss of sync
      continue;
    }

    if (frameOk)
    {
      noSyncTime_ms = 0;
      if (!synced)
      {
        synced = true;
        emit signal_dip_sync_found();
      }
    }
    else if (noSyncTime_ms >= cNoSyncTimeout_ms)
    {
      noSyncTime_ms = 0;
      synced = false;
      emit signal_no_dip_sync_found();
    }
  }
}

// Evaluates one ETI(NI) frame according ETSI EN 300 799 (similar to ETIPlayer::DecodeFrame() in dablin).
// Returns false if the frame is not valid.
bool DabProcessor::_process_eti_frame(const TEtiFrame & iFrame, std::vector<SEtiSubChannel> & oSubChannels)
{
  const u8 * const f = iFrame.data();

  // ERR
  if (f[0] != 0xFF)
  {
    return false;
  }

  // FSYNC
  if (const u32 fsync = (u32)f[1] << 16 | (u32)f[2] << 8 | (u32)f[3];
      fsync != 0x073AB6 && fsync != 0xF8C549)
  {
    return false;
  }

  // null transmission
  if (f[4] == 0xFF && f[5] == 0xFF && f[6] == 0xFF && f[7] == 0xFF)
  {
    return false;
  }

  // FC
  const bool ficf = (f[5] & 0x80) != 0;
  const i32 nst = f[5] & 0x7F;
  const i32 mid = (f[6] & 0x18) >> 3;
  const i32 fl = (f[6] & 0x07) << 8 | f[7]; // in 32 bit words

  // header CRC over FC, STC and MNSC
  const i32 headerCrcDataLen = 4 + nst * 4 + 2;
  const u16 headerCrcStored = (u16)(f[4 + headerCrcDataLen] << 8 | f[4 + headerCrcDataLen + 1]);
  if (calc_crc(f + 4, headerCrcDataLen) != headerCrcStored)
  {
    return false;
  }

  i32 mstOffset = 4 + 4 + nst * 4 + 4;
  const i32 mstLen = (fl - nst - 1) * 4;

  if (mstLen < 0 || mstOffset + mstLen + 4 > cEtiFrameSize)
  {
    return false;
  }

  // EOF CRC over MST
  const u16 mstCrcStored = (u16)(f[mstOffset + mstLen] << 8 | f[mstOffset + mstLen + 1]);
  if (calc_crc(f + mstOffset, mstLen) != mstCrcStored)
  {
    return false;
  }

  // FIC: 24 words (3 FIBs) in mode I, II, IV; 32 words (4 FIBs) in mode III
  const i32 ficLen = ficf ? (mid == 3 ? 32 : 24) * 4 : 0;
  const i32 mstEnd = mstOffset + mstLen;

  if (mstOffset + ficLen > mstEnd)
  {
    return false;
  }

  if (ficLen > 0)
  {
    mFicHandler.process_eti_fic(f + mstOffset, ficLen / 32);
  }
  mstOffset += ficLen;

  if (mScanMode)
  {
    return true;
  }

  // STC
  oSubChannels.clear();

  for (i32 i = 0; i < nst; i++)
  {
    const u8 * const stc = f + 8 + i * 4;
    const i32 size = ((stc[2] & 0x03) << 8 | stc[3]) * 8; // STL is given in 64 bit units

    if (mstOffset + size > mstEnd)
    {
      return false;
    }

    oSubChannels.push_back({ .subChId = (stc[0] & 0xFC) >> 2,
                             .startAddr = (stc[0] & 0x03) << 8 | stc[1],
                             .pData = f + mstOffset,
                             .size = size });
    mstOffset += size;
  }

  mMscHandler.process_eti_frame(oSubChannels);
  return true;
}

void DabProcessor::_state_process_rest_of_frame(i32 & ioSampleCount)
{
  /*
   * The current OFDM symbol 0 in mOfdmBuffer is special in that it is used for fine time synchronization,
   * for coarse frequency synchronization and its content is used as a reference for decoding the first datablock.
   */

  // memcpy() is considerable faster than std::copy on my i7-6700K (nearly twice as fast for size == 2048)
  memcpy(mFftInBuffer.data(), mOfdmBuffer.data(), mFftInBuffer.size() * sizeof(cf32));

  fftwf_execute(mFftPlan);
  mOfdmDecoder.store_reference_symbol_0(mFftOutBuffer);

  i32 correction = 0;
  if (mFicHandler.get_fic_decode_ratio_percent() < 30) // Correction needed ?
  {
    // Here we look only at the symbol 0 when we need a coarse frequency synchronization.
    correction = mPhaseReference.estimate_carrier_offset_from_sync_symbol_0(mFftOutBuffer);
    if (correction != PhaseReference::IDX_NOT_FOUND)
    {
      mFreqOffsSyncSymb += (f32)correction;

      if (std::abs(mFreqOffsSyncSymb) > kHz(35))
      {
        mFreqOffsSyncSymb = 0.0f;
      }
    }
    if (correction != 0)
    {
      mClockErrHz = 0.0f;
    }

    _set_bb_freq_offs_Hz(mFreqOffsSyncSymb);
  }
  else
  {
    if (!mRfFreqShiftUsed && mAllowRfFreqShift)
    {
      mRfFreqShiftUsed = true;
      _set_rf_freq_offs_Hz(mFreqOffsSyncSymb); // takeover BB shift to RF
      _set_bb_freq_offs_Hz(0.0f); // no, no BB shift should be necessary
      mFreqOffsSyncSymb = 0.0f; // allow collect new remaining freq. shift
    }
  }

  mPhaseOffsetCyclPrefRad = _process_ofdm_symbols_1_to_L(ioSampleCount);

  // The first sample to be found for the next frame should be T_g samples ahead. Before going for the next frame, we just check the fineCorrector
  // We integrate the newly found frequency error with the existing frequency error.
  limit_symmetrically(mPhaseOffsetCyclPrefRad, 20.0f * F_RAD_PER_DEG);
  mFreqOffsSyncSymb += mPhaseOffsetCyclPrefRad / F_2_M_PI * (f32)cCarrDiff; // formerly 0.05
  _set_bb_freq_offs_Hz(mFreqOffsSyncSymb);

  _process_null_symbol(ioSampleCount);

  if (correction == 0)
  {
    f32 clockErrHz = INPUT_RATE * ((f32)ioSampleCount / (f32)cTF - 1.0f);
    limit_symmetrically(clockErrHz, 307.2f); // = 150 ppm
    mean_filter(mClockErrHz, clockErrHz, 0.1f);
  }
  mClockOffsetTotalSamples += ioSampleCount;
  if (++mClockOffsetFrameCount > 10) // about each second
  {
    const f32 clockErrHz = INPUT_RATE * ((f32)mClockOffsetTotalSamples / ((f32)mClockOffsetFrameCount * (f32)cTF) - 1.0f);
    emit signal_show_clock_err(clockErrHz);
    mClockOffsetTotalSamples = 0;
    mClockOffsetFrameCount = 0;


    f32 peakLevel, rmsLevel;
    mSampleReader.get_linear_peak_level_and_clear(peakLevel, rmsLevel);
    emit signal_linear_peak_and_rms_level(peakLevel, rmsLevel);
  }
}

void DabProcessor::_process_null_symbol(i32 & ioSampleCount)
{
  // We are at the end of the frame and we read the next T_n null samples
  mSampleReader.get_samples(mOfdmBuffer, 0, cTn, mFreqOffsBBHz, false);
  ioSampleCount += cTn;

  // is this null symbol with TII?
  const bool isTiiNullSegment = ((mpFibDecoder->get_cif_count() & 0x7) >= 4);
  memcpy(mFftInBuffer.data(), &(mOfdmBuffer[cTg]), cTu * sizeof(cf32));
  fftwf_execute(mFftPlan);

  if (!isTiiNullSegment) // eval SNR only in non-TII null segments
  {
    mOfdmDecoder.store_null_symbol_without_tii(mFftOutBuffer); // for SNR evaluation
  }
  else // this is TII null segment
  {
    mOfdmDecoder.store_null_symbol_with_tii(mFftOutBuffer); // for displaying TII

    // The TII data is encoded in the null period of the odd frames
    mTiiDetector.add_to_tii_buffer(mFftOutBuffer);

    if (++mTiiCounter >= mcTiiFramesToCount)
    {
      if (mEnableTii)
      {
        std::vector<STiiResult> transmitterIds = mTiiDetector.process_tii_data(mTiiThreshold);
        if (!transmitterIds.empty())
        {
          emit signal_show_tii(transmitterIds);
        }
      }
      mTiiCounter = 0;
    }
  }
}

f32 DabProcessor::_process_ofdm_symbols_1_to_L(i32 & ioSampleCount)
{
/**
  * After symbol 0, we will just read in the other (params -> L - 1) blocks
  *
  * The first ones are the FIC blocks these are handled within
  * the thread executing this "task", the other blocks
  * are passed on to be handled in the mscHandler, running
  * in a different thread.
  * We immediately start with building up an average of
  * the phase difference between the samples in the cyclic prefix
  * and the corresponding samples in the datapart.
  */
  cf32 freqCorr = cf32(0, 0);

  for (i16 ofdmSymbIdx = 1; ofdmSymbIdx < cL; ofdmSymbIdx++)
  {
    mSampleReader.get_samples(mOfdmBuffer, 0, cTs, mFreqOffsBBHz, true);
    ioSampleCount += cTs;

    // eval phase shift in cyclic prefix part
#ifdef HAVE_SSE_OR_AVX
    cf32 result;
    volk_32fc_x2_conjugate_dot_prod_32fc_a(&result, &mOfdmBuffer[cTu], &mOfdmBuffer[0], cTg);
    freqCorr += result;
#else
    for (i32 i = cTu; i < cTs; i++)
    {
      freqCorr += mOfdmBuffer[i] * conj(mOfdmBuffer[i - cTu]);
    }
#endif

    mOfdmDecoder.set_dc_offset(mSampleReader.get_dc_offset());
    memcpy(mFftInBuffer.data(), &(mOfdmBuffer[cTg]), cTu * sizeof(cf32));
    fftwf_execute(mFftPlan);
#ifdef DO_TIME_MEAS
    mTimeMeas.trigger_begin();
#endif
    mOfdmDecoder.decode_symbol(mFftOutBuffer, ofdmSymbIdx, mPhaseOffsetCyclPrefRad, mClockErrHz, freqCorr, mBits);
#ifdef DO_TIME_MEAS
    mTimeMeas.trigger_end();
#endif

    if (ofdmSymbIdx <= 3)
    {
      mFicHandler.process_block(mBits, ofdmSymbIdx);
    }

    if (mEtiOn)
    {
      mpEtiGenerator->process_block(mBits, ofdmSymbIdx);
    }

    if (ofdmSymbIdx > 3 && !mScanMode)
    {
      mMscHandler.process_block(mBits, ofdmSymbIdx);
    }
  }
#ifdef DO_TIME_MEAS
  mTimeMeas.print_time_per_round();
#endif

  return arg(freqCorr);
}

void DabProcessor::_set_bb_freq_offs_Hz(const f32 iFreqHz)
{
  const i32 iFreqHzInt = (i32)std::round(iFreqHz);
  if (std::round(mFreqOffsBBHz) != iFreqHzInt)
  {
    emit signal_show_freq_corr_bb_Hz(iFreqHzInt);
  }
  mFreqOffsBBHz = iFreqHz;
}

void DabProcessor::_set_rf_freq_offs_Hz(const f32 iFreqHz)
{
  const i32 iFreqHzInt = (i32)std::round(iFreqHz);
  if (mFreqOffsRFHz != iFreqHzInt)
  {
    mFreqOffsRFHz = iFreqHzInt;
    emit signal_set_and_show_freq_corr_rf_Hz(mFreqOffsRFHz);   // this call changes the RF frequency and display! (hopefully fast enough)
  }
}

bool DabProcessor::_state_eval_sync_symbol(i32 & oSampleCount, const f32 iThreshold)
{
  // get first OFDM symbol after time sync marker
  mSampleReader.get_samples(mOfdmBuffer, 0, cTu, mFreqOffsBBHz, false);

  const i32 startIndex = mPhaseReference.correlate_with_phase_ref_and_find_max_peak(mOfdmBuffer, iThreshold);

  if (startIndex < 0)
  {
    // no sync, try again
    return false;
  }
  else
  {
    // Once here, we are synchronized, we need to copy the data we used for synchronization for symbol 0
    const i32 nextOfdmBufferIdx = cTu - startIndex;
    assert(nextOfdmBufferIdx >= 0);

    // move/read OFDM symbol 0 which contains the synchronization data
    memmove(mOfdmBuffer.data(), &(mOfdmBuffer[startIndex]), nextOfdmBufferIdx * sizeof(cf32)); // memmove can move overlapping segments correctly
    mSampleReader.get_samples(mOfdmBuffer, nextOfdmBufferIdx, cTu - nextOfdmBufferIdx, mFreqOffsBBHz, false); // get reference symbol

    oSampleCount = startIndex + cTu;
    return true;
  }
}

bool DabProcessor::_state_wait_for_time_sync_marker()
{
  const TimeSyncer::EState timeSyncState = mTimeSyncer.read_samples_until_end_of_level_drop();
  switch (timeSyncState)
  {
  case TimeSyncer::EState::TIMESYNC_ESTABLISHED:
    // if (timeSyncState != mTimeSyncerStateLast)  qDebug() << "Time sync established";
    mTimeSyncAttemptCount = 0;
    emit signal_dip_sync_found();
    return true;
  case TimeSyncer::EState::NO_DIP_FOUND:
    if (++mTimeSyncAttemptCount >= 8)
    {
      if (timeSyncState != mTimeSyncerStateLast) qWarning() << "No DIP found after" << mTimeSyncAttemptCount << "attempts";
      mTimeSyncAttemptCount = 0;
      emit signal_no_dip_sync_found();
    }
    break;
  case TimeSyncer::EState::NO_END_OF_DIP_FOUND:
    if (timeSyncState != mTimeSyncerStateLast) qWarning() << "No end of DIP found";
    mTimeSyncAttemptCount = 0;
    break;
  default: qCritical() << "Unknown time syncer state";
  }
  mTimeSyncerStateLast = timeSyncState;
  return false;
}

void DabProcessor::set_scan_mode(bool b)
{
  mScanMode = b;
}

//  just convenience functions
//  FicDecoder abstracts channel data

void DabProcessor::activate_cir_viewer(bool iActivate)
{
  mSampleReader.set_cir_buffer(iActivate ? mpCirBuffer : nullptr);
}

bool DabProcessor::is_service_running(const SDescriptorType & iDT, const EProcessFlag iProcessFlag) const
{
  // assert(!mScanMode);
  return mMscHandler.is_service_running(iDT.SubChId, iProcessFlag);
}

bool DabProcessor::is_service_running(const i32 iSubChId, const EProcessFlag iProcessFlag) const
{
  // assert(!mScanMode);
  return mMscHandler.is_service_running(iSubChId, iProcessFlag);
}

void DabProcessor::stop_service(const i32 iSubChId, const EProcessFlag iProcessFlag)
{
  if (!mScanMode)
  {
    mMscHandler.stop_service(iSubChId, iProcessFlag);
  }
}

void DabProcessor::stop_all_services()
{
  if (!mScanMode)
  {
    mMscHandler.stop_all_services();
  }
}

bool DabProcessor::set_audio_channel(const SAudioData & iAD, RingBuffer<i16> * ipoAudioBuffer, const EProcessFlag iProcessFlag)
{
  if (!mScanMode)
  {
    return mMscHandler.set_channel(&iAD, ipoAudioBuffer, (RingBuffer<u8>*)nullptr, iProcessFlag);
  }
  else
  {
    return false;
  }
}

bool DabProcessor::set_data_channel(const SPacketData & iPD, RingBuffer<u8> * ipoDataBuffer, const EProcessFlag iProcessFlag)
{
  if (!mScanMode)
  {
    return mMscHandler.set_channel(&iPD, (RingBuffer<i16>*)nullptr, ipoDataBuffer, iProcessFlag);
  }
  else
  {
    return false;
  }
}

void DabProcessor::start_dumping(SNDFILE * f)
{
  mSampleReader.start_dumping(f);
}

void DabProcessor::stop_dumping()
{
  mSampleReader.stop_dumping();
}

void DabProcessor::start_fic_dump(FILE * f)
{
  mFicHandler.start_fic_dump(f);
}

void DabProcessor::stop_fic_dump()
{
  mFicHandler.stop_fic_dump();
}

bool DabProcessor::start_eti_generator(const QString & s)
{
  if (mpEtiSource != nullptr) // there are no soft bits available to generate ETI from
  {
    return false;
  }

  if (mEtiOn)
  {
    return true;
  }

  mpEtiGenerator = std::make_unique<EtiGenerator>(&mFicHandler);

  if (mpEtiGenerator == nullptr || !mpEtiGenerator->start_eti_generator(s))
  {
    mpEtiGenerator.reset();
    return false;
  }

  mEtiOn = true; // do this at last at it is asked for in a different thread
  return true;
}

void DabProcessor::stop_eti_generator()
{
  if (!mEtiOn)
  {
    return;
  }

  mEtiOn = false;
  mpEtiGenerator->stop_eti_generator(); // this would hang short time if process thread is still running
  mpEtiGenerator.reset();
}

void DabProcessor::reset_eti_generator()
{
  if (!mEtiOn)
  {
    return;
  }

  mpEtiGenerator->reset();
}

void DabProcessor::slot_select_carrier_plot_type(ECarrierPlotType iPlotType)
{
  mOfdmDecoder.set_select_carrier_plot_type(iPlotType);
}

void DabProcessor::slot_select_iq_plot_type(EIqPlotType iPlotType)
{
  mOfdmDecoder.set_select_iq_plot_type(iPlotType);
}

void DabProcessor::slot_soft_bit_gen_type(ESoftBitType iSoftBitType)
{
  mOfdmDecoder.set_soft_bit_gen_type(iSoftBitType);
}

void DabProcessor::set_dc_avoidance_algorithm(bool iUseDcAvoidanceAlgorithm)
{
  if (!iUseDcAvoidanceAlgorithm)
  {
    mFreqOffsSyncSymb += (f32)mFreqOffsRFHz;  // take RF offset to BB
    _set_bb_freq_offs_Hz(mFreqOffsSyncSymb);
    _set_rf_freq_offs_Hz(0.0f); // reset RF shift, sets mFreqOffsRFHz = 0
  }

  mRfFreqShiftUsed = false;
  mAllowRfFreqShift = iUseDcAvoidanceAlgorithm;
}

void DabProcessor::set_dc_and_iq_correction(const bool iDoDcCorr, const bool iDoIqCorr)
{
  mSampleReader.set_dc_and_iq_correction(iDoDcCorr, iDoIqCorr);
}

void DabProcessor::set_sync_on_strongest_peak(bool sync)
{
  mTiiDetector.reset();
  mPhaseReference.set_sync_on_strongest_peak(sync);
}

void DabProcessor::set_tii_processing(bool b)
{
  mEnableTii = b;
}

void DabProcessor::set_tii_collisions(bool b)
{
  mTiiDetector.set_detect_collisions(b);
}

void DabProcessor::set_tii_sub_id(u8 subid)
{
  mTiiDetector.set_subid_for_collision_search(subid);
}

void DabProcessor::set_tii_threshold(u8 trs)
{
  //fprintf(stderr, "tii_threshold = %d\n", trs);
  mTiiThreshold = trs;
}
