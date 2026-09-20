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

#include "audio_display.h"
#include "plot_widget.h"
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QLinearGradient>
#include <QChart>
#include <QValueAxis>
#include <QCategoryAxis>
#include <algorithm>
#include <cmath>
#include <numeric>

AudioDisplay::AudioDisplay(DabRadio * mr, PlotWidget * pPlot, QSettings * dabSettings)
  : mpRadioInterface(mr)
  , mpDabSettings(dabSettings)
  , mpPlot(pPlot)
{
  mpLineSeries = new QLineSeries();
  mpLineSeries->setPen(QPen(QColor(0x62a0ea), 2.0));
  mpLineSeries->setUseOpenGL(false);

  mpUpperCurve = new QLineSeries();
  mpLowerCurve = new QLineSeries();
  mpAreaSeries = new QAreaSeries(mpUpperCurve, mpLowerCurve);
  mpAreaSeries->setPen(QPen(QColor(0x62a0ea), 1.5));
  mpAreaSeries->setUseOpenGL(false);

  QLinearGradient grad(0, 0, 0, 1);
  grad.setCoordinateMode(QGradient::ObjectBoundingMode);
  grad.setColorAt(0.0, QColor(0x78, 0xd0, 0xff, 230));
  grad.setColorAt(0.3, QColor(0x40, 0x90, 0xf0, 180));
  grad.setColorAt(0.7, QColor(0x20, 0x60, 0xc0, 120));
  grad.setColorAt(1.0, QColor(0x10, 0x30, 0x80, 40));
  mpAreaSeries->setBrush(QBrush(grad));

  mpCategoryAxis = new QCategoryAxis();
  mpCategoryAxis->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
  mpCategoryAxis->setLabelsColor(Qt::lightGray);
  mpCategoryAxis->setGridLineColor(QColor(0x5e5c64));
  mpCategoryAxis->setMinorGridLinePen(Qt::NoPen);
  mpCategoryAxis->setMinorGridLineVisible(false);
  mpCategoryAxis->setTruncateLabels(false);
  mpCategoryAxis->setVisible(false);

  mpPlot->chart()->addSeries(mpLineSeries);
  mpPlot->chart()->addSeries(mpAreaSeries);
  mpPlot->chart()->addAxis(mpCategoryAxis, Qt::AlignBottom);
  mpLineSeries->attachAxis(mpPlot->get_x_axis());
  mpLineSeries->attachAxis(mpPlot->get_y_axis());
  mpAreaSeries->attachAxis(mpPlot->get_x_axis());
  mpAreaSeries->attachAxis(mpPlot->get_y_axis());

  mpPlot->get_x_axis()->setGridLineVisible(true);
  mpPlot->get_x_axis()->setMinorGridLinePen(QPen(QColor(0x3a3840)));
  mpPlot->get_x_axis()->setMinorGridLineVisible(true);
  mpPlot->get_y_axis()->setGridLineVisible(true);
  mpPlot->get_y_axis()->setMinorGridLineVisible(false);
  mpPlot->setup_y_zoom(PlotWidget::SRange(-120, -20));
  mpPlot->setToolTip(_get_plot_mode_tool_tip(mPlotMode)); // set_plot_mode() is a no-op if the restored mode equals the default

  mYDispBuffer.fill(-120.0f);
  mYBand8Buffer.fill(-120.0f);
  _calculate_band_limits(48000);

  create_blackman_window(mWindow.data(), cSpectrumSize);
}

AudioDisplay::~AudioDisplay()
{
  if (mpCategoryAxis)
  {
    if (mpPlot && mpPlot->chart())
    {
      mpPlot->chart()->removeAxis(mpCategoryAxis);
    }
    delete mpCategoryAxis;
    mpCategoryAxis = nullptr;
  }
  fftwf_destroy_plan(mFftPlan);
}

QStringList AudioDisplay::get_plot_mode_names()
{
  return {
    "FFT Spectrum (unfilled)",
    "FFT Spectrum (filled)",
    "8 Log Bands",
    "Spectrum Off",
  };
}

QString AudioDisplay::_get_plot_mode_tool_tip(const EAudioPlotMode iMode)
{
  QString tip;

  switch (iMode)
  {
  case EAudioPlotMode::FFT_SPECTRUM_UNFILLED:
  case EAudioPlotMode::FFT_SPECTRUM_FILLED:
    tip = "Shows the spectrum of the decoded audio in dBFS over the frequency in kHz.";
    break;

  case EAudioPlotMode::LOG_BANDS:
    tip = "Shows the decoded audio level in dBFS summed up over 8 logarithmically spaced frequency bands."
          "<p>The x-axis labels mark the upper frequency boundary of each band.</p>";
    break;

  case EAudioPlotMode::OFF:
    return "The audio spectrum display is switched off.";
  }

  tip += "<p>Both stereo channels are mixed to mono and transformed with a 512 point FFT. "
         "The coherent gain of the Blackman window is compensated, so a full-scale (0 dBFS) sine wave peaks at 0 dB. "
         "The trace is averaged over the last 3 FFTs.</p>"
         "<p><b>Zooming</b><br>"
         "Press <b>&lt;CTRL&gt;</b> key for <b>vertical</b> zooming, panning and reset zoom.<br>";

  if (iMode != EAudioPlotMode::LOG_BANDS) // the band axis is fixed, so horizontal zoom is disabled there
  {
    tip += "Press <b>&lt;SHIFT&gt;</b> key for <b>horizontal</b> zooming, panning and reset zoom.<br>";
  }

  tip += "Use mouse wheel for zooming.<br>"
         "Press left mouse button for panning.<br>Press right mouse button to reset zoom.</p>";

  return tip;
}

