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
 *	DabProcessor is the embodying of all functionality related
 *	to the actual DAB processing.
 */
#include "dab-constants.h"
#include "sample-reader.h"
#include "phasereference.h"
#include "fic-handler.h"
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


// #define DO_TIME_MEAS

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

  //	inheriting from our delegates
  //	for the FicHandler:
  QString find_service(u32, i32);
  void get_parameters(const QString &, u32 *, i32 *);
  std::vector<SServiceId> get_services();
  bool is_audio_service(const QString & s);
  bool is_packet_service(const QString & s);
  void get_data_for_audio_service(const QString &, AudioData *);
  void get_data_for_packet_service(const QString &, PacketData *, i16);
  i32 get_sub_channel_id(const QString &, u32);
  u8 get_ecc();
  i32 get_ensemble_id();
  QString get_ensemble_name();
  u16 get_country_name();
  void set_epg_data(i32, i32, const QString &, const QString &);
  bool has_time_table(u32);
  std::vector<SEpgElement> find_epg_data(u32);
  u32 get_julian_date();
  QStringList basicPrint();
  i32 scan_width();
  void start_ficDump(FILE *);
  void stop_fic_dump();

  //	for the mscHandler
  void reset_services();
  void stop_service(DescriptorType *, i32);
  void stop_service(i32, i32);
  bool set_audio_channel(const AudioData * d, RingBuffer<i16> * b, FILE * dump, i32 flag);
  bool set_data_channel(PacketData *, RingBuffer<u8> *, i32);
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
  FicHandler mFicHandler;
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
  bool mCorrectionNeeded = true;
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
