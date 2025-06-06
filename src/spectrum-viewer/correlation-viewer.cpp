/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C)  2014 .. 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
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

#include "correlation-viewer.h"
#include <QSettings>
#include <QPen>
#include <QLabel>
#include "glob_defs.h"
#include "dab-constants.h"

CorrelationViewer::CorrelationViewer(QwtPlot * pPlot, QLabel * pLabel, QSettings * s, RingBuffer<f32> * b)
  : mpSettings(s)
  , mpResponseBuffer(b)
  , mpQwtPlot(pPlot)
  , mpIndexDisplay(pLabel)
  , mZoomPan(pPlot, CustQwtZoomPan::SRange(0, 2047))
{
  mQwtGrid.setMajorPen(QPen(QColor(0x5e5c64), 0, Qt::DotLine));
  mQwtGrid.setMinorPen(QPen(QColor(0x5e5c64), 0, Qt::DotLine));
  mQwtGrid.enableXMin(true);
  mQwtGrid.enableYMin(false);
  mQwtGrid.attach(mpQwtPlot);

  mQwtPlotCurve.setPen(QPen(QColor(0xffbe6f), 2.0));
  mQwtPlotCurve.setBaseline(0);
  mQwtPlotCurve.attach(mpQwtPlot);

  mpQwtPlot->enableAxis(QwtPlot::yLeft);

  mpThresholdMarker = new QwtPlotMarker();
  mpThresholdMarker->setLineStyle(QwtPlotMarker::HLine);
  mpThresholdMarker->setLinePen(QPen(Qt::darkYellow, 0, Qt::DashLine));
  mpThresholdMarker->setYValue(0); // threshold line is normed to 0dB
  mpThresholdMarker->attach(mpQwtPlot);
  mpQwtPlot->setAxisScale(QwtPlot::xBottom, 0, 2047);
  mpQwtPlot->enableAxis(QwtPlot::xBottom);
}

void CorrelationViewer::showCorrelation(f32 threshold, const QVector<i32> & v, const std::vector<STiiResult> & iTr)
{
  constexpr i32 cPlotLength = 2048;
  auto * const data = make_vla(f32, cPlotLength);
  // using log10_times_10() (not times 20) as the correlation is some kind of energy signal
  const f32 threshold_dB = 20 * log10(threshold);
  constexpr f32 cScalerFltAlpha1 = (f32)0.1;
  constexpr f32 cScalerFltAlpha2 = cScalerFltAlpha1 / (f32)cPlotLength;

  const i32 numRead = mpResponseBuffer->get_data_from_ring_buffer(data, cPlotLength);
  (void)numRead;

  if (numRead != cPlotLength)
  {
    // TODO: sometimes this happens in windows while startup
    qWarning("numRead != cPlotLength in correlation plot)");
    return;
  }

  std::array<f32, cPlotLength> X_axis;
  std::array<f32, cPlotLength> Y_values;

  f32 maxYVal = -1000.0f;

  for (u16 i = 0; i < cPlotLength; ++i)
  {
    X_axis[i] = (f32)i;
    Y_values[i] = 20.0f * std::log10(data[i]) - threshold_dB; // norm to threshold value

    if (Y_values[i] > maxYVal)
    {
      maxYVal = Y_values[i];
    }

    if (Y_values[i] < 0)
    {
      mean_filter(mMinValFlt, Y_values[i], cScalerFltAlpha2);
    }
  }

  mean_filter(mMaxValFlt, maxYVal, cScalerFltAlpha1);

  mZoomPan.set_y_range(CustQwtZoomPan::SRange(mMinValFlt - 8, mMaxValFlt + 3, -20.0, 20.0));
  mpQwtPlot->enableAxis(QwtPlot::yLeft);
  mQwtPlotCurve.setSamples(X_axis.data(), Y_values.data(), cPlotLength);
  mpQwtPlot->replot();

  mpIndexDisplay->setText(_get_best_match_text(v));

  // delete old markers
  for (auto & p : mQwtPlotMarkerVec)
  {
    p->detach();
    delete p;
    p = nullptr;
  }

  // ...a vertical line for each TII
  const i32 noMarkers = iTr.size();
  mQwtPlotMarkerVec.resize(noMarkers);
  for (i32 i = 0; i < noMarkers; ++i)
  {
    mQwtPlotMarkerVec[i] = new QwtPlotMarker();
    QwtPlotMarker * const p = mQwtPlotMarkerVec[i];
    QString tii = QString::number(iTr[i].mainId) + "-" + QString::number(iTr[i].subId);
    p->setLabel(tii);
    p->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    p->setLabelOrientation(Qt::Vertical);
    p->setLineStyle(QwtPlotMarker::VLine);
    p->setLinePen(Qt::white, 0, Qt::DashDotLine);
    f32 sample = (f32)iTr[i].phaseDeg * 2048 / 360 + 400;
    if (sample < 0) sample += 2048;
    else if (sample > 2407) sample -= 2048;
    p->setXValue(sample);
    p->attach(mpQwtPlot);
  }
}

QString CorrelationViewer::_get_best_match_text(const QVector<i32> & v)
{
  QString txt;

  if (!v.empty())
  {
    txt = "Best matches at (km): ";
    constexpr i32 MAX_NO_ELEM = 7;
    const i32 vSize = (v.size() < MAX_NO_ELEM ? v.size() : MAX_NO_ELEM); // limit size as display will broaden
    for (i32 i = 0; i < vSize; i++)
    {
      txt += "<b>" + QString::number(v[i]) + "</b>"; // display in "bold"

      if (i > 0)
      {
        const f64 distKm = (f64)(v[i] - v[0]) / (f64)INPUT_RATE * (f64)LIGHT_SPEED_MPS / 1000.0;
        txt += " (" + QString::number(distKm, 'f', 1) + ")";
      }

      if (i + 1 < v.size())
      {
        txt += ", ";
      }
    }
  }

  return txt;
}
