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
#include  "dab-constants.h"
#include  <QThread>
#include  <QObject>
#include  <QByteArray>
#include  <QStringList>
#include  <vector>
#include  <cstdint>
#include  <sndfile.h>
#include  "sample-reader.h"
#include  "phasereference.h"
#include  "ofdm-decoder.h"
#include  "fic-handler.h"
#include  "msc-handler.h"
#include  "device-handler.h"
#include  "ringbuffer.h"
#include  "tii-detector.h"
#include  "eti-generator.h"
#include "timesyncer.h"

class RadioInterface;
class DabParams;
class ProcessParams;

class DabProcessor : public QThread
{
Q_OBJECT
public:
  DabProcessor(RadioInterface * mr, deviceHandler * inputDevice, ProcessParams * p);
  ~DabProcessor() override;

  void start();
  void stop();
  void startDumping(SNDFILE *);
  void stopDumping();
  bool start_etiGenerator(const QString &);
  void stop_etiGenerator();
  void reset_etiGenerator();
  void set_scanMode(bool);

  //	inheriting from our delegates
  //	for the FicHandler:
  QString findService(uint32_t, int);
  void getParameters(const QString &, uint32_t *, int *);
  std::vector<serviceId> getServices(int);
  bool is_audioService(const QString & s);
  bool is_packetService(const QString & s);
  void dataforAudioService(const QString &, Audiodata *);
  void dataforPacketService(const QString &, Packetdata *, int16_t);
  int getSubChId(const QString &, uint32_t);
  uint8_t get_ecc();
  int32_t get_ensembleId();
  QString get_ensembleName();
  uint16_t get_countryName();
  void set_epgData(int32_t, int32_t, const QString &, const QString &);
  bool has_timeTable(uint32_t);
  std::vector<EpgElement> find_epgData(uint32_t);
  uint32_t julianDate();
  QStringList basicPrint();
  int scanWidth();
  void start_ficDump(FILE *);
  void stop_ficDump();

  //	for the mscHandler
  void reset_Services();
  void stop_service(DescriptorType *, int);
  void stop_service(int, int);
  bool set_audioChannel(Audiodata *, RingBuffer<int16_t> *, FILE *, int);
  bool set_dataChannel(Packetdata *, RingBuffer<uint8_t> *, int);
  void set_tiiDetectorMode(bool);
  void set_dc_avoidance_algorithm(bool iUseDcAvoidanceAlgorithm);

private:
  RadioInterface * const mpRadioInterface;
  SampleReader mSampleReader;
  FicHandler mFicHandler;
  mscHandler mMscHandler;
  PhaseReference mPhaseReference;
  TII_Detector mTiiDetector;
  OfdmDecoder mOfdmDecoder;
  etiGenerator mEtiGenerator;
  TimeSyncer mTimeSyncer;
  const uint8_t mcDabMode;
  const float mcThreshold;
  const int16_t mcTiiDelay;
  const DabParams::SDabPar mDabPar;
  bool mScanMode = false;
  int16_t mTiiCounter = 0;
  bool mEti_on = false;
  bool mCorrectionNeeded = true;
  float mPhaseOffsetCyclPrefRad = 0.0f;
  float mFreqOffsCylcPrefHz = 0.0f;
  float mFreqOffsSyncSymb = 0.0f;
  int32_t mFreqOffsBBHz = 0;
  int32_t mFreqOffsRFHz = 0;
  int32_t mTimeSyncAttemptCount = 0;
  int32_t mClockOffsetTotalSamples = 0;
  int32_t mClockOffsetFrameCount = 0;
  bool mRfFreqShiftUsed = false;
  bool mAllowRfFreqShift = false;

  std::vector<cmplx> mOfdmBuffer;
  std::vector<int16_t> mBits;

  void run() override; // the new QThread

  bool _state_wait_for_time_sync_marker();
  bool _state_eval_sync_symbol(int32_t & oStartIndex, int32_t & oSampleCount, float iThreshold);
  void _state_process_rest_of_frame(int32_t iStartIndex, int32_t & ioSampleCount);
  float _process_ofdm_symbols_1_to_L(int32_t & ioSampleCount);
  void _process_null_symbol(int32_t & ioSampleCount);
  void _set_rf_freq_Hz(int32_t iFreqHz);
  void _set_bb_freq_Hz(int32_t iFreqHz);

public slots:
  void slot_select_carrier_plot_type(ECarrierPlotType iPlotType);
  void slot_select_iq_plot_type(EIqPlotType iPlotType);
  void slot_show_nominal_carrier(bool iShowNominalCarrier);
  void slot_use_old_soft_bit_gen(bool iUseOldSoftBitGen);

signals:
  void signal_set_synced(bool);
  void signal_no_signal_found();
  void signal_set_sync_lost();
  void signal_show_tii(int, int);
  void signal_show_spectrum(int);
  void signal_show_clock_err(int);
};

#endif