void AudioDisplay::set_plot_mode(const EAudioPlotMode iMode)
{
  if (mPlotMode != iMode)
  {
    mPlotMode = iMode;
    mPlotModeChanged = true;
    mpPlot->setToolTip(_get_plot_mode_tool_tip(mPlotMode));
    if (mPlotMode != EAudioPlotMode::OFF)
    {
      const i32 sampleRate = (mSampleRateLast > 0) ? mSampleRateLast : 48000;
      _setup_x_axis(sampleRate);
      mpPlot->reset_y_zoom();
    }
    mpUpperCurve->clear();
    mpLowerCurve->clear();
    mpLineSeries->clear();
  }
}

void AudioDisplay::_calculate_band_limits(const i32 iSampleRate)
{
  const i32 sampleRate = (iSampleRate > 0) ? iSampleRate : 48000;
  i32 curStart = 0;
  for (i32 b = 0; b < 7; ++b)
  {
    mBandLimits8[b].startBin = curStart;
    const i32 rawBin = static_cast<i32>(std::round(cBorderFrequencies[b] * static_cast<f32>(cSpectrumSize) / static_cast<f32>(sampleRate))) - 1;
    mBandLimits8[b].endBin = std::clamp(rawBin, curStart, cDisplaySize - 1);
    curStart = std::min(mBandLimits8[b].endBin + 1, cDisplaySize - 1);
  }
  mBandLimits8[7].startBin = curStart;
  mBandLimits8[7].endBin = cDisplaySize - 1;
}

void AudioDisplay::_setup_x_axis(const i32 iSampleRate)
{
  const i32 sampleRate = (iSampleRate > 0) ? iSampleRate : 48000;
  _calculate_band_limits(sampleRate);

  switch (mPlotMode)
  {
  case EAudioPlotMode::FFT_SPECTRUM_UNFILLED:
  case EAudioPlotMode::FFT_SPECTRUM_FILLED:
  {
    if (mpCategoryAxis && mpCategoryAxis->isVisible())
    {
      mpAreaSeries->detachAxis(mpCategoryAxis);
      mpCategoryAxis->setVisible(false);
      mpPlot->get_x_axis()->setVisible(true);
      mpAreaSeries->attachAxis(mpPlot->get_x_axis());
      mpLineSeries->attachAxis(mpPlot->get_x_axis());
    }

    const f32 maxFreqKhz = (f32)(cDisplaySize - 1) * (f32)sampleRate / (f32)cSpectrumSize / 1000.0f;
    for (i32 i = 0; i < cDisplaySize; i++)
    {
      mXDispBuffer[i] = (f32)i * (f32)sampleRate / (f32)cSpectrumSize / 1000.0f;
    }
    mpPlot->setup_x_zoom(PlotWidget::SRange(0.0, maxFreqKhz));
    mpPlot->set_x_tick_dynamic(0.0, 2.0);

    mpPlot->setup_y_zoom(PlotWidget::SRange(-120, -20, 0, 30));
    break;
  }
  case EAudioPlotMode::LOG_BANDS:
  {
    if (mpCategoryAxis && !mpCategoryAxis->isVisible())
    {
      mpAreaSeries->detachAxis(mpPlot->get_x_axis());
      mpLineSeries->detachAxis(mpPlot->get_x_axis());
      mpPlot->get_x_axis()->setVisible(false);
      mpCategoryAxis->setVisible(true);
      mpAreaSeries->attachAxis(mpCategoryAxis);
    }

    if (mpCategoryAxis)
    {
      for (const QString & label : mpCategoryAxis->categoriesLabels())
      {
        mpCategoryAxis->remove(label);
      }
      mpCategoryAxis->setStartValue(0.5);
      for (i32 b = 0; b < 7; ++b)
      {
        const f32 freqBoundary = (f32)(mBandLimits8[b].endBin + 1) * (f32)sampleRate / (f32)cSpectrumSize;
        // freqBoundary = std::round(freqBoundary / 5.0f) * 5.0f;

        QString label;
        if (freqBoundary >= 1000.0f)
        {
          label = QString::asprintf("%.1fk", (f64)(freqBoundary / 1000.0f));
        }
        else if (freqBoundary < 100.0f && std::fmod(freqBoundary * 2.0f, 1.0f) == 0.0f && std::fmod(freqBoundary, 1.0f) != 0.0f)
        {
          label = QString::asprintf("%.1f", (f64)freqBoundary);
        }
        else
        {
          label = QString::asprintf("%.0f", (f64)freqBoundary);
        }
        mpCategoryAxis->append(label, (f64)(b + 1) + 0.5);
      }
      mpCategoryAxis->setRange(0.5, 8.5);
    }
    mpPlot->setup_x_zoom(PlotWidget::SRange(0.0, 0.0));
    mpPlot->setup_y_zoom(PlotWidget::SRange(-50, -10, -30, 10));
    break;
  }
  case EAudioPlotMode::OFF:
    break;
  }
}

