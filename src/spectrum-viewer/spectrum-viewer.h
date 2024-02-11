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
#include  <qwt_plot_layout.h>
#include  <qwt_picker_machine.h>
#include  <qwt_scale_widget.h>
#include  "dab-constants.h"
#include  "glob_enums.h"
#include  "ui_spectrum_viewer.h"
#include  "ringbuffer.h"
#include  "fft-handler.h"
#include  "custom_frame.h"

constexpr int32_t SP_DISPLAYSIZE = 512;
constexpr int32_t SP_SPECTRUMSIZE = 2048;
constexpr int32_t SP_SPEC_OVR_SMP_FAC = (SP_SPECTRUMSIZE / SP_DISPLAYSIZE);


class RadioInterface;
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
  SpectrumViewer(RadioInterface * ipRI, QSettings * ipDabSettings, RingBuffer<cmplx> * ipSpecBuffer, RingBuffer<cmplx> * ipIqBuffer, RingBuffer<float> * ipCarrBuffer, RingBuffer<float> * ipCorrBuffer);
  ~SpectrumViewer() override;

  void show_spectrum(int32_t);
  void show_correlation(int32_t dots, int marker, const QVector<int> & v);
  void show_nominal_frequency_MHz(float);
  void show_freq_corr_rf_Hz(int32_t iFreqCorrRF);
  void show_freq_corr_bb_Hz(int32_t iFreqCorrBB);
  void show_iq(int32_t, float);
  void show_quality(int32_t, float, float, float, float, float);
  void show_clock_error(float e);
  //void set_bit_depth(int16_t);
  void show();
  void hide();
  bool is_hidden();
  void show_digital_peak_level(float);

  enum class EAvrRate { SLOW, MEDIUM, FAST, DEFAULT = MEDIUM };
  void set_spectrum_averaging_rate(EAvrRate iAvrRate);

private:
  static constexpr char SETTING_GROUP_NAME[] = "spectrumViewer";

  CustomFrame myFrame{ nullptr };
  RadioInterface * const mpRadioInterface;
  QSettings * const mpDabSettings;
  RingBuffer<cmplx> * const mpSpectrumBuffer;
  RingBuffer<cmplx> * const mpIqBuffer;
  RingBuffer<float> * const mpCarrBuffer;
  RingBuffer<float> * const mpCorrelationBuffer;
  std::vector<cmplx> mIqValuesVec;
  std::vector<float> mCarrValuesVec;
  uint32_t mThermoPeakLevelConfigured = 0;
  bool mThermoModQualConfigured = false;
  SpecViewLimits<double> mSpecViewLimits;
  double mAvrAlpha = 0.1;

  fftHandler fft{ SP_SPECTRUMSIZE, false };

  std::array<cmplx, SP_SPECTRUMSIZE> mSpectrumVec{ 0 };
  std::array<float, SP_SPECTRUMSIZE> mWindowVec{ 0 };
  std::array<double, SP_DISPLAYSIZE> mXAxisVec{ 0 };
  std::array<double, SP_DISPLAYSIZE> mYValVec{ 0 };
  std::array<double, SP_DISPLAYSIZE> mDisplayBuffer{ 0 };

  int32_t mLastVcoFreq = -1; // do not use 0 as input files can have 0Hz center frequencies and this cache would be "valid"
  bool mShowInLogScale = false;

  CarrierDisp * mpCarrierDisp = nullptr;
  IQDisplay * mpIQDisplay = nullptr;
  SpectrumScope * mpSpectrumScope = nullptr;
  WaterfallScope * mpWaterfallScope = nullptr;
  CorrelationViewer * mpCorrelationViewer = nullptr;

  void _load_save_combobox_settings(QComboBox * ipCmb, const QString & iName, bool iSave);

public slots:
  void slot_update_settings();

private slots:
  void _slot_handle_cmb_carrier(int);
  void _slot_handle_cmb_iqscope(int);
  void _slot_handle_cb_nom_carrier(int);

signals:
  void signal_cmb_carrier_changed(ECarrierPlotType);
  void signal_cmb_iqscope_changed(EIqPlotType);
  void signal_cb_nom_carrier_changed(bool);
  void signal_window_closed();
};

#endif
