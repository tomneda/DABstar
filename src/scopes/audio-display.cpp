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
#include  <qwt_plot_picker.h>
#include  <qwt_picker_machine.h>

AudioDisplay::AudioDisplay(IDabRadio * mr, QwtPlot * plotGrid, QSettings * dabSettings)
  : mpRadioInterface(mr)
  , mpDabSettings(dabSettings)
  , pPlotGrid(plotGrid)
{
  dabSettings->beginGroup(SETTING_GROUP_NAME);
  QString colorString = dabSettings->value("gridColor", "#5e5c64").toString();
  this->GridColor = QColor(colorString);
  colorString = dabSettings->value("curveColor", "#62a0ea").toString();
  this->mCurveColor = QColor(colorString);
  dabSettings->endGroup();

  mGrid.setMajorPen(QPen(GridColor, 0, Qt::DashLine));
  mGrid.setMinorPen(QPen(GridColor, 0, Qt::NoPen));
  mGrid.enableXMin(false);
  mGrid.enableYMin(false);
  mGrid.attach(plotGrid);
  QwtPlotPicker * pLmPicker = new QwtPlotPicker(plotGrid->canvas());
  QwtPickerMachine * pLPickerMachine = new QwtPickerClickPointMachine();

  pLmPicker->setStateMachine(pLPickerMachine);
  pLmPicker->setMousePattern(QwtPlotPicker::MouseSelect1, Qt::RightButton);

#ifdef _WIN32
  // It is strange, the non-macro based variant seems not working on windows, so use the macro-base version here.
  connect(pLmPicker, SIGNAL(selected(const QPointF&)), this, SLOT(_slot_rightMouseClick(const QPointF &)));
#else
  // The non macro-based variant is type-secure so it should be preferred.
  // Clang-glazy mentioned that QwtPlotPicker::selected would be no signal, but it is?!
  connect(pLmPicker, qOverload<const QPointF &>(&QwtPlotPicker::selected), this, &AudioDisplay::_slot_rightMouseClick);
#endif

  mSpectrumCurve.setPen(QPen(mCurveColor, 2.0));
  mSpectrumCurve.setOrientation(Qt::Horizontal);
  mSpectrumCurve.attach(plotGrid);

  pPlotGrid->enableAxis(QwtPlot::xBottom);
  pPlotGrid->setAxisScale(QwtPlot::yLeft, -120, -20);

  create_blackman_window(mWindow.data(), cSpectrumSize);
}

AudioDisplay::~AudioDisplay()
{
  fftwf_destroy_plan(mFftPlan);
}

void AudioDisplay::create_spectrum(const int16_t * const ipSampleData, const int iNumSamples, int iSampleRate)
{
  constexpr int16_t averageCount = 3;

  // iNumSamples is number of single samples (so it is halved for stereo)
  assert(iNumSamples % 2 == 0); // check of even number of samples
  const int32_t numStereoSamples = iNumSamples / 2;
  assert(cSpectrumSize == numStereoSamples);

  for (int32_t i = 0; i < numStereoSamples; i++)
  {
    // mSpectrumBuffer[i] = std::cos(F_2_M_PI * 12000.0f * (float)i / (float)iSampleRate);
    mFftInBuffer[i] = ((float)ipSampleData[2 * i + 0] + (float)ipSampleData[2 * i + 1]) / (2.0f * (float)INT16_MAX);
  }

  // and window it
  for (int32_t i = 0; i < cSpectrumSize; i++)
  {
    mFftInBuffer[i] *= mWindow[i];
  }

  // real value FFT, only the first half of the given back vector is useful
  fftwf_execute(mFftPlan);

  // first X axis labels
  if (iSampleRate != mSampleRateLast)
  {
    mSampleRateLast = iSampleRate;
    for (int32_t i = 0; i < cDisplaySize; i++)
    {
      mXDispBuffer[i] = (float)i * (float)iSampleRate / (float)cSpectrumSize / 1000.0f; // we use only the half spectrum
    }
    pPlotGrid->setAxisScale(QwtPlot::xBottom, (double)mXDispBuffer[0], mXDispBuffer[cDisplaySize - 1]);
  }

  // and map the spectrumSize values onto displaySize elements
  for (int32_t i = 0; i < cDisplaySize; i++)
  {
    constexpr float fftOffset = 20.0f * std::log10(cSpectrumSize / 4);
    const float yVal = log10_times_20(std::abs(mFftOutBuffer[i])) - fftOffset;
    mYDispBuffer[i] = (float)(averageCount - 1) / averageCount * mYDispBuffer[i] + 1.0f / averageCount * yVal; // average the image a little
  }

  mSpectrumCurve.setSamples(mXDispBuffer.data(), mYDispBuffer.data(), cDisplaySize);
  pPlotGrid->replot();
}

void AudioDisplay::_slot_rightMouseClick(const QPointF & point)
{
  (void)point;

  if (!ColorSelector::show_dialog(GridColor, ColorSelector::GRIDCOLOR))
  {
    return;
  }

  if (!ColorSelector::show_dialog(mCurveColor, ColorSelector::CURVECOLOR))
  {
    return;
  }

  mpDabSettings->beginGroup(SETTING_GROUP_NAME);
  mpDabSettings->setValue("gridColor", GridColor.name());
  mpDabSettings->setValue("curveColor", mCurveColor.name());
  mpDabSettings->endGroup();
  mSpectrumCurve.setPen(QPen(this->mCurveColor, 2.0));
}
