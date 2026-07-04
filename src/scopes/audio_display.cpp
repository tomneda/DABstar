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

#include  "audio_display.h"
#include  "plot_widget.h"
#include  <QColor>
#include  <QPen>
#include  <QChart>
#include  <QValueAxis>

AudioDisplay::AudioDisplay(DabRadio * mr, PlotWidget * pPlot, QSettings * dabSettings)
  : mpRadioInterface(mr)
  , mpDabSettings(dabSettings)
  , mpPlot(pPlot)
{
  mpCurve = new QLineSeries();
  mpCurve->setPen(QPen(QColor(0x62a0ea), 2.0));
  mpCurve->setUseOpenGL(false);
  mpPlot->chart()->addSeries(mpCurve);
  mpCurve->attachAxis(mpPlot->get_x_axis());
  mpCurve->attachAxis(mpPlot->get_y_axis());

  mpPlot->get_x_axis()->setGridLineVisible(true);
  mpPlot->get_x_axis()->setMinorGridLineVisible(false);
  mpPlot->get_y_axis()->setGridLineVisible(true);
  mpPlot->get_y_axis()->setMinorGridLineVisible(false);
  mpPlot->set_y_range(-120, -20);

  create_blackman_window(mWindow.data(), cSpectrumSize);
}

AudioDisplay::~AudioDisplay()
{
  fftwf_destroy_plan(mFftPlan);
}

void AudioDisplay::create_spectrum(const i16 * const ipSampleData, const i32 iNumSamples, i32 iSampleRate)
{
  constexpr i16 averageCount = 3;

  // iNumSamples is number of single samples (so it is halved for stereo)
  assert(iNumSamples % 2 == 0); // check of even number of samples
  const i32 numStereoSamples = iNumSamples / 2;
  assert(cSpectrumSize == numStereoSamples);

  for (i32 i = 0; i < numStereoSamples; i++)
  {
    mFftInBuffer[i] = ((f32)ipSampleData[2 * i + 0] + (f32)ipSampleData[2 * i + 1]) / (2.0f * (f32)INT16_MAX);
  }

  // and window it
  for (i32 i = 0; i < cSpectrumSize; i++)
  {
    mFftInBuffer[i] *= mWindow[i];
  }

  // real value FFT, only the first half of the given back vector is useful
  fftwf_execute(mFftPlan);

  // first X axis labels
  if (iSampleRate != mSampleRateLast)
  {
    mSampleRateLast = iSampleRate;
    for (i32 i = 0; i < cDisplaySize; i++)
    {
      mXDispBuffer[i] = (f32)i * (f32)iSampleRate / (f32)cSpectrumSize / 1000.0f;
    }
    mpPlot->set_x_range(mXDispBuffer[0], mXDispBuffer[cDisplaySize - 1]);
    mpPlot->set_x_tick_dynamic(0.0, 4.0);
  }

  // and map the spectrumSize values onto displaySize elements
  for (i32 i = 0; i < cDisplaySize; i++)
  {
    static const f32 fftOffset = 20.0f * std::log10(cSpectrumSize / 4);
    const f32 yVal = log10_times_10(std::norm(mFftOutBuffer[i])) - fftOffset;
    mYDispBuffer[i] = (f32)(averageCount - 1) / averageCount * mYDispBuffer[i] + 1.0f / averageCount * yVal;
  }
  mYDispBuffer[0] -= 3.01; // compensate for the DC bin offset

  QList<QPointF> pts;
  pts.reserve(cDisplaySize);
  for (i32 i = 0; i < cDisplaySize; i++)
  {
    pts.append(QPointF(mXDispBuffer[i], mYDispBuffer[i]));
  }
  mpCurve->replace(pts);
}
