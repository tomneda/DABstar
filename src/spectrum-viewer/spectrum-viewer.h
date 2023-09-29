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
#include  <QFrame>
#include  <QObject>
#include  "ui_scopewidget.h"
#include  "ringbuffer.h"
#include  "fft/fft-handler.h"
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
#include  <QBrush>
#include  <QTimer>

constexpr int32_t SP_DISPLAYSIZE = 512;
constexpr int32_t SP_SPECTRUMSIZE = 2048;
constexpr int32_t SP_SPECTRUMOVRSMPFAC = (SP_SPECTRUMSIZE / SP_DISPLAYSIZE);


class RadioInterface;
class QSettings;
class IQDisplay;
class CarrierDisp;
class CorrelationViewer;
class SpectrumScope;
class WaterfallScope;

class SpectrumViewer : public QObject, Ui_scopeWidget
{
Q_OBJECT
public:
  SpectrumViewer(RadioInterface * ipRI, QSettings * ipDabSettings, RingBuffer<cmplx> * ipSpecBuffer, RingBuffer<cmplx> * ipIqBuffer, RingBuffer<float> * ipCarrBuffer, RingBuffer<float> * ipCorrBuffer);
  ~SpectrumViewer() override;

  void showSpectrum(int32_t, int32_t);
  void showCorrelation(int32_t dots, int marker, const QVector<int> & v);
  void show_frequency_MHz(float);
  void showIQ(int32_t, float);
  void showQuality(int32_t, float, float, float, float, float);
  void show_snr(float);
  void show_clockErr(int);
  void show_correction(int);
  void setBitDepth(int16_t);
  void show();
  void hide();
  bool isHidden();

private:
  QFrame myFrame;
  RadioInterface * const myRadioInterface;
  QSettings * const dabSettings;
  RingBuffer<cmplx> * const spectrumBuffer;
  RingBuffer<cmplx> * const iqBuffer;
  RingBuffer<float> * const carrBuffer;
  RingBuffer<float> * const mpCorrelationBuffer;
  QwtPlotPicker * lm_picker = nullptr;
  std::vector<cmplx> mIqValuesVec;
  std::vector<float> mCarrValuesVec;

  fftHandler fft;

  std::array<cmplx, SP_SPECTRUMSIZE> spectrum{ 0 };
  std::array<double, SP_SPECTRUMSIZE> displayBuffer{ 0 };
  std::array<float, SP_SPECTRUMSIZE> Window{ 0 };
  std::array<double, SP_DISPLAYSIZE> X_axis{ 0 };
  std::array<double, SP_DISPLAYSIZE> Y_values{ 0 };
  std::array<double, SP_DISPLAYSIZE> Y2_values{ 0 };

  QwtPlot * plotgrid{};
  QwtPlotGrid * grid{};
  int32_t normalizer = 0;
  int32_t lastVcoFreq = 0;
  bool mShowInLogScale = false;

  CarrierDisp * mpCarrierDisp = nullptr;
  IQDisplay * mpIQDisplay = nullptr;
  SpectrumScope * mpSpectrumScope = nullptr;
  WaterfallScope * mpWaterfallScope = nullptr;
  CorrelationViewer * mpCorrelationViewer = nullptr;

  [[nodiscard]] float get_db(float) const;

private slots:
  void _slot_handle_cmb_carrier(int);
  void _slot_handle_cmb_iqscope(int);
  void _slot_handle_cb_nom_carrier(int);

signals:
  void signal_cmb_carrier_changed(ECarrierPlotType);
  void signal_cmb_iqscope_changed(EIqPlotType);
  void signal_cb_nom_carrier_changed(bool);
};

#endif
