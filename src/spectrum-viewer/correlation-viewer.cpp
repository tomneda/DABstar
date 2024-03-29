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

#include  "correlation-viewer.h"
#include  <QSettings>
#include  <QColor>
#include  <QPen>
#include  <QLabel>
#include  <qwt.h>
#include  <qwt_plot.h>
#include  <qwt_plot_marker.h>
#include  <qwt_plot_zoomer.h>
#include  <qwt_picker_machine.h>
#include  "color-selector.h"
#include  "glob_defs.h"
#include  "dab-constants.h"

CorrelationViewer::CorrelationViewer(QwtPlot * pPlot, QLabel * pLabel, QSettings * s, RingBuffer<float> * b)
  : mpSettings(s)
  , mpResponseBuffer(b)
  , mpPlotGrid(pPlot)
  , mpIndexDisplay(pLabel)
{
  QString colorString;
  mpSettings->beginGroup(SETTING_GROUP_NAME);
  colorString = mpSettings->value("gridColor", "#5e5c64").toString();
  mGridColor = QColor(colorString);
  colorString = mpSettings->value("curveColor", "#ffbe6f").toString();
  mCurveColor = QColor(colorString);
  const bool brush = (mpSettings->value("brush", 0).toInt() == 1);
  mpSettings->endGroup();

  mQwtGrid.setMajorPen(QPen(mGridColor, 0, Qt::DotLine));
  mQwtGrid.enableXMin(true);
  mQwtGrid.enableYMin(false);
  mQwtGrid.setMinorPen(QPen(mGridColor, 0, Qt::DotLine));
  mQwtGrid.attach(mpPlotGrid);

  lm_picker = new QwtPlotPicker(mpPlotGrid->canvas());
  QwtPickerMachine * lpickerMachine = new QwtPickerClickPointMachine();

  lm_picker->setStateMachine(lpickerMachine);
  lm_picker->setMousePattern(QwtPlotPicker::MouseSelect1, Qt::RightButton);

  connect(lm_picker, qOverload<const QPointF &>(&QwtPlotPicker::selected), this, &CorrelationViewer::_slot_right_mouse_click);

  mQwtPlotCurve.setPen(QPen(mCurveColor, 2.0));
  mQwtPlotCurve.setOrientation(Qt::Horizontal);
  mQwtPlotCurve.setBaseline(0);

  if (brush)
  {
    QBrush ourBrush(mCurveColor);
    ourBrush.setStyle(Qt::Dense3Pattern);
    mQwtPlotCurve.setBrush(ourBrush);
  }

  mQwtPlotCurve.attach(mpPlotGrid);
  mpPlotGrid->enableAxis(QwtPlot::yLeft);

  mpThresholdMarker = new QwtPlotMarker();
  mpThresholdMarker->setLineStyle(QwtPlotMarker::HLine);
  mpThresholdMarker->setLinePen(QPen(Qt::darkYellow, 0, Qt::DashLine));
  mpThresholdMarker->setYValue(0); // threshold line is normed to 0dB
  mpThresholdMarker->attach(mpPlotGrid);
}

void CorrelationViewer::showCorrelation(int32_t dots, int marker, float threshold, const QVector<int> & v)
{
  uint16_t i;
  auto * const data = make_vla(float, dots);
  // using log10_times_10() (not times 20) as the correlation is some kind of energy signal
  const float threshold_dB = log10_times_10(threshold);
  constexpr TQwtData sScalerFltAlpha = (TQwtData)0.1;
  const TQwtData scalerFltAlpha = sScalerFltAlpha / (TQwtData)dots;

  mpResponseBuffer->get_data_from_ring_buffer(data, dots);

  constexpr int32_t sPlotLength = 700;
  std::array<TQwtData, sPlotLength> X_axis;
  std::array<TQwtData, sPlotLength> Y_values;

  for (i = 0; i < sPlotLength; i++)
  {
    X_axis[i] = (float)(marker - sPlotLength / 2 + i);
  }

  TQwtData maxYVal = -1000.0f;

  for (i = 0; i < sPlotLength; i++)
  {
    Y_values[i] = log10_times_10(data[marker - sPlotLength / 2 + i]) - threshold_dB; // norm to threshold value

    if (Y_values[i] > maxYVal)
    {
      maxYVal = Y_values[i];
    }

    if (Y_values[i] < 0)
    {
      mean_filter(mMinValFlt, Y_values[i], scalerFltAlpha);
    }
  }

  mean_filter(mMaxValFlt, maxYVal, sScalerFltAlpha);

  mpPlotGrid->setAxisScale(QwtPlot::xBottom, (double)marker - sPlotLength / 2, (double)marker + sPlotLength / 2 - 1);
  mpPlotGrid->enableAxis(QwtPlot::xBottom);
  mpPlotGrid->setAxisScale(QwtPlot::yLeft, mMinValFlt - 8, mMaxValFlt + 3);
  mpPlotGrid->enableAxis(QwtPlot::yLeft);
  mQwtPlotCurve.setSamples(X_axis.data(), Y_values.data(), sPlotLength);
  mpPlotGrid->replot();

  mpIndexDisplay->setText(_get_best_match_text(v));
}

QString CorrelationViewer::_get_best_match_text(const QVector<int> & v)
{
  QString txt;

  if (!v.empty())
  {
    txt = "Best matches at (km): ";
    constexpr int32_t MAX_NO_ELEM = 7;
    const int32_t vSize = (v.size() < MAX_NO_ELEM ? v.size() : MAX_NO_ELEM); // limit size as display will broaden
    for (int32_t i = 0; i < vSize; i++)
    {
      txt += "<b>" + QString::number(v[i]) + "</b>"; // display in "bold"

      if (i > 0)
      {
        const double distKm = (double)(v[i] - v[0]) / (double)INPUT_RATE * (double)LIGHT_SPEED_MPS / 1000.0;
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

void CorrelationViewer::_slot_right_mouse_click(const QPointF & point)
{
  (void)point;

  if (!ColorSelector::show_dialog(mGridColor, ColorSelector::GRIDCOLOR))
  {
    return;
  }
  if (!ColorSelector::show_dialog(mCurveColor, ColorSelector::CURVECOLOR))
  {
    return;
  }

  mpSettings->beginGroup(SETTING_GROUP_NAME);
  mpSettings->setValue("gridColor", mGridColor.name());
  mpSettings->setValue("curveColor", mCurveColor.name());
  mpSettings->endGroup();
  mQwtPlotCurve.setPen(QPen(this->mCurveColor, 2.0));
  mQwtGrid.setMajorPen(QPen(this->mGridColor, 0, Qt::DotLine));
  mQwtGrid.enableXMin(true);
  mQwtGrid.enableYMin(true);
  mQwtGrid.setMinorPen(QPen(this->mGridColor, 0, Qt::DotLine));
}

