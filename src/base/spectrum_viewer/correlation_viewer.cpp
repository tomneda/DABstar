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

#include "correlation_viewer.h"
#include <QSettings>
#include <QPen>
#include <QLabel>
#include <QChart>
#include "glob_defs.h"
#include "dab_constants.h"

CorrelationViewer::CorrelationViewer(PlotWidget * pPlot, QLabel * pLabel, QSettings * s, RingBuffer<f32> * b)
  : mpSettings(s)
  , mpResponseBuffer(b)
  , mpPlot(pPlot)
  , mpIndexDisplay(pLabel)
{
  mpPlot->get_x_axis()->setGridLineVisible(true);
  mpPlot->get_x_axis()->setMinorGridLineVisible(true);
  mpPlot->get_y_axis()->setGridLineVisible(true);
  mpPlot->get_y_axis()->setMinorGridLineVisible(false);

  mpCurve = new QLineSeries();
  mpCurve->setPen(QPen(QColor(0xffbe6f), 2.0));
  mpPlot->chart()->addSeries(mpCurve);
  mpCurve->attachAxis(mpPlot->get_x_axis());
  mpCurve->attachAxis(mpPlot->get_y_axis());

  // Horizontal threshold line at y=0 (threshold is normed to 0 dB)
  mpThresholdLine = new QLineSeries();
  mpThresholdLine->setPen(QPen(Qt::darkYellow, 1, Qt::DashLine));
  mpThresholdLine->append(-1e9, 0);
  mpThresholdLine->append(+1e9, 0);
  mpPlot->chart()->addSeries(mpThresholdLine);
  mpThresholdLine->attachAxis(mpPlot->get_x_axis());
  mpThresholdLine->attachAxis(mpPlot->get_y_axis());

  mpPlot->set_x_range(0, 2047);
  mpPlot->setup_x_zoom(PlotWidget::SRange(0, 2047));

  // Reposition text labels whenever zoom, pan, or resize changes the plot geometry
  connect(mpPlot->get_x_axis(), &QValueAxis::rangeChanged, this, [this](f64, f64) { _update_tii_label_positions(); });
  connect(mpPlot->get_y_axis(), &QValueAxis::rangeChanged, this, [this](f64, f64) { _update_tii_label_positions(); });
  connect(mpPlot, &PlotWidget::signal_plot_area_changed,   this, [this](int, int)  { _update_tii_label_positions(); });
}

void CorrelationViewer::show_correlation(const f32 iThreshold, const QVector<i32> & iV, const std::vector<STiiResult> & iTiiResultList)
{
  constexpr i32 cPlotLength = 2048;
  auto * const data = make_vla(f32, cPlotLength);
  const f32 threshold_dB = 20 * std::log10(iThreshold);
  constexpr f32 cScalerFltAlpha1 = (f32)0.1;
  constexpr f32 cScalerFltAlpha2 = cScalerFltAlpha1 / (f32)cPlotLength;

  const i32 numRead = mpResponseBuffer->get_data_from_ring_buffer(data, cPlotLength);

  if (numRead != cPlotLength)
  {
    qWarning("numRead != cPlotLength in correlation plot)");
    return;
  }

  std::array<f32, cPlotLength> X_axis;
  std::array<f32, cPlotLength> Y_values;

  f32 maxYVal = -1000.0f;

  for (u16 i = 0; i < cPlotLength; ++i)
  {
    X_axis[i] = (f32)i;
    Y_values[i] = 20.0f * std::log10(data[i]) - threshold_dB;

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

  mpPlot->setup_y_zoom(PlotWidget::SRange(mMinValFlt - 8, mMaxValFlt + 3, -20.0, 20.0));

  QList<QPointF> pts;
  pts.reserve(cPlotLength);
  for (i32 i = 0; i < cPlotLength; i++)
  {
    pts.append(QPointF(X_axis[i], Y_values[i]));
  }
  mpCurve->replace(pts);

  mpIndexDisplay->setText(_get_best_match_text(iV));

  // Remove old TII markers (line + label)
  for (const auto & marker : mTiiMarkerVec)
  {
    mpPlot->chart()->removeSeries(marker.pLine);
    delete marker.pLine;
    delete marker.pText;
  }
  mTiiMarkerVec.clear();

  // Add vertical line + text label for each TII result
  for (const auto & tiiResult : iTiiResultList)
  {
    STiiMarker marker;

    f32 sample = (f32)tiiResult.phaseDeg * 2048 / 360 + 400;
    if (sample < 0) sample += 2048;
    else if (sample > 2407) sample -= 2048;
    marker.xPos = sample;

    marker.pLine = new QLineSeries();
    marker.pLine->setPen(QPen(Qt::white, 1, Qt::DashDotLine));
    marker.pLine->append(sample, -1e9);
    marker.pLine->append(sample, +1e9);
    mpPlot->chart()->addSeries(marker.pLine);
    marker.pLine->attachAxis(mpPlot->get_x_axis());
    marker.pLine->attachAxis(mpPlot->get_y_axis());

    const QString tii = QString::number(tiiResult.mainId) + "-" + QString::number(tiiResult.subId);
    marker.pText = new QGraphicsSimpleTextItem(tii, mpPlot->chart());
    marker.pText->setBrush(Qt::white);
    marker.pText->setRotation(-90);
    marker.pText->setZValue(10);

    // make the font smaller
    QFont font = marker.pText->font();
    font.setPointSize(8);
    marker.pText->setFont(font);

    mTiiMarkerVec.push_back(std::move(marker));
  }

  _update_tii_label_positions();
}

void CorrelationViewer::_update_tii_label_positions() const
{
  const f64 yTop = mpPlot->get_y_axis()->max();
  for (const auto & marker : mTiiMarkerVec)
  {
    if (!marker.pText) continue;
    // Map data position (xPos, yTop) to chart widget coordinates
    const QPointF pos = mpPlot->chart()->mapToPosition(QPointF(marker.xPos, yTop));
    const f64 textW = marker.pText->boundingRect().width();
    // After -90° rotation world-x spans [px, px+textH] and world-y spans [py-textW, py].
    // Use a small fixed offset so the text starts just to the right of the line.
    marker.pText->setPos(pos.x() + 2.0, pos.y() + textW);
  }
}

QString CorrelationViewer::_get_best_match_text(const QVector<i32> & v)
{
  QString txt;

  if (!v.empty())
  {
    txt = "Best matches at (km): ";
    constexpr i32 MAX_NO_ELEM = 6; // maximum number of elements which can be display at a narrow window
    const i32 vSize = std::min((i32)v.size(), MAX_NO_ELEM);
    for (i32 i = 0; i < vSize; i++)
    {
      txt += "<b>" + QString::number(v[i]) + "</b>";

      if (i > 0)
      {
        const f64 distKm = (f64)(v[i] - v[0]) / (f64)INPUT_RATE * (f64)LIGHT_SPEED_MPS / 1000.0;
        txt += " (" + QString::number(distKm, 'f', 1) + ")";
      }

      if (i + 1 < vSize)
      {
        txt += ", ";
      }
    }
  }

  return txt;
}