void AudioDisplay::plot_spectrum(const i16 * const ipSampleData, const i32 iNumSamples, i32 iSampleRate)
{
  if (mPlotMode == EAudioPlotMode::OFF)
  {
    return;
  }

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

  if (iSampleRate != mSampleRateLast || mPlotModeChanged)
  {
    mSampleRateLast = iSampleRate;
    mPlotModeChanged = false;
    _setup_x_axis(iSampleRate);
  }

  constexpr f32 yFloor = -120.0f;
  // Calibrate a 0 dBFS sine peak to 0 dB using the window's coherent gain (sum of window coefficients).
  // A real sine wave of peak amplitude 1.0 splits into positive and negative frequency components (factor 0.5),
  // so the FFT peak magnitude is 0.5 * sum(mWindow).
  static const f32 fftOffset = 20.0f * std::log10(std::accumulate(mWindow.begin(), mWindow.end(), 0.0f) * 0.5f);

  switch (mPlotMode)
  {
  case EAudioPlotMode::FFT_SPECTRUM_UNFILLED:
  case EAudioPlotMode::FFT_SPECTRUM_FILLED:
  {

    for (i32 i = 0; i < cDisplaySize; i++)
    {
      const f32 yVal = log10_times_10(std::norm(mFftOutBuffer[i])) - fftOffset;
      mYDispBuffer[i] = (f32)(averageCount - 1) / averageCount * mYDispBuffer[i] + 1.0f / averageCount * yVal;
    }
    // Compensate for the DC bin (bin 0): unlike AC sine waves that split into positive and negative
    // frequency components (factor 0.5), a DC signal has all its energy in bin 0 (factor 1.0, 2x amplitude).
    // Subtracting 20 * log10(2) = 6.02 dB calibrates a 0 dBFS DC level to 0 dB.
    mYDispBuffer[0] -= 6.02f;

    QList<QPointF> ptsUpper;
    ptsUpper.reserve(cDisplaySize);
    for (i32 i = 0; i < cDisplaySize; i++)
    {
      ptsUpper.append(QPointF(mXDispBuffer[i], mYDispBuffer[i]));
    }

    if (mPlotMode == EAudioPlotMode::FFT_SPECTRUM_FILLED)
    {
      mpUpperCurve->replace(ptsUpper);

      QList<QPointF> ptsLower;
      ptsLower.reserve(2);
      ptsLower.append(QPointF(mXDispBuffer[0], yFloor));
      ptsLower.append(QPointF(mXDispBuffer[cDisplaySize - 1], yFloor));
      mpLowerCurve->replace(ptsLower);
    }
    else
    {
      mpLineSeries->replace(ptsUpper);
    }
    break;
  }

  case EAudioPlotMode::LOG_BANDS:
  {
    constexpr f64 barWidth = 0.8;
    QList<QPointF> ptsUpper;
    ptsUpper.reserve(8 * 4 + 2);
    ptsUpper.append(QPointF(0.5, yFloor));

    for (i32 b = 0; b < 8; ++b)
    {
      f32 pBand = 0.0f;
      for (i32 k = mBandLimits8[b].startBin; k <= mBandLimits8[b].endBin; ++k)
      {
        pBand += std::norm(mFftOutBuffer[k]);
      }
      // pBand /= (mBandLimits8[b].endBin - mBandLimits8[b].startBin + 1);
      const f32 yVal = log10_times_10(pBand) - fftOffset;
      mYBand8Buffer[b] = (f32)(averageCount - 1) / averageCount * mYBand8Buffer[b] + 1.0f / averageCount * yVal;

      const f64 xCenter = (f64)(b + 1);
      const f64 xLeft   = xCenter - barWidth * 0.5;
      const f64 xRight  = xCenter + barWidth * 0.5;
      const f64 yBarTop = std::max(static_cast<f64>(mYBand8Buffer[b]), static_cast<f64>(yFloor));

      ptsUpper.append(QPointF(xLeft, yFloor));
      ptsUpper.append(QPointF(xLeft, yBarTop));
      ptsUpper.append(QPointF(xRight, yBarTop));
      ptsUpper.append(QPointF(xRight, yFloor));
    }
    ptsUpper.append(QPointF(8.5, yFloor));
    mpUpperCurve->replace(ptsUpper);

    QList<QPointF> ptsLower;
    ptsLower.reserve(2);
    ptsLower.append(QPointF(0.5, yFloor));
    ptsLower.append(QPointF(8.5, yFloor));
    mpLowerCurve->replace(ptsLower);
    break;
  }
  case EAudioPlotMode::OFF:
    break;
  }
}
