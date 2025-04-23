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

/**
  *	\brief DabProcessor
  *	The DabProcessor class is the driver of the processing
  *	of the samplestream.
  *	It is the main interface to the qt-dab program,
  *	local are classes OfdmDecoder, FicHandler and mschandler.
  */

DabProcessor::DabProcessor(DabRadio * const mr, IDeviceHandler * const inputDevice, ProcessParams * const p)
  : mpRadioInterface(mr)
  , mSampleReader(mr, inputDevice, p->spectrumBuffer)
  , mFicHandler(mr)
  , mMscHandler(mr, p->frameBuffer)
  , mPhaseReference(mr, p)
  , mOfdmDecoder(mr, p->iqBuffer, p->carrBuffer)
  , mEtiGenerator(&mFicHandler)
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

  mBits.resize(2 * cK);
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
   *	\brief run
   *	The main thread, reading samples,
   *	time synchronization and frequency synchronization
   *	Identifying blocks in the DAB frame
   *	and sending them to the OfdmDecoder who will transfer the results
   *	Finally, estimating the small freqency error
   */
void DabProcessor::run()  // run QThread
{
  // this method is called with each new set frequency
  float syncThreshold;
  int32_t sampleCount = 0;
  mRfFreqShiftUsed = false;

  mSampleReader.setRunning(true);  // useful after a restart

  enum class EState
  {
    WAIT_FOR_TIME_SYNC_MARKER, EVAL_SYNC_SYMBOL, PROCESS_REST_OF_FRAME
  };

  _set_rf_freq_offs_Hz(0.0f);
  _set_bb_freq_offs_Hz(0.0f);

  mCorrectionNeeded = true;
  mFicHandler.reset_fic_decode_success_ratio(); // mCorrectionNeeded will be true next call to getFicDecodeRatioPercent()
  mFreqOffsCylcPrefHz = 0.0f;
  mFreqOffsSyncSymb = 0.0f;

  EState state = EState::WAIT_FOR_TIME_SYNC_MARKER;

  try
  {
    // To get some idea of the signal strength
    for (int32_t i = 0; i < cTF / 5; i++)
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
  catch (int e)
  {
    (void)e;
    //fprintf(stderr, "Caught exception in DabProcessor: %d\n", e);
    fprintf(stdout, "DabProcessor has stopped\n");  // TODO: find a nicer way stopping the DAB processor
  }
}

void DabProcessor::_state_process_rest_of_frame(int32_t & ioSampleCount)
{
  /*
   * The current OFDM symbol 0 in mOfdmBuffer is special in that it is used for fine time synchronization,
   * for coarse frequency synchronization and its content is used as a reference for decoding the first datablock.
   */

  // memcpy() is considerable faster than std::copy on my i7-6700K (nearly twice as fast for size == 2048)
  memcpy(mFftInBuffer.data(), mOfdmBuffer.data(), mFftInBuffer.size() * sizeof(cmplx));

  fftwf_execute(mFftPlan);
  mOfdmDecoder.store_reference_symbol_0(mFftOutBuffer);

  // Here we look only at the symbol 0 when we need a coarse frequency synchronization.
  mCorrectionNeeded = mFicHandler.get_fic_decode_ratio_percent() < 50;

  if (mCorrectionNeeded)
  {
    const int32_t correction = mPhaseReference.estimate_carrier_offset_from_sync_symbol_0(mFftOutBuffer);

    if (correction != PhaseReference::IDX_NOT_FOUND)
    {
      mFreqOffsSyncSymb += (float)correction * (float)cCarrDiff;

      if (std::abs(mFreqOffsSyncSymb) > kHz(35))
      {
        mFreqOffsSyncSymb = 0.0f;
      }
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
  mFreqOffsCylcPrefHz += mPhaseOffsetCyclPrefRad / F_2_M_PI * (float)cCarrDiff; // formerly 0.05

  _set_bb_freq_offs_Hz(mFreqOffsSyncSymb + mFreqOffsCylcPrefHz);

  mClockOffsetTotalSamples += ioSampleCount;

  if (++mClockOffsetFrameCount > 10) // about each second
  {
    const float clockErrHz = INPUT_RATE * ((float)mClockOffsetTotalSamples / ((float)mClockOffsetFrameCount * (float)cTF) - 1.0f);
    emit signal_show_clock_err(clockErrHz);
    mClockOffsetTotalSamples = 0;
    mClockOffsetFrameCount = 0;

    const float peakLevel = mSampleReader.get_linear_peak_level_and_clear();
    emit signal_linear_peak_level(peakLevel);
  }
}

void DabProcessor::_process_null_symbol(int32_t & ioSampleCount)
{
  // We are at the end of the frame and we read the next T_n null samples
  mSampleReader.getSamples(mOfdmBuffer, 0, cTn, mFreqOffsBBHz, false);
  ioSampleCount += cTn;

  // is this null symbol with TII?
  const bool isTiiNullSegment = ((mFicHandler.get_cif_count() & 0x7) >= 4);
  memcpy(mFftInBuffer.data(), &(mOfdmBuffer[cTg]), cTu * sizeof(cmplx));
  fftwf_execute(mFftPlan);

  if (!isTiiNullSegment) // eval SNR only in non-TII null segments
  {
    mOfdmDecoder.store_null_symbol_without_tii(mFftOutBuffer); // for SNR evaluation
  }
  else // this is TII null segment
  {
    mOfdmDecoder.store_null_symbol_with_tii(mFftOutBuffer); // for displaying TII

    // The TII data is encoded in the null period of the	odd frames
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

float DabProcessor::_process_ofdm_symbols_1_to_L(int32_t & ioSampleCount)
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
  * and the	corresponding samples in the datapart.
  */
  cmplx freqCorr = cmplx(0, 0);

  for (int16_t ofdmSymbCntIdx = 1; ofdmSymbCntIdx < cL; ofdmSymbCntIdx++)
  {
    mSampleReader.getSamples(mOfdmBuffer, 0, cTs, mFreqOffsBBHz, true);
    ioSampleCount += cTs;

    for (int32_t i = cTu; i < cTs; i++)
    {
      freqCorr += mOfdmBuffer[i] * conj(mOfdmBuffer[i - cTu]); // eval phase shift in cyclic prefix part
    }

    mOfdmDecoder.set_dc_offset(mSampleReader.get_dc_offset());
    memcpy(mFftInBuffer.data(), &(mOfdmBuffer[cTg]), cTu * sizeof(cmplx));
    fftwf_execute(mFftPlan);
#ifdef DO_TIME_MEAS
    mTimeMeas.trigger_begin();
#endif
    mOfdmDecoder.decode_symbol(mFftOutBuffer, ofdmSymbCntIdx, mPhaseOffsetCyclPrefRad, mBits);
#ifdef DO_TIME_MEAS
    mTimeMeas.trigger_end();
#endif

    if (ofdmSymbCntIdx <= 3)
    {
      mFicHandler.process_block(mBits, ofdmSymbCntIdx);
    }

    if (mEti_on)
    {
      mEtiGenerator.process_block(mBits, ofdmSymbCntIdx);
    }

    if (ofdmSymbCntIdx > 3 && !mScanMode)
    {
      mMscHandler.process_block(mBits, ofdmSymbCntIdx);
    }
  }
#ifdef DO_TIME_MEAS
  mTimeMeas.print_time_per_round();
#endif

  return arg(freqCorr);
}

void DabProcessor::_set_bb_freq_offs_Hz(float iFreqHz)
{
  const int32_t iFreqHzInt = (int32_t)std::round(iFreqHz);
  if (mFreqOffsBBHz != iFreqHzInt)
  {
    mFreqOffsBBHz = iFreqHzInt;
    emit signal_show_freq_corr_bb_Hz(mFreqOffsBBHz);
  }
}

void DabProcessor::_set_rf_freq_offs_Hz(float iFreqHz)
{
  const int32_t iFreqHzInt = (int32_t)std::round(iFreqHz);
  if (mFreqOffsRFHz != iFreqHzInt)
  {
    mFreqOffsRFHz = iFreqHzInt;
    emit signal_set_and_show_freq_corr_rf_Hz(mFreqOffsRFHz);   // this call changes the RF frequency and display! (hopefully fast enough)
  }
}

bool DabProcessor::_state_eval_sync_symbol(int32_t & oSampleCount, float iThreshold)
{
  // get first OFDM symbol after time sync marker
  mSampleReader.getSamples(mOfdmBuffer, 0, cTu, mFreqOffsBBHz, false);

  const int32_t startIndex = mPhaseReference.correlate_with_phase_ref_and_find_max_peak(mOfdmBuffer, iThreshold);

  if (startIndex < 0)
  {
    // no sync, try again
    return false;
  }
  else
  {
    // Once here, we are synchronized, we need to copy the data we used for synchronization for symbol 0
    const int32_t nextOfdmBufferIdx = cTu - startIndex;
    assert(nextOfdmBufferIdx >= 0);

    // move/read OFDM symbol 0 which contains the synchronization data
    memmove(mOfdmBuffer.data(), &(mOfdmBuffer[startIndex]), nextOfdmBufferIdx * sizeof(cmplx)); // memmove can move overlapping segments correctly
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

  switch (mTimeSyncer.read_samples_until_end_of_level_drop(cTn, cTF))
  {
  case TimeSyncer::EState::TIMESYNC_ESTABLISHED:
    return true;
  case TimeSyncer::EState::NO_DIP_FOUND:
    if (++mTimeSyncAttemptCount >= 8)
    {
      emit signal_no_signal_found();
      mTimeSyncAttemptCount = 0;
    }
    break;
  case TimeSyncer::EState::NO_END_OF_DIP_FOUND:
    break;
  }
  return false;
}

void DabProcessor::set_scan_mode(bool b)
{
  mScanMode = b;
}

//	just convenience functions
//	FicHandler abstracts channel data

void DabProcessor::activate_cir_viewer(bool iActivate)
{
  mSampleReader.set_cir_buffer(iActivate ? mpCirBuffer : nullptr);
}

QString DabProcessor::find_service(uint32_t SId, int SCIds)
{
  return mFicHandler.find_service(SId, SCIds);
}

void DabProcessor::get_parameters(const QString & s, uint32_t * p_SId, int * p_SCIds)
{
  mFicHandler.get_parameters(s, p_SId, p_SCIds);
}

std::vector<SServiceId> DabProcessor::get_services()
{
  return mFicHandler.get_services();
}

int DabProcessor::get_sub_channel_id(const QString & s, uint32_t SId)
{
  return mFicHandler.get_sub_channel_id(s, SId);
}

bool DabProcessor::is_audio_service(const QString & s)
{
  Audiodata ad;
  mFicHandler.get_data_for_audio_service(s, &ad);
  return ad.defined;
}

bool DabProcessor::is_packet_service(const QString & s)
{
  Packetdata pd;
  mFicHandler.get_data_for_packet_service(s, &pd, 0);
  return pd.defined;
}

void DabProcessor::get_data_for_audio_service(const QString & iS, Audiodata * opAD)
{
  mFicHandler.get_data_for_audio_service(iS, opAD);
}

void DabProcessor::get_data_for_packet_service(const QString & iS, Packetdata * opPD, int16_t iCompNr)
{
  mFicHandler.get_data_for_packet_service(iS, opPD, iCompNr);
}

uint8_t DabProcessor::get_ecc()
{
  return mFicHandler.get_ecc();
}

[[maybe_unused]] uint16_t DabProcessor::get_country_name()
{
  return mFicHandler.get_country_name();
}

int32_t DabProcessor::get_ensemble_id()
{
  return mFicHandler.get_ensembleId();
}

[[maybe_unused]] QString DabProcessor::get_ensemble_name()
{
  return mFicHandler.get_ensemble_name();
}

void DabProcessor::set_epg_data(int SId, int32_t theTime, const QString & s, const QString & d)
{
  mFicHandler.set_epg_data(SId, theTime, s, d);
}

bool DabProcessor::has_time_table(uint32_t SId)
{
  return mFicHandler.has_time_table(SId);
}

std::vector<SEpgElement> DabProcessor::find_epg_data(uint32_t SId)
{
  return mFicHandler.find_epg_data(SId);
}

QStringList DabProcessor::basicPrint()
{
  return mFicHandler.basic_print();
}

int DabProcessor::scan_width()
{
  return mFicHandler.scan_width();
}

//	for the mscHandler:
[[maybe_unused]] void DabProcessor::reset_services()
{
  if (!mScanMode)
  {
    mMscHandler.reset_channel();
  }
}

[[maybe_unused]] void DabProcessor::stop_service(DescriptorType * d, int flag)
{
  fprintf(stderr, "function obsolete\n");
  if (!mScanMode)
  {
    mMscHandler.stop_service(d->subchId, flag);
  }
}

void DabProcessor::stop_service(int subChId, int flag)
{
  if (!mScanMode)
  {
    mMscHandler.stop_service(subChId, flag);
  }
}

bool DabProcessor::set_audio_channel(const Audiodata * d, RingBuffer<int16_t> * b, FILE * dump, int flag)
{
  if (!mScanMode)
  {
    return mMscHandler.set_channel(d, b, (RingBuffer<uint8_t>*)nullptr, dump, flag);
  }
  else
  {
    return false;
  }
}

bool DabProcessor::set_data_channel(Packetdata * d, RingBuffer<uint8_t> * b, int flag)
{
  if (!mScanMode)
  {
    return mMscHandler.set_channel(d, (RingBuffer<int16_t>*)nullptr, b, nullptr, flag);
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

void DabProcessor::start_ficDump(FILE * f)
{
  mFicHandler.start_fic_dump(f);
}

void DabProcessor::stop_fic_dump()
{
  mFicHandler.stop_fic_dump();
}

uint32_t DabProcessor::get_julian_date()
{
  return mFicHandler.get_julian_date();
}

bool DabProcessor::start_eti_generator(const QString & s)
{
  if (mEtiGenerator.start_eti_generator(s))
  {
    mEti_on = true;
  }
  return mEti_on;
}

[[maybe_unused]] void DabProcessor::stop_eti_generator()
{
  mEtiGenerator.stop_eti_generator();
  mEti_on = false;
}

[[maybe_unused]] void DabProcessor::reset_eti_generator()
{
  mEtiGenerator.reset();
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
    mFreqOffsSyncSymb += (float)mFreqOffsRFHz;  // take RF offset to BB
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

void DabProcessor::set_tii_sub_id(uint8_t subid)
{
  mTiiDetector.set_subid_for_collision_search(subid);
}

void DabProcessor::set_tii_threshold(uint8_t trs)
{
  //fprintf(stderr, "tii_threshold = %d\n", trs);
  mTiiThreshold = trs;
}
