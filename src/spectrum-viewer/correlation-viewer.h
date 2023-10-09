/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2017
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
//	Simple viewer for correlation
//
#ifndef    CORRELATION_VIEWER_H
#define    CORRELATION_VIEWER_H

#include  <QFrame>
#include  <QSettings>
#include  <QObject>
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
#include  <QVector>
#include  "ringbuffer.h"
#include  "dab-constants.h"
#include  "ui_scopewidget.h"


class RadioInterface;

class CorrelationViewer : public QObject/*, Ui_scopeWidget*/
{
Q_OBJECT
public:
  CorrelationViewer(QwtPlot *, QLabel *, QSettings *, RingBuffer<float> *);
  ~CorrelationViewer() override = default;
  void showCorrelation(int32_t dots, int marker, const QVector<int> & v);

private:
  static constexpr char SETTING_GROUP_NAME[] = "correlationViewer";
  static QString _get_best_match_text(const QVector<int> & v);

  RadioInterface * myRadioInterface;
  QSettings * dabSettings;
  QwtPlotCurve spectrumCurve;
  QwtPlotGrid grid;
  std::vector<int> indexVector;
  static float get_db(float);
  RingBuffer<float> * responseBuffer;
  int16_t displaySize;
  QwtPlot * plotgrid;
  QLabel * mpIndexDisplay;
  QwtPlotPicker * lm_picker;
  //QColor displayColor;
  QColor gridColor;
  QColor curveColor;

private slots:
  void rightMouseClick(const QPointF &);
};

#endif

