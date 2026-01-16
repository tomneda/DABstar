/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2015 .. 2020
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
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef  DAB_PROCESSOR_H
#define  DAB_PROCESSOR_H
/*
 *  DabProcessor is the embodying of all functionality related
 *  to the actual DAB processing.
 */
#include "dab-constants.h"
#include "sample-reader.h"
#include "phasereference.h"
#include "fic-decoder.h"
#include "msc-handler.h"
#include "device-handler.h"
#include "ringbuffer.h"
#include "tii-detector.h"
#include "timesyncer.h"
#include <fftw3.h>
#include <QThread>
#include <QObject>
#include <QStringList>
#include <vector>
#include <cstdint>
#include <memory>

#ifdef HAVE_SSE_OR_AVX
  #include "ofdm-decoder-simd.h"
#else
  #include "ofdm-decoder.h"
#endif


//#define DO_TIME_MEAS

#ifdef DO_TIME_MEAS
  #include "time_meas.h"
#endif

class DabRadio;
class DabParams;
class ProcessParams;
class EtiGenerator;

class DabProcessor : public QThread
{
Q_OBJECT
public:
  DabProcessor(DabRadio * mr, IDeviceHandler * inputDevice, ProcessParams * p);
  ~DabProcessor() override;

  inline IFibDecoder * get_fib_decoder() const { return mpFibDecoder; };

  void start();
  void stop();
  void startDumping(SNDFILE *);
  void stop_dumping();
  bool start_eti_generator(const QString &);
  void stop_eti_generator();
  void reset_eti_generator();
  void set_scan_mode(bool);
  void add_bb_freq(i32 iFreqOffHz);
  void activate_cir_viewer(bool iActivate);

  void start_fic_dump(FILE *);
  void stop_fic_dump();

  // for the MscHandler
  void reset_services();
  bool is_service_running(const SDescriptorType & iDT, EProcessFlag iProcessFlag);
  bool is_service_running(i32 iSubChId, EProcessFlag iProcessFlag);
  void stop_service(const SDescriptorType & iDT, EProcessFlag iProcessFlag);
  void stop_service(i32 iSubChId, EProcessFlag iProcessFlag);
  void stop_all_services();
  bool set_audio_channel(const SAudioData & iAD, RingBuffer<i16> * ipoAudioBuffer, EProcessFlag iProcessFlag);
  bool set_data_channel(const SPacketData & iPD, RingBuffer<u8> *, EProcessFlag iProcessFlag);

  void set_sync_on_strongest_peak(bool);
  void set_dc_avoidance_algorithm(bool iUseDcAvoidanceAlgorithm);
  void set_dc_removal(bool iRemoveDC);
  void set_tii_processing(bool);
  void set_tii_threshold(u8);
  void set_tii_sub_id(u8);
  void set_tii_collisions(bool);

private:
  DabRadio * const mpRadioInterface;
  SampleReader mSampleReader;
  FicDecoder mFicHandler;
  IFibDecoder * const mpFibDecoder;
  MscHandler mMscHandler;
  PhaseReference mPhaseReference;
  TiiDetector mTiiDetector{};
  OfdmDecoder mOfdmDecoder;
  std::unique_ptr<EtiGenerator> mpEtiGenerator; // seldom needed and takes more memory, instance it only on dememand
  std::atomic<bool> mEtiOn{false};
  TimeSyncer mTimeSyncer;
  const f32 mcThreshold;
  const i16 mcTiiFramesToCount;
  u8 mTiiThreshold;
  bool mScanMode = false;
  i16 mTiiCounter = 0;
  bool mEnableTii = false;
  f32 mPhaseOffsetCyclPrefRad = 0.0f;
  f32 mFreqOffsCylcPrefHz = 0.0f;
  f32 mFreqOffsSyncSymb = 0.0f;
  i32 mFreqOffsBBAddedHz = 0;
  i32 mFreqOffsBBHz = 0;
  i32 mFreqOffsRFHz = 0;
  i32 mTimeSyncAttemptCount = 0;
  i32 mClockOffsetTotalSamples = 0;
  i32 mClockOffsetFrameCount = 0;
  bool mRfFreqShiftUsed = false;
  bool mAllowRfFreqShift = false;
  bool mInputOverdrivenShown = false;
  f32 mClockErrHz = 0.0f;

  RingBuffer<cf32> * mpCirBuffer = nullptr;

  alignas(64) TArrayTn mOfdmBuffer;
  alignas(64) TArrayTu mFftInBuffer;
  alignas(64) TArrayTu mFftOutBuffer;
  fftwf_plan mFftPlan;
  std::vector<i16> mBits;

#ifdef DO_TIME_MEAS
  TimeMeas mTimeMeas{"decode_symbol", 1000};
#endif

  void run() override; // the new QThread

  bool _state_wait_for_time_sync_marker();
  bool _state_eval_sync_symbol(i32 & oSampleCount, f32 iThreshold);
  void _state_process_rest_of_frame(i32 & ioSampleCount);
  f32 _process_ofdm_symbols_1_to_L(i32 & ioSampleCount);
  void _process_null_symbol(i32 & ioSampleCount);
  void _set_rf_freq_offs_Hz(f32 iFreqHz);
  void _set_bb_freq_offs_Hz(f32 iFreqHz);

public slots:
  void slot_select_carrier_plot_type(ECarrierPlotType iPlotType);
  void slot_select_iq_plot_type(EIqPlotType iPlotType);
  void slot_show_nominal_carrier(bool iShowNominalCarrier);
  void slot_soft_bit_gen_type(ESoftBitType iSoftBitType);

signals:
  void signal_no_signal_found();
  void signal_show_tii(const std::vector<STiiResult> & iTr);
  void signal_show_spectrum(i32);
  void signal_show_clock_err(f32);
  void signal_set_and_show_freq_corr_rf_Hz(i32);
  void signal_show_freq_corr_bb_Hz(i32);
  void signal_linear_peak_level(f32);
};

#endif
