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
#include "eti-generator.h"
#include "timesyncer.h"
#include <fftw3.h>
#include <QThread>
#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <vector>
#include <cstdint>
#include <sndfile.h>

#ifdef HAVE_SSE_OR_AVX
  #include "ofdm-decoder-simd.h"
#else
  #include "ofdm-decoder.h"
#endif


// #define DO_TIME_MEAS

#ifdef DO_TIME_MEAS
  #include "time_meas.h"
#endif

class RadioInterface;
class DabParams;
class ProcessParams;

class DabProcessor : public QThread
{
Q_OBJECT
public:
  DabProcessor(RadioInterface * mr, IDeviceHandler * inputDevice, ProcessParams * p);
  ~DabProcessor() override;

  void start();
  void stop();
  void startDumping(SNDFILE *);
  void stop_dumping();
  bool start_eti_generator(const QString &);
  void stop_eti_generator();
  void reset_eti_generator();
  void set_scan_mode(bool);
  void add_bb_freq(int32_t iFreqOffHz);

  //	inheriting from our delegates
  //	for the FicHandler:
  QString find_service(uint32_t, int);
  void get_parameters(const QString &, uint32_t *, int *);
  std::vector<SServiceId> get_services();
  bool is_audio_service(const QString & s);
  bool is_packet_service(const QString & s);
  void get_data_for_audio_service(const QString &, Audiodata *);
  void get_data_for_packet_service(const QString &, Packetdata *, int16_t);
  int get_sub_channel_id(const QString &, uint32_t);
  uint8_t get_ecc();
  int32_t get_ensemble_id();
  QString get_ensemble_name();
  uint16_t get_country_name();
  void set_epg_data(int32_t, int32_t, const QString &, const QString &);
  bool has_time_table(uint32_t);
  std::vector<SEpgElement> find_epg_data(uint32_t);
  uint32_t get_julian_date();
  QStringList basicPrint();
  int scan_width();
  void start_ficDump(FILE *);
  void stop_fic_dump();

  //	for the mscHandler
  void reset_services();
  void stop_service(DescriptorType *, int);
  void stop_service(int, int);
  bool set_audio_channel(const Audiodata * d, RingBuffer<int16_t> * b, FILE * dump, int flag);
  bool set_data_channel(Packetdata *, RingBuffer<uint8_t> *, int);
  void set_sync_on_strongest_peak(bool);
  void set_dc_avoidance_algorithm(bool iUseDcAvoidanceAlgorithm);
  void set_dc_removal(bool iRemoveDC);
  void set_tii(bool);
  void set_tii_threshold(uint8_t);
  void set_tii_sub_id(uint8_t);
  void set_tii_collisions(bool);

private:
  RadioInterface * const mpRadioInterface;
  SampleReader mSampleReader;
  FicHandler mFicHandler;
  MscHandler mMscHandler;
  PhaseReference mPhaseReference;
  TiiDetector mTiiDetector;
  OfdmDecoder mOfdmDecoder;
  etiGenerator mEtiGenerator;
  TimeSyncer mTimeSyncer;
  const uint8_t mcDabMode;
  const float mcThreshold;
  const int16_t mcTiiDelay;
  uint8_t mTiiThreshold;
  const DabParams::SDabPar mDabPar;
  bool mScanMode = false;
  int16_t mTiiCounter = 0;
  bool mEti_on = false;
  bool mEnableTii = false;
  bool mCorrectionNeeded = true;
  float mPhaseOffsetCyclPrefRad = 0.0f;
  float mFreqOffsCylcPrefHz = 0.0f;
  float mFreqOffsSyncSymb = 0.0f;
  int32_t mFreqOffsBBAddedHz = 0;
  int32_t mFreqOffsBBHz = 0;
  int32_t mFreqOffsRFHz = 0;
  int32_t mTimeSyncAttemptCount = 0;
  int32_t mClockOffsetTotalSamples = 0;
  int32_t mClockOffsetFrameCount = 0;
  bool mRfFreqShiftUsed = false;
  bool mAllowRfFreqShift = false;
  bool mInputOverdrivenShown = false;

  std::vector<cmplx> mOfdmBuffer;
  std::vector<int16_t> mBits;

  std::vector<cmplx> mFftInBuffer;
  std::vector<cmplx> mFftOutBuffer;
  fftwf_plan mFftPlan;

#ifdef DO_TIME_MEAS
  TimeMeas mTimeMeas{"decode_symbol", 1000};
#endif

  void run() override; // the new QThread

  bool _state_wait_for_time_sync_marker();
  bool _state_eval_sync_symbol(int32_t & oSampleCount, float iThreshold);
  void _state_process_rest_of_frame(int32_t & ioSampleCount);
  float _process_ofdm_symbols_1_to_L(int32_t & ioSampleCount);
  void _process_null_symbol(int32_t & ioSampleCount);
  void _set_rf_freq_offs_Hz(float iFreqHz);
  void _set_bb_freq_offs_Hz(float iFreqHz);

public slots:
  void slot_select_carrier_plot_type(ECarrierPlotType iPlotType);
  void slot_select_iq_plot_type(EIqPlotType iPlotType);
  void slot_show_nominal_carrier(bool iShowNominalCarrier);
  void slot_soft_bit_gen_type(ESoftBitType iSoftBitType);

signals:
  void signal_no_signal_found();
  void signal_show_tii(const std::vector<STiiResult> & iTr);
  void signal_show_spectrum(int);
  void signal_show_clock_err(float);
  void signal_set_and_show_freq_corr_rf_Hz(int);
  void signal_show_freq_corr_bb_Hz(int);
  void signal_linear_peak_level(float);
};

#endif
