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
#include "dab-processor.h"
#include "msc-handler.h"
#include "dabradio.h"
#include "process-params.h"
#include "eti-generator.h"

/**
  * \brief DabProcessor
  * The DabProcessor class is the driver of the processing
  * of the samplestream.
  * It is the main interface to the qt-dab program,
  * local are classes OfdmDecoder, FicHandler and mschandler.
  */

DabProcessor::DabProcessor(DabRadio * const mr, IDeviceHandler * const inputDevice, ProcessParams * const p)
  : mpRadioInterface(mr)
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
  mFftPlan = fftwf_plan_dft_1d(cTu, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_FORWARD, FFTW_ESTIMATE);

  connect(this, &DabProcessor::signal_show_spectrum, mpRadioInterface, &DabRadio::slot_show_spectrum);
  connect(this, &DabProcessor::signal_show_tii, mpRadioInterface, &DabRadio::slot_show_tii);
  connect(this, &DabProcessor::signal_show_clock_err, mpRadioInterface, &DabRadio::slot_show_clock_error);
  connect(this, &DabProcessor::signal_set_and_show_freq_corr_rf_Hz, mpRadioInterface, &DabRadio::slot_set_and_show_freq_corr_rf_Hz);
  connect(this, &DabProcessor::signal_show_freq_corr_bb_Hz, mpRadioInterface, &DabRadio::slot_show_freq_corr_bb_Hz);
  connect(this, &DabProcessor::signal_linear_peak_level, mpRadioInterface, &DabRadio::slot_show_digital_peak_level);

  mBits.resize(c2K);
  mTiiDetector.reset();
  mTiiCounter = 0;
}

DabProcessor::~DabProcessor()
{
  fftwf_destroy_plan(mFftPlan);
  if (isRunning())
  {
    mSampleReader.setRunning(false);
    // exception to be raised through the getSample(s) functions.
    msleep(100);
    while (isRunning())
    {
      usleep(100);
    }
  }
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
  mSampleReader.setRunning(false);
  while (isRunning())
  {
    wait();
  }
  usleep(10000);
  mFicHandler.stop();
}

/***
   *    \brief run
   *    The main thread, reading samples,
   *    time synchronization and frequency synchronization
   *    Identifying blocks in the DAB frame
   *    and sending them to the OfdmDecoder who will transfer the results
   *    Finally, estimating the small freqency error
   */
void DabProcessor::run()  // run QThread
{
  // this method is called with each new set frequency
  f32 syncThreshold;
  i32 sampleCount = 0;
  mRfFreqShiftUsed = false;

  mSampleReader.setRunning(true);  // useful after a restart

  enum class EState
  {
    WAIT_FOR_TIME_SYNC_MARKER, EVAL_SYNC_SYMBOL, PROCESS_REST_OF_FRAME
  };

  _set_rf_freq_offs_Hz(0.0f);
  _set_bb_freq_offs_Hz(0.0f);

  mFicHandler.reset_fic_decode_success_ratio(); // mCorrectionNeeded will be true next call to getFicDecodeRatioPercent()
  mFreqOffsCylcPrefHz = 0.0f;
  mFreqOffsSyncSymb = 0.0f;
  mTimeSyncAttemptCount = 0;

  EState state = EState::WAIT_FOR_TIME_SYNC_MARKER;

  try
  {
    // To get some idea of the signal strength
    for (i32 i = 0; i < cTF / 5; i++)
    {
      mSampleReader.getSample(0);
    }

    while (true)
    {
      switch (state)
      {
      case EState::WAIT_FOR_TIME_SYNC_MARKER:
      {
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
        syncThreshold = 3 * mcThreshold; // threshold is less sensitive while startup
        break;
      }
      } // switch
    } // while (true)
  }
  catch (i32 e)
  {
    (void)e;
    // qInfo() << "DabProcessor has stopped" << thread()->currentThreadId() << e;
  }
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
  if (mFicHandler.get_fic_decode_ratio_percent() < 50) // Correction needed ?
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

    _set_bb_freq_offs_Hz(mFreqOffsSyncSymb + mFreqOffsCylcPrefHz);
  }
  else
  {
    if (!mRfFreqShiftUsed && mAllowRfFreqShift)
    {
      mRfFreqShiftUsed = true;
      _set_rf_freq_offs_Hz(mFreqOffsSyncSymb + mFreqOffsCylcPrefHz); // takeover BB shift to RF
      _set_bb_freq_offs_Hz(0.0f); // no, no BB shift should be necessary
      mFreqOffsSyncSymb = mFreqOffsCylcPrefHz = 0.0f; // allow collect new remaining freq. shift
    }
  }

  mPhaseOffsetCyclPrefRad = _process_ofdm_symbols_1_to_L(ioSampleCount);

  _process_null_symbol(ioSampleCount);

  // The first sample to be found for the next frame should be T_g samples ahead. Before going for the next frame, we just check the fineCorrector
  // We integrate the newly found frequency error with the existing frequency error.
  limit_symmetrically(mPhaseOffsetCyclPrefRad, 20.0f * F_RAD_PER_DEG);
  mFreqOffsCylcPrefHz += mPhaseOffsetCyclPrefRad / F_2_M_PI * (f32)cCarrDiff; // formerly 0.05

  _set_bb_freq_offs_Hz(mFreqOffsSyncSymb + mFreqOffsCylcPrefHz);

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

    const f32 peakLevel = mSampleReader.get_linear_peak_level_and_clear();
    emit signal_linear_peak_level(peakLevel);
  }
}

