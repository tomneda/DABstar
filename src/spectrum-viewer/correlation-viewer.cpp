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
#include  "color-selector.h"

CorrelationViewer::CorrelationViewer(QwtPlot * pPlot, QLabel * pLabel, QSettings * s, RingBuffer<float> * b)
  : /*myFrame(nullptr),*/
    spectrumCurve("")
{
  QString colorString = "black";
  //this->myRadioInterface = mr;
  this->dabSettings = s;
  this->responseBuffer = b;

  dabSettings->beginGroup(SETTING_GROUP_NAME);
  colorString = dabSettings->value("gridColor", "#5e5c64").toString();
  gridColor = QColor(colorString);
  colorString = dabSettings->value("curveColor", "#ffbe6f").toString();
  curveColor = QColor(colorString);
  const bool brush = dabSettings->value("brush", 0).toInt() == 1;

  dabSettings->endGroup();

  plotgrid = pPlot;
  mpIndexDisplay = pLabel;
  //plotgrid->setCanvasBackground(displayColor);

  grid.setMajorPen(QPen(gridColor, 0, Qt::DotLine));
  grid.enableXMin(true);
  grid.enableYMin(true);
  grid.setMinorPen(QPen(gridColor, 0, Qt::DotLine));
  grid.attach(plotgrid);

  lm_picker = new QwtPlotPicker(plotgrid->canvas());
  QwtPickerMachine * lpickerMachine = new QwtPickerClickPointMachine();

  lm_picker->setStateMachine(lpickerMachine);
  lm_picker->setMousePattern(QwtPlotPicker::MouseSelect1, Qt::RightButton);
  connect(lm_picker, SIGNAL (selected(const QPointF&)), this, SLOT (rightMouseClick(const QPointF &)));

  spectrumCurve.setPen(QPen(curveColor, 2.0));
  spectrumCurve.setOrientation(Qt::Horizontal);
  spectrumCurve.setBaseline(0);

  if (brush)
  {
    QBrush ourBrush(curveColor);
    ourBrush.setStyle(Qt::Dense3Pattern);
    spectrumCurve.setBrush(ourBrush);
  }

  spectrumCurve.attach(plotgrid);
  plotgrid->enableAxis(QwtPlot::yLeft);
}

static int lcount = 0;

void CorrelationViewer::showCorrelation(int32_t dots, int marker, const QVector<int> & v)
{
  uint16_t i;
  float data[dots];
  float mmax = 0;

  responseBuffer->getDataFromBuffer(data, dots);

  constexpr int32_t plotLength = 700;
  double X_axis[plotLength];
  double Y_values[plotLength];

  for (i = 0; i < plotLength; i++)
  {
    X_axis[i] = marker - plotLength / 2 + i;
  }

  for (i = 0; i < plotLength; i++)
  {
    Y_values[i] = get_db(data[marker - plotLength / 2 + i]);
    if (Y_values[i] > mmax)
    {
      mmax = Y_values[i];
    }
  }

  if (++lcount < 2)
  {
    return;
  }

  lcount = 0;
  plotgrid->setAxisScale(QwtPlot::xBottom, (double)marker - plotLength / 2, (double)marker + plotLength / 2 - 1);
  plotgrid->enableAxis(QwtPlot::xBottom);
  plotgrid->setAxisScale(QwtPlot::yLeft, get_db(0), mmax);
  plotgrid->enableAxis(QwtPlot::yLeft);

  spectrumCurve.setBaseline(get_db(0));
  spectrumCurve.setSamples(X_axis, Y_values, plotLength);
  plotgrid->replot();

  mpIndexDisplay->setText(_get_best_match_text(v));
}

QString CorrelationViewer::_get_best_match_text(const QVector<int> & v)
{
  QString txt;

  if (v.size() > 0)
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

      if (i + 1 < v.size()) txt += ", ";
    }
  }

  return txt;
}

float CorrelationViewer::get_db(float x)
{
  return 20.0f * std::log10((x + 1) / (float)(4 * 512));
}

void CorrelationViewer::rightMouseClick(const QPointF & point)
{
  (void)point;

  if (!ColorSelector::show_dialog(gridColor, ColorSelector::GRIDCOLOR)) return;
  if (!ColorSelector::show_dialog(curveColor, ColorSelector::CURVECOLOR)) return;

  dabSettings->beginGroup(SETTING_GROUP_NAME);
  dabSettings->setValue("gridColor", gridColor.name());
  dabSettings->setValue("curveColor", curveColor.name());
  dabSettings->endGroup();
  spectrumCurve.setPen(QPen(this->curveColor, 2.0));
  grid.setMajorPen(QPen(this->gridColor, 0, Qt::DotLine));
  grid.enableXMin(true);
  grid.enableYMin(true);
  grid.setMinorPen(QPen(this->gridColor, 0, Qt::DotLine));
}

