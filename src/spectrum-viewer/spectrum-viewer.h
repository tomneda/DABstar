/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2019
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB.
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

//
//	Simple spectrum scope object
//	Shows the spectrum of the incoming data stream 
//	If made invisible, it is a "do nothing"
//
#ifndef    SPECTRUM_VIEWER_H
#define    SPECTRUM_VIEWER_H


#include  "dab-constants.h"
#include  "glob_enums.h"
#include  "ui_spectrum_viewer.h"
#include  "ringbuffer.h"
#include  "custom_frame.h"
#include  "tii-detector.h"
#include  <array>
#include  <QFrame>
#include  <QObject>
#include  <QBrush>
#include  <QTimer>
#include  <qwt.h>
#include  <qwt_plot.h>
#include  <qwt_plot_marker.h>
#include  <qwt_plot_grid.h>
#include  <qwt_plot_curve.h>
#include  <qwt_color_map.h>
#include  <qwt_plot_zoomer.h>
#include  <qwt_plot_textlabel.h>
#include  <qwt_plot_panner.h>
#include  <qwt_picker_machine.h>
#include  <qwt_scale_widget.h>
#include  <fftw3.h>

constexpr i32 SP_DISPLAYSIZE = 512;
constexpr i32 SP_SPECTRUMSIZE = 2048;
constexpr i32 SP_SPEC_OVR_SMP_FAC = (SP_SPECTRUMSIZE / SP_DISPLAYSIZE);


class DabRadio;
class QSettings;
class IQDisplay;
class CarrierDisp;
class CorrelationViewer;
class SpectrumScope;
class WaterfallScope;

class SpectrumViewer : public QObject, private Ui_scopeWidget
{
Q_OBJECT
public:
  SpectrumViewer(DabRadio * ipRI, QSettings * ipDabSettings, RingBuffer<cf32> * ipSpecBuffer, RingBuffer<cf32> * ipIqBuffer, RingBuffer<f32> * ipCarrBuffer, RingBuffer<f32> * ipCorrBuffer);
  ~SpectrumViewer() override;

  void show_spectrum(i32);
  void show_correlation(f32 threshold, const QVector<i32> & v, const std::vector<STiiResult> & iTr);
  void show_nominal_frequency_MHz(f32);
  void show_freq_corr_rf_Hz(i32 iFreqCorrRF);
  void show_freq_corr_bb_Hz(i32 iFreqCorrBB);
  void show_iq(i32, f32);
  void show_lcd_data(i32, f32, f32, f32, f32, f32);
  void show_fic_ber(f32 ber);
  void show_clock_error(f32 e);
  //void set_bit_depth(i16);
  void show();
  void hide();
  bool is_hidden();
  void show_digital_peak_level(f32);

  enum class EAvrRate { SLOW, MEDIUM, FAST, DEFAULT = MEDIUM };
  void set_spectrum_averaging_rate(EAvrRate iAvrRate);

private:

  CustomFrame myFrame{ nullptr };
  DabRadio * const mpRadioInterface;
  QSettings * const mpDabSettings;
  RingBuffer<cf32> * const mpSpectrumBuffer;
  RingBuffer<cf32> * const mpIqBuffer;
  RingBuffer<f32> * const mpCarrBuffer;
  RingBuffer<f32> * const mpCorrelationBuffer;
  std::vector<cf32> mIqValuesVec;
  std::vector<f32> mCarrValuesVec;
  u32 mThermoPeakLevelConfigured = 0;
  bool mThermoModQualConfigured = false;
  SpecViewLimits<f64> mSpecViewLimits;
  f64 mAvrAlpha = 0.1;

  alignas(64) std::array<cf32, SP_SPECTRUMSIZE> mFftInBuffer;
  alignas(64) std::array<cf32, SP_SPECTRUMSIZE> mFftOutBuffer;
  fftwf_plan mFftPlan{fftwf_plan_dft_1d(SP_SPECTRUMSIZE, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_FORWARD, FFTW_ESTIMATE)};

  std::array<f32, SP_SPECTRUMSIZE> mWindowVec{ 0 };
  std::array<f64, SP_DISPLAYSIZE> mXAxisVec{ 0 };
  std::array<f64, SP_DISPLAYSIZE> mYValVec{ 0 };
  std::array<f64, SP_DISPLAYSIZE> mDisplayBuffer{ 0 };

  i32 mLastVcoFreq = -1; // do not use 0 as input files can have 0Hz center frequencies and this cache would be "valid"

  CarrierDisp * mpCarrierDisp = nullptr;
  IQDisplay * mpIQDisplay = nullptr;
  SpectrumScope * mpSpectrumScope = nullptr;
  WaterfallScope * mpWaterfallScope = nullptr;
  CorrelationViewer * mpCorrelationViewer = nullptr;

  bool _calc_spectrum_display_limits(SpecViewLimits<f64>::SMaxMin & ioMaxMin) const;

public slots:
  void slot_update_settings();

private slots:
  void _slot_handle_cmb_carrier(i32);
  void _slot_handle_cmb_iqscope(i32);
  void _slot_handle_cb_nom_carrier(i32);
  void _slot_handle_cb_map_1st_quad(i32);

signals:
  void signal_cmb_carrier_changed(ECarrierPlotType);
  void signal_cmb_iqscope_changed(EIqPlotType);
  void signal_cb_nom_carrier_changed(bool);
  void signal_window_closed();
};

#endif
