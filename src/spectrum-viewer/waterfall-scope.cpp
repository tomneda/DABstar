/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C)  2012, 2013, 2014
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

#include  "waterfall-scope.h"

WaterfallScope::WaterfallScope(QwtPlot * scope, int displaySize, int rasterSize) :
  QwtPlotSpectrogram(),
  mpPlotgrid(scope),
  mDisplaySize(displaySize),
  mRasterSize(rasterSize)
{
  mPlotDataVec.resize(displaySize * rasterSize);
  std::fill(mPlotDataVec.begin(), mPlotDataVec.end(), 0.0);

  mpColorMap = new QwtLinearColorMap(Qt::darkCyan, Qt::red);
  mpColorMap->addColorStop(0.1, Qt::cyan);
  mpColorMap->addColorStop(0.4, Qt::green);
  mpColorMap->addColorStop(0.7, Qt::yellow);
  setColorMap(mpColorMap);

  setDisplayMode(QwtPlotSpectrogram::ImageMode, true);

  mpPlotgrid->enableAxis(QwtPlot::yLeft, true);
  mpPlotgrid->setAxisScale(QwtPlot::yLeft, -rasterSize, 0);
  mpPlotgrid->enableAxis(QwtPlot::xBottom, false);
  mpPlotgrid->enableAxis(QwtPlot::xTop, true);
  mpPlotgrid->axisScaleDraw(QwtPlot::xTop)->enableComponent(QwtAbstractScaleDraw::Labels, false); // disable labels (labels from spectrum used)

  mpPlotgrid->replot();
}

WaterfallScope::~WaterfallScope()
{
  detach();
}

void WaterfallScope::show_waterfall(const double * X_axis, const double * Y1_value, double amp)
{
  const int32_t orig = (int32_t)(X_axis[0]);
  const int32_t width = (int32_t)(X_axis[mDisplaySize - 1] - orig);

  //invalidateCache();
  if (mOrig != orig || mWidth != width || mAmp != amp)
  {
    if (mOrig != orig) // clean screen only on frequency change
    {
      std::fill(mPlotDataVec.begin(), mPlotDataVec.end(), 0.0);
    }
    mpWaterfallData = new SpectrogramData(mPlotDataVec.data(), orig, width, -mRasterSize, mDisplaySize, amp / 2);
    detach();
    setData(mpWaterfallData);
    attach(mpPlotgrid);
    mOrig = orig;
    mWidth = width;
    mAmp = amp;
  }

  constexpr int32_t elemSize = sizeof(decltype(mPlotDataVec.back()));
  memmove(&mPlotDataVec[mDisplaySize], &mPlotDataVec[0], (mRasterSize - 1) * mDisplaySize * elemSize); // move rows one row further

  for (int i = 0; i < mDisplaySize; ++i)
  {
    mPlotDataVec[i] = 20.0 * std::log10(Y1_value[i] + 1.0);
  }

  mpPlotgrid->setAxisScale(QwtPlot::xBottom, X_axis[0], X_axis[mDisplaySize - 1]); // for plot
  mpPlotgrid->setAxisScale(QwtPlot::xTop,    X_axis[0], X_axis[mDisplaySize - 1]); // for ticks

  mpPlotgrid->replot();
}

void WaterfallScope::set_bit_depth(int32_t n)
{
  mNormalizer = n;
}
