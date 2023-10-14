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
#include  "dab-processor.h"
#include  "msc-handler.h"
#include  "radio.h"
#include  "process-params.h"

/**
  *	\brief DabProcessor
  *	The DabProcessor class is the driver of the processing
  *	of the samplestream.
  *	It is the main interface to the qt-dab program,
  *	local are classes OfdmDecoder, FicHandler and mschandler.
  */

DabProcessor::DabProcessor(RadioInterface * const mr, deviceHandler * const inputDevice, ProcessParams * const p) :
  mpRadioInterface(mr),
  mSampleReader(mr, inputDevice, p->spectrumBuffer),
  mFicHandler(mr, p->dabMode),
  mMscHandler(mr, p->dabMode, p->frameBuffer),
  mPhaseReference(mr, p),
  mTiiDetector(p->dabMode, p->tii_depth),
  mOfdmDecoder(mr, p->dabMode, p->iqBuffer, p->carrBuffer),
  mEtiGenerator(p->dabMode, &mFicHandler),
  mTimeSyncer(&mSampleReader),
  mcDabMode(p->dabMode),
  mcThreshold(p->threshold),
  mcTiiDelay(p->tii_delay),
  mDabPar(DabParams(p->dabMode).get_dab_par())
{
  //connect(this, &DabProcessor::signal_set_synced, mpRadioInterface, &RadioInterface::slot_set_synced);
  connect(this, &DabProcessor::signal_set_sync_lost, mpRadioInterface, &RadioInterface::slot_set_sync_lost);
  connect(this, &DabProcessor::signal_show_spectrum, mpRadioInterface, &RadioInterface::slot_show_spectrum);
  connect(this, &DabProcessor::signal_show_tii, mpRadioInterface, &RadioInterface::slot_show_tii);
  connect(this, &DabProcessor::signal_show_clock_err, mpRadioInterface, &RadioInterface::slot_show_clock_error);
  connect(this, &DabProcessor::signal_set_and_show_freq_corr_rf_Hz, mpRadioInterface, &RadioInterface::slot_set_and_show_freq_corr_rf_Hz);
  connect(this, &DabProcessor::signal_show_freq_corr_bb_Hz, mpRadioInterface, &RadioInterface::slot_show_freq_corr_bb_Hz);

  mOfdmBuffer.resize(2 * mDabPar.T_s);
  mBits.resize(2 * mDabPar.K);
  mTiiDetector.reset();
}

