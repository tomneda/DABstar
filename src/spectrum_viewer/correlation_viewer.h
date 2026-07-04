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

#pragma once

#include <QObject>
#include <QColor>
#include <QVector>
#include <QLineSeries>
#include <QGraphicsSimpleTextItem>
#include "plot_widget.h"
#include "ringbuffer.h"
#include "tii_detector.h"

class DabRadio;
class QSettings;
class QLabel;

class CorrelationViewer : public QObject
{
Q_OBJECT
public:
  CorrelationViewer(PlotWidget *, QLabel *, QSettings *, RingBuffer<f32> *);
  ~CorrelationViewer() override = default;

  void show_correlation(f32 iThreshold, const QVector<i32> & iV, const std::vector<STiiResult> & iTiiResultList);

private:
  struct STiiMarker
  {
    QLineSeries * pLine = nullptr;
    QGraphicsSimpleTextItem * pText = nullptr;
    f32 xPos = 0;
  };

  static constexpr char SETTING_GROUP_NAME[] = "correlationViewer";
  static QString _get_best_match_text(const QVector<i32> & v);
  void _update_tii_label_positions() const;

  QSettings * const mpSettings = nullptr;
  RingBuffer<f32> * const mpResponseBuffer;
  PlotWidget * const mpPlot;
  QLabel * const mpIndexDisplay;

  QLineSeries * mpCurve = nullptr;
  QLineSeries * mpThresholdLine = nullptr;         // horizontal threshold line
  std::vector<STiiMarker> mTiiMarkerVec;           // vertical TII lines + labels

  PlotWidget::SRange mXZoomRange{0, 2047};
  std::vector<i32> mIndexVector;
  f32 mMinValFlt = -15;
  f32 mMaxValFlt = 10;
};
