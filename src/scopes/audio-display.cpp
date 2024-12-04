/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C)  2014 .. 2017
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

#include  "audio-display.h"
#include  "color-selector.h"
#include  <QColor>
#include  <QPen>

AudioDisplay::AudioDisplay(RadioInterface * mr, QwtPlot * plotGrid, QSettings * dabSettings) :
  myRadioInterface(mr),
  dabSettings(dabSettings),
  plotGrid(plotGrid)
{
  std::fill(displayBuffer.begin(), displayBuffer.end(), 0);

  spectrumBuffer = new cmplx[spectrumSize];

  dabSettings->beginGroup(SETTING_GROUP_NAME);
  QString colorString = dabSettings->value("gridColor", "#5e5c64").toString();
  this->gridColor = QColor(colorString);
  colorString = dabSettings->value("curveColor", "#f9f06b").toString();
  this->curveColor = QColor(colorString);
  brush = dabSettings->value("brush", 1).toInt() == 1;
  displaySize = dabSettings->value("displaySize", displaySize).toInt();
  dabSettings->endGroup();

  grid.setMajorPen(QPen(gridColor, 0, Qt::DotLine));
  grid.enableXMin(true);
  grid.enableYMin(true);
  grid.setMinorPen(QPen(gridColor, 0, Qt::DotLine));
  grid.attach(plotGrid);
  lm_picker = new QwtPlotPicker(plotGrid->canvas());
  QwtPickerMachine * lpickerMachine = new QwtPickerClickPointMachine();

  lm_picker->setStateMachine(lpickerMachine);
  lm_picker->setMousePattern(QwtPlotPicker::MouseSelect1, Qt::RightButton);

#ifdef _WIN32
  // It is strange, the non-macro based variant seems not working on windows, so use the macro-base version here.
  connect(lm_picker, SIGNAL(selected(const QPointF&)), this, SLOT(_slot_rightMouseClick(const QPointF &)));
#else
  // The non macro-based variant is type-secure so it should be preferred.
  // Clang-glazy mentioned that QwtPlotPicker::selected would be no signal, but it is?!
  connect(lm_picker, qOverload<const QPointF &>(&QwtPlotPicker::selected), this, &AudioDisplay::_slot_rightMouseClick);
#endif

  spectrumCurve.setPen(QPen(curveColor, 2.0));
  spectrumCurve.setOrientation(Qt::Horizontal);
  spectrumCurve.setBaseline(_get_db(0));
  if (brush)
  {
    QBrush ourBrush(curveColor);
    ourBrush.setStyle(Qt::Dense3Pattern);
    spectrumCurve.setBrush(ourBrush);
  }
  spectrumCurve.attach(plotGrid);

  create_blackman_window(Window.data(), spectrumSize);
}

AudioDisplay::~AudioDisplay()
{
  delete spectrumBuffer;
}


void AudioDisplay::create_spectrum(int16_t * data, int amount, int sampleRate)
{
  auto * const X_axis = make_vla(double, displaySize);
  auto * const Y_values = make_vla(double, displaySize);
  constexpr int16_t averageCount = 3;

  if (amount > spectrumSize)
  {
    amount = spectrumSize;
  }
  for (int32_t i = 0; i < amount / 2; i++)
  {
    spectrumBuffer[i] = cmplx(data[2 * i] / 8192.0f, data[2 * i + 1] / 8192.0f);
  }

  for (int32_t i = amount / 2; i < spectrumSize; i++)
  {
    spectrumBuffer[i] = cmplx(0, 0);
  }
  //	and window it

  for (int32_t i = 0; i < spectrumSize; i++)
  {
    spectrumBuffer[i] = spectrumBuffer[i] * Window[i];
  }

  fft.fft(spectrumBuffer);

  // first X axis labels
  for (int32_t i = 0; i < displaySize; i++)
  {
    X_axis[i] = (double)((i) * (double)sampleRate / 2) / displaySize / 1000;
  }

  // and map the spectrumSize values onto displaySize elements
  for (int32_t i = 0; i < displaySize; i++)
  {
    double f = 0;

    for (int32_t j = 0; j < spectrumSize / 2 / displaySize; j++)
    {
      f += abs(spectrumBuffer[i * spectrumSize / 2 / displaySize + j]);
    }

    Y_values[i] = f / (spectrumSize / 2 / displaySize);
  }
  //
  //	average the image a little.
  for (int32_t i = 0; i < displaySize; i++)
  {
    if (std::isnan(Y_values[i]) || std::isinf(Y_values[i]))
    {
      continue;
    }
    displayBuffer[i] = (double)(averageCount - 1) / averageCount * displayBuffer[i] + 1.0f / averageCount * Y_values[i];
  }

  memcpy(Y_values, displayBuffer.data(), displaySize * sizeof(double));
  _view_spectrum(X_axis, Y_values, 100, 0);
}

void AudioDisplay::_view_spectrum(double * X_axis, double * Y1_value, double amp, int32_t marker)
{
  float amp1 = amp / 100;

  amp = amp / 50.0 * (-_get_db(0));
  plotGrid->setAxisScale(QwtPlot::xBottom, (double)X_axis[0], X_axis[displaySize - 1]);
  plotGrid->enableAxis(QwtPlot::xBottom);
  plotGrid->setAxisScale(QwtPlot::yLeft, _get_db(0), _get_db(0) + amp1 * 40);

  for (int32_t i = 0; i < displaySize; i++)
  {
    Y1_value[i] = _get_db(amp1 * Y1_value[i]);
  }

  spectrumCurve.setBaseline(_get_db(0));
  Y1_value[0] = _get_db(0);
  Y1_value[displaySize - 1] = _get_db(0);

  spectrumCurve.setSamples(X_axis, Y1_value, displaySize);
  plotGrid->replot();
}

float AudioDisplay::_get_db(float x)
{
  return 20 * log10((x + 1) / (float)(normalizer));
}

void AudioDisplay::_slot_rightMouseClick(const QPointF & point)
{
  (void)point;

  //  const QString displayColor = ColorSelector::show_dialog(ColorSelector::DISPCOLOR);
  //  if (displayColor.isEmpty()) return;
  if (!ColorSelector::show_dialog(gridColor, ColorSelector::GRIDCOLOR))
  {
    return;
  }
  if (!ColorSelector::show_dialog(curveColor, ColorSelector::CURVECOLOR))
  {
    return;
  }

  dabSettings->beginGroup(SETTING_GROUP_NAME);
  dabSettings->setValue("gridColor", gridColor.name());
  dabSettings->setValue("curveColor", curveColor.name());
  dabSettings->endGroup();
  spectrumCurve.setPen(QPen(this->curveColor, 2.0));

  if (brush)
  {
    QBrush ourBrush(this->curveColor);
    ourBrush.setStyle(Qt::Dense3Pattern);
    spectrumCurve.setBrush(ourBrush);
  }

  grid.setMajorPen(QPen(this->gridColor, 0, Qt::DotLine));
  grid.enableXMin(true);
  grid.enableYMin(true);
  grid.setMinorPen(QPen(this->gridColor, 0, Qt::DotLine));
}
