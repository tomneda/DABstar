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

#include "waterfall-scope.h"
#include "glob_defs.h"

WaterfallScope::WaterfallScope(QwtPlot * scope, int displaySize, int rasterSize) :
  QwtPlotSpectrogram(),
  mpPlotgrid(scope),
  mDisplaySize(displaySize),
  mRasterSize(rasterSize)
{
  mPlotDataVec.resize(displaySize * rasterSize);
  std::fill(mPlotDataVec.begin(), mPlotDataVec.end(), 0.0);

  _gen_color_map(0);

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

void WaterfallScope::show_waterfall(const double * ipX_axis, const double * ipY1_value, double iAmp)
{
  const int32_t orig = (int32_t)(ipX_axis[0]);

  if (const int32_t width = (int32_t)(ipX_axis[mDisplaySize - 1] - orig);
      mOrig != orig || mWidth != width || mAmp != iAmp)
  {
    mpWaterfallData = new SpectrogramData(mPlotDataVec.data(), orig, width, -mRasterSize, mDisplaySize, iAmp / 2);
    detach();
    setData(mpWaterfallData);
    attach(mpPlotgrid);
    mOrig = orig;
    mWidth = width;
    mAmp = iAmp;
  }

  constexpr int32_t elemSize = sizeof(decltype(mPlotDataVec.back()));
  memmove(&mPlotDataVec[mDisplaySize], &mPlotDataVec[0], (mRasterSize - 1) * mDisplaySize * elemSize); // move rows one row further

  for (int i = 0; i < mDisplaySize; ++i)
  {
    mPlotDataVec[i] = 20.0 * std::log10(ipY1_value[i] + 1.0);
  }

  mpPlotgrid->setAxisScale(QwtPlot::xBottom, ipX_axis[0], ipX_axis[mDisplaySize - 1]); // for plot
  mpPlotgrid->setAxisScale(QwtPlot::xTop, ipX_axis[0], ipX_axis[mDisplaySize - 1]); // for ticks

  mpPlotgrid->replot();
}

void WaterfallScope::set_bit_depth(int32_t d)
{
  mNormalizer = get_range_from_bit_depth(d);
}

void WaterfallScope::_gen_color_map(const int32_t iStyleNr)
{
  switch (iStyleNr)
  {
  case 0:
  {
    mpColorMap = new QwtLinearColorMap(Qt::black, Qt::white);
    mpColorMap->addColorStop(0.2, Qt::darkBlue);
    mpColorMap->addColorStop(0.4, Qt::blue);
    mpColorMap->addColorStop(0.6, Qt::yellow);
    mpColorMap->addColorStop(0.8, Qt::red);
    break;
  }

  case 1:
  {
    mpColorMap = new QwtLinearColorMap(Qt::black, Qt::white);
    mpColorMap->addColorStop(0.2, 0x2020A0);
    mpColorMap->addColorStop(0.4, 0xFF2020);
    mpColorMap->addColorStop(0.6, 0xFF40FF);
    mpColorMap->addColorStop(0.8, 0xFFFF40);
    break;
  }

  case 2:
  {
    constexpr int32_t COL_STEPS = 16;

    const int32_t R_start = 0x40;
    const int32_t G_start = 0x00;
    const int32_t B_start = 0x00;

    const int32_t R_stop = 0xA0;
    const int32_t G_stop = 0x80;
    const int32_t B_stop = 0xFF;

    const double R_step = (double)(R_stop - R_start) / COL_STEPS;
    const double G_step = (double)(G_stop - G_start) / COL_STEPS;
    const double B_step = (double)(B_stop - B_start) / COL_STEPS;

    mpColorMap = new QwtLinearColorMap(Qt::black, R_stop << 16 | G_stop << 8 | B_stop);

    for (int colIdx = 1; colIdx < COL_STEPS; ++colIdx)
    {
      const int32_t R_val = (int32_t)(R_step * colIdx) + R_start;
      const int32_t G_val = (int32_t)(G_step * colIdx) + G_start;
      const int32_t B_val = (int32_t)(B_step * colIdx) + B_start;
      const int32_t col_val = R_val << 16 | G_val << 8 | B_val;
      mpColorMap->addColorStop((double)colIdx / COL_STEPS, col_val);
    }
    break;
  }
  default: qFatal("Invalid waterfall color style number %d", iStyleNr);
  }

  setColorMap(mpColorMap);
}