void DabProcessor::_process_null_symbol(i32 & ioSampleCount)
{
  // We are at the end of the frame and we read the next T_n null samples
  mSampleReader.getSamples(mOfdmBuffer, 0, cTn, mFreqOffsBBHz, false);
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
    mSampleReader.getSamples(mOfdmBuffer, 0, cTs, mFreqOffsBBHz, true);
    ioSampleCount += cTs;

    for (i32 i = cTu; i < cTs; i++)
    {
      freqCorr += mOfdmBuffer[i] * conj(mOfdmBuffer[i - cTu]); // eval phase shift in cyclic prefix part
    }

    mOfdmDecoder.set_dc_offset(mSampleReader.get_dc_offset());
    memcpy(mFftInBuffer.data(), &(mOfdmBuffer[cTg]), cTu * sizeof(cf32));
    fftwf_execute(mFftPlan);
#ifdef DO_TIME_MEAS
    mTimeMeas.trigger_begin();
#endif
    mOfdmDecoder.decode_symbol(mFftOutBuffer, ofdmSymbIdx, mPhaseOffsetCyclPrefRad, mClockErrHz, mBits);
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

void DabProcessor::_set_bb_freq_offs_Hz(f32 iFreqHz)
{
  const i32 iFreqHzInt = (i32)std::round(iFreqHz);
  if (mFreqOffsBBHz != iFreqHzInt)
  {
    mFreqOffsBBHz = iFreqHzInt;
    emit signal_show_freq_corr_bb_Hz(mFreqOffsBBHz);
  }
}

void DabProcessor::_set_rf_freq_offs_Hz(f32 iFreqHz)
{
  const i32 iFreqHzInt = (i32)std::round(iFreqHz);
  if (mFreqOffsRFHz != iFreqHzInt)
  {
    mFreqOffsRFHz = iFreqHzInt;
    emit signal_set_and_show_freq_corr_rf_Hz(mFreqOffsRFHz);   // this call changes the RF frequency and display! (hopefully fast enough)
  }
}

bool DabProcessor::_state_eval_sync_symbol(i32 & oSampleCount, f32 iThreshold)
{
  // get first OFDM symbol after time sync marker
  mSampleReader.getSamples(mOfdmBuffer, 0, cTu, mFreqOffsBBHz, false);

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
    mSampleReader.getSamples(mOfdmBuffer, nextOfdmBufferIdx, cTu - nextOfdmBufferIdx, mFreqOffsBBHz, false); // get reference symbol

    oSampleCount = startIndex + cTu;
    return true;
  }
}

bool DabProcessor::_state_wait_for_time_sync_marker()
{
  mTiiDetector.reset();
  mOfdmDecoder.reset();
  mTiiCounter = 0;

  switch (mTimeSyncer.read_samples_until_end_of_level_drop())
  {
  case TimeSyncer::EState::TIMESYNC_ESTABLISHED:
    mTimeSyncAttemptCount = 0;
    return true;
  case TimeSyncer::EState::NO_DIP_FOUND:
    if (++mTimeSyncAttemptCount >= 8)
    {
      emit signal_no_signal_found();
      mTimeSyncAttemptCount = 0;
    }
    break;
  case TimeSyncer::EState::NO_END_OF_DIP_FOUND:
    mTimeSyncAttemptCount = 0;
    break;
  }
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

//  for the mscHandler:
void DabProcessor::reset_services()
{
  if (!mScanMode)
  {
    mMscHandler.reset_channel();
  }
}

bool DabProcessor::is_service_running(const SDescriptorType & iDT, EProcessFlag iProcessFlag)
{
  assert(!mScanMode);
  return mMscHandler.is_service_running(iDT.SubChId, iProcessFlag);
}

bool DabProcessor::is_service_running(const i32 iSubChId, EProcessFlag iProcessFlag)
{
  assert(!mScanMode);
  return mMscHandler.is_service_running(iSubChId, iProcessFlag);
}

void DabProcessor::stop_service(const SDescriptorType & iDT, const EProcessFlag iProcessFlag)
{
  fprintf(stderr, "function obsolete\n");
  if (!mScanMode)
  {
    mMscHandler.stop_service(iDT.SubChId, iProcessFlag);
  }
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

void DabProcessor::startDumping(SNDFILE * f)
{
  mSampleReader.startDumping(f);
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

void DabProcessor::slot_show_nominal_carrier(bool iShowNominalCarrier)
{
  mOfdmDecoder.set_show_nominal_carrier(iShowNominalCarrier);
}

void DabProcessor::set_dc_avoidance_algorithm(bool iUseDcAvoidanceAlgorithm)
{
  if (!iUseDcAvoidanceAlgorithm)
  {
    mFreqOffsSyncSymb += (f32)mFreqOffsRFHz;  // take RF offset to BB
    _set_bb_freq_offs_Hz(mFreqOffsSyncSymb + mFreqOffsCylcPrefHz);
    _set_rf_freq_offs_Hz(0.0f); // reset RF shift, sets mFreqOffsRFHz = 0
  }

  mRfFreqShiftUsed = false;
  mAllowRfFreqShift = iUseDcAvoidanceAlgorithm;
}

void DabProcessor::set_dc_removal(bool iRemoveDC)
{
  mSampleReader.set_dc_removal(iRemoveDC);
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