DabProcessor::~DabProcessor()
{
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

void DabProcessor::set_tiiDetectorMode(bool b)
{
  mTiiDetector.setMode(b);
}

void DabProcessor::start()
{
  mFicHandler.restart();
  if (!mScanMode)
  {
    mMscHandler.reset_Channel();
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
  float syncThreshold;
  int32_t startIndex;
  int32_t sampleCount = 0;
  mRfFreqShiftUsed = false;

  mSampleReader.setRunning(true);  // useful after a restart

  enum class EState
  {
    WAIT_FOR_TIME_SYNC_MARKER, EVAL_SYNC_SYMBOL, PROCESS_REST_OF_FRAME
  };

  EState state = EState::WAIT_FOR_TIME_SYNC_MARKER;

  try
  {
    // To get some idea of the signal strength
    for (int32_t i = 0; i < mDabPar.T_F / 5; i++)
    {
      mSampleReader.getSample(0);
    }

    while (true)
    {
      switch (state)
      {
      case EState::WAIT_FOR_TIME_SYNC_MARKER:
      {
        _set_rf_freq_offs_Hz(0.0f);
        _set_bb_freq_offs_Hz(0.0f);

        startIndex = 0;
        mCorrectionNeeded = true;
        mFreqOffsCylcPrefHz = 0.0f;
        mFreqOffsSyncSymb = 0.0f;
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
        const bool ok = _state_eval_sync_symbol(startIndex, sampleCount, syncThreshold);
        state = (ok ? EState::PROCESS_REST_OF_FRAME : EState::WAIT_FOR_TIME_SYNC_MARKER);
        break;
      }

      case EState::PROCESS_REST_OF_FRAME:
      {
        _state_process_rest_of_frame(startIndex, sampleCount);
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

void DabProcessor::_state_process_rest_of_frame(const int32_t iStartIndex, int32_t & ioSampleCount)
{
  /*
   * The current OFDM symbol 0 in mOfdmBuffer is special in that it is used for fine time synchronization,
   * for coarse frequency synchronization and its content is used as a reference for decoding the first datablock.
   */

  mOfdmDecoder.store_reference_symbol_0(mOfdmBuffer);

  // Here we look only at the symbol 0 when we need a coarse frequency synchronization.
  mCorrectionNeeded = !mFicHandler.syncReached();

  if (mCorrectionNeeded)
  {
    const int32_t correction = mPhaseReference.estimate_carrier_offset_from_sync_symbol_0(mOfdmBuffer);

    if (correction != PhaseReference::IDX_NOT_FOUND)
    {
      mFreqOffsSyncSymb += 0.4f * (float)correction * (float)mDabPar.CarrDiff;

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
      mFreqOffsSyncSymb = mFreqOffsCylcPrefHz = 0; // allow collect new remaining freq. shift
    }
  }

  mPhaseOffsetCyclPrefRad = _process_ofdm_symbols_1_to_L(ioSampleCount);

  _process_null_symbol(ioSampleCount);

  // The first sample to be found for the next frame should be T_g samples ahead. Before going for the next frame, we we just check the fineCorrector
  // We integrate the newly found frequency error with the existing frequency error.
  limit_symmetrically(mPhaseOffsetCyclPrefRad, 20.0f * F_RAD_PER_DEG);
  mFreqOffsCylcPrefHz += 1.00f * mPhaseOffsetCyclPrefRad / F_2_M_PI * (float)mDabPar.CarrDiff; // formerly 0.05

  _set_bb_freq_offs_Hz(mFreqOffsSyncSymb + mFreqOffsCylcPrefHz);

  mClockOffsetTotalSamples += ioSampleCount;

  if (++mClockOffsetFrameCount > 100)
  {
    emit signal_show_clock_err(mClockOffsetTotalSamples - mClockOffsetFrameCount * mDabPar.T_F); // samples per 100 frames
    mClockOffsetTotalSamples = 0;
    mClockOffsetFrameCount = 0;
  }
}

void DabProcessor::_process_null_symbol(int32_t & ioSampleCount)
{
  // We are at the end of the frame and we read the next T_n null samples
  mSampleReader.getSamples(mOfdmBuffer, 0, mDabPar.T_n, mFreqOffsBBHz);
  ioSampleCount += mDabPar.T_n;

  // is this null symbol with TII?
  const bool isTiiNullSegment = (mcDabMode == 1 && (mFicHandler.get_CIFcount() & 0x7) >= 4);

  if (!isTiiNullSegment) // eval SNR only in non-TII null segments
  {
    mOfdmDecoder.store_null_symbol_without_tii(mOfdmBuffer); // for SNR evaluation
  }
  else // this is TII null segment
  {
    mOfdmDecoder.store_null_symbol_with_tii(mOfdmBuffer); // for displaying TII

    // The TII data is encoded in the null period of the	odd frames
    mTiiDetector.addBuffer(mOfdmBuffer);

    if (++mTiiCounter >= mcTiiDelay)
    {
      uint16_t res = mTiiDetector.processNULL();
      if (res != 0)
      {
        uint8_t mainId = res >> 8;
        uint8_t subId = res & 0xFF;
        emit signal_show_tii(mainId, subId);
      }
      mTiiCounter = 0;
      mTiiDetector.reset();
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

  for (int16_t ofdmSymbCntIdx = 1; ofdmSymbCntIdx < mDabPar.L; ofdmSymbCntIdx++)
  {
    mSampleReader.getSamples(mOfdmBuffer, 0, mDabPar.T_s, mFreqOffsBBHz);
    ioSampleCount += mDabPar.T_s;

    for (int32_t i = mDabPar.T_u; i < mDabPar.T_s; i++)
    {
      freqCorr += mOfdmBuffer[i] * conj(mOfdmBuffer[i - mDabPar.T_u]); // eval phase shift in cyclic prefix part
    }

    mOfdmDecoder.decode_symbol(mOfdmBuffer, ofdmSymbCntIdx, mPhaseOffsetCyclPrefRad, mBits);

    if (ofdmSymbCntIdx <= 3)
    {
      mFicHandler.process_ficBlock(mBits, ofdmSymbCntIdx);
    }

    if (mEti_on)
    {
      mEtiGenerator.processBlock(mBits, ofdmSymbCntIdx);
    }

    if (ofdmSymbCntIdx > 3 && !mScanMode)
    {
      mMscHandler.process_mscBlock(mBits, ofdmSymbCntIdx);
    }
  }

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

bool DabProcessor::_state_eval_sync_symbol(int32_t & oStartIndex, int32_t & oSampleCount, float iThreshold)
{
  // get first OFDM symbol after time sync marker
  mSampleReader.getSamples(mOfdmBuffer, 0, mDabPar.T_u, mFreqOffsBBHz);

  oStartIndex = mPhaseReference.correlate_with_phase_ref_and_find_max_peak(mOfdmBuffer, iThreshold);

  if (oStartIndex < 0)
  {
    // no sync, try again
    if (!mCorrectionNeeded)
    {
      emit signal_set_sync_lost();
    }
    return false;
  }
  else
  {
    // Once here, we are synchronized, we need to copy the data we used for synchronization for symbol 0
    const int32_t nextOfdmBufferIdx = mDabPar.T_u - oStartIndex;
    assert(nextOfdmBufferIdx >= 0);

    // move/read OFDM symbol 0 which contains the synchronization data
    memmove(mOfdmBuffer.data(), &(mOfdmBuffer[oStartIndex]), nextOfdmBufferIdx * sizeof(cmplx)); // memmove can move overlapping segments correctly
    mSampleReader.getSamples(mOfdmBuffer, nextOfdmBufferIdx, mDabPar.T_u - nextOfdmBufferIdx, mFreqOffsBBHz); // get reference symbol

    oSampleCount = oStartIndex + mDabPar.T_u;

    emit signal_set_synced(true);

    return true;
  }
}

bool DabProcessor::_state_wait_for_time_sync_marker()
{
  emit signal_set_synced(false);
  mTiiDetector.reset();
  mOfdmDecoder.reset();

  switch (mTimeSyncer.read_samples_until_end_of_level_drop(mDabPar.T_n, mDabPar.T_F))
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

void DabProcessor::set_scanMode(bool b)
{
  mScanMode = b;
}

//	just convenience functions
//	FicHandler abstracts channel data

QString DabProcessor::findService(uint32_t SId, int SCIds)
{
  return mFicHandler.findService(SId, SCIds);
}

void DabProcessor::getParameters(const QString & s, uint32_t * p_SId, int * p_SCIds)
{
  mFicHandler.getParameters(s, p_SId, p_SCIds);
}

std::vector<serviceId> DabProcessor::getServices(int n)
{
  return mFicHandler.getServices(n);
}

int DabProcessor::getSubChId(const QString & s, uint32_t SId)
{
  return mFicHandler.getSubChId(s, SId);
}

bool DabProcessor::is_audioService(const QString & s)
{
  Audiodata ad;
  mFicHandler.dataforAudioService(s, &ad);
  return ad.defined;
}

bool DabProcessor::is_packetService(const QString & s)
{
  Packetdata pd;
  mFicHandler.dataforPacketService(s, &pd, 0);
  return pd.defined;
}

void DabProcessor::dataforAudioService(const QString & s, Audiodata * d)
{
  mFicHandler.dataforAudioService(s, d);
}

void DabProcessor::dataforPacketService(const QString & s, Packetdata * pd, int16_t compnr)
{
  mFicHandler.dataforPacketService(s, pd, compnr);
}

uint8_t DabProcessor::get_ecc()
{
  return mFicHandler.get_ecc();
}

[[maybe_unused]] uint16_t DabProcessor::get_countryName()
{
  return mFicHandler.get_countryName();
}

int32_t DabProcessor::get_ensembleId()
{
  return mFicHandler.get_ensembleId();
}

[[maybe_unused]] QString DabProcessor::get_ensembleName()
{
  return mFicHandler.get_ensembleName();
}

void DabProcessor::set_epgData(int SId, int32_t theTime, const QString & s, const QString & d)
{
  mFicHandler.set_epgData(SId, theTime, s, d);
}

bool DabProcessor::has_timeTable(uint32_t SId)
{
  return mFicHandler.has_timeTable(SId);
}

std::vector<EpgElement> DabProcessor::find_epgData(uint32_t SId)
{
  return mFicHandler.find_epgData(SId);
}

QStringList DabProcessor::basicPrint()
{
  return mFicHandler.basicPrint();
}

int DabProcessor::scanWidth()
{
  return mFicHandler.scanWidth();
}

//
//	for the mscHandler:
[[maybe_unused]] void DabProcessor::reset_Services()
{
  if (!mScanMode)
  {
    mMscHandler.reset_Channel();
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

bool DabProcessor::set_audioChannel(Audiodata * d, RingBuffer<int16_t> * b, FILE * dump, int flag)
{
  if (!mScanMode)
  {
    return mMscHandler.set_Channel(d, b, (RingBuffer<uint8_t> *)nullptr, dump, flag);
  }
  else
  {
    return false;
  }
}

bool DabProcessor::set_dataChannel(Packetdata * d, RingBuffer<uint8_t> * b, int flag)
{
  if (!mScanMode)
  {
    return mMscHandler.set_Channel(d, (RingBuffer<int16_t> *)nullptr, b, nullptr, flag);
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

void DabProcessor::stopDumping()
{
  mSampleReader.stopDumping();
}

void DabProcessor::start_ficDump(FILE * f)
{
  mFicHandler.start_ficDump(f);
}

void DabProcessor::stop_ficDump()
{
  mFicHandler.stop_ficDump();
}

uint32_t DabProcessor::julianDate()
{
  return mFicHandler.julianDate();
}

bool DabProcessor::start_etiGenerator(const QString & s)
{
  if (mEtiGenerator.start_etiGenerator(s))
  {
    mEti_on = true;
  }
  return mEti_on;
}

[[maybe_unused]] void DabProcessor::stop_etiGenerator()
{
  mEtiGenerator.stop_etiGenerator();
  mEti_on = false;
}

[[maybe_unused]] void DabProcessor::reset_etiGenerator()
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
    _set_rf_freq_offs_Hz(0.0f); // reset RF shift
  }

  mRfFreqShiftUsed = false;
  mAllowRfFreqShift = iUseDcAvoidanceAlgorithm;
}
