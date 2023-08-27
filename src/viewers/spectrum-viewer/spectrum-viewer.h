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
#include  <QFrame>
#include  <QObject>
#include  "ui_scopewidget.h"
#include  "ringbuffer.h"
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

#include  "fft/fft-handler.h"

class RadioInterface;
class QSettings;
class IQDisplay;
class PhaseVsCarrDisp;
class CorrelationViewer;
class SpectrumScope;
class WaterfallScope;

class SpectrumViewer : public QObject, Ui_scopeWidget
{
Q_OBJECT
public:
  SpectrumViewer(RadioInterface *, QSettings *, RingBuffer<cmplx> *, RingBuffer<cmplx> *, RingBuffer<float> *);
  ~SpectrumViewer() override;

  void showSpectrum(int32_t, int32_t);
  void showCorrelation(int32_t dots, int marker, const QVector<int> & v);
  void showFrequency(float);
  void showIQ(int32_t, float);
  void showQuality(int32_t, float, float, float, float);
  void show_snr(float);
  void show_clockErr(int);
  void show_correction(int);
  void setBitDepth(int16_t);
  void show();
  void hide();
  bool isHidden();

private:
  QFrame myFrame;
  RadioInterface * myRadioInterface;
  QSettings * dabSettings;
  RingBuffer<cmplx> * spectrumBuffer;
  RingBuffer<cmplx> * iqBuffer;
  RingBuffer<float> * mpCorrelationBuffer = nullptr;
  QwtPlotPicker * lm_picker{};
  //QColor mDisplayColor;
  QColor mGridColor;
  QColor mCurveColor;

  fftHandler fft;

  std::array<cmplx, SP_SPECTRUMSIZE> spectrum{ 0 };
  std::array<double, SP_SPECTRUMSIZE> displayBuffer{ 0 };
  std::array<float, SP_SPECTRUMSIZE> Window{ 0 };
  std::array<double, SP_DISPLAYSIZE> X_axis{ 0 };
  std::array<double, SP_DISPLAYSIZE> Y_values{ 0 };
  std::array<double, SP_DISPLAYSIZE> Y2_values{ 0 };

  //QwtPlotMarker * Marker{};
  QwtPlot * plotgrid{};
  QwtPlotGrid * grid{};
  QwtPlotCurve * spectrumCurve{};
  //QBrush * ourBrush{};
  //int32_t indexforMarker{};
  //void ViewSpectrum(double *, double *, double, int);
  [[nodiscard]] float get_db(float) const;
  int32_t normalizer = 0;
  int32_t lastVcoFreq = 0;

  PhaseVsCarrDisp * mpPhaseVsCarrDisp = nullptr;
  IQDisplay * myIQDisplay = nullptr;
  SpectrumScope * mySpectrumScope = nullptr;
  WaterfallScope * myWaterfallScope = nullptr;
  CorrelationViewer * mpCorrelationViewer = nullptr;
};

#endif
