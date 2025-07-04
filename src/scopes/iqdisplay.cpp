/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2008, 2009, 2010, 2011, 2012, 2013
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
#include "iqdisplay.h"
#include "spectrogramdata.h"
#include <QwtLinearColorMap>

IQDisplay::IQDisplay(QwtPlot * const ipPlot)
  : QwtPlotSpectrogram()
{
  auto * const colorMap = new QwtLinearColorMap(QColor(0, 0, 255, 20), QColor(255, 255, 178, 255));

  setRenderThreadCount(1);
  mPlotgrid = ipPlot;
  setColorMap(colorMap);
  mPlotDataBackgroundBuffer.resize(2 * cRadius * 2 * cRadius, 0.0);
  mPlotDataDrawBuffer.resize(2 * cRadius * 2 * cRadius, 0.0);
  mpIQData = new SpectrogramData(mPlotDataDrawBuffer.data(), 0, 2 * cRadius, 2 * cRadius, 2 * cRadius);
  mpIQData->set_min_max_z_value(0.0, 50.0);
  setData(mpIQData);
  ipPlot->enableAxis(QwtPlot::xBottom, false);
  ipPlot->enableAxis(QwtPlot::yLeft, false);
  setDisplayMode(QwtPlotSpectrogram::ImageMode, true);

  select_plot_type(EIqPlotType::DEFAULT);
  attach(mPlotgrid);

  mPlotgrid->replot();
}

IQDisplay::~IQDisplay()
{
  detach();
  // delete mIQData; // delete here will cause an exception
}

inline void IQDisplay::set_point(const i32 iX, const i32 iY, i32 iVal)
{
  i32 x, y;

  if (mMap1stQuad)
  {
    if (iX < 0)
    {
      if (iY < 0) { x = -iX; y = -iY; }
      else        { x =  iY; y = -iX; }
    }
    else // ix >= 0
    {
      if (iY < 0) { x = -iY; y = iX; }
      else        { x =  iX; y = iY; }
    }

    limit_symmetrically(x, 2 * cRadius - 1);
    limit_symmetrically(y, 2 * cRadius - 1);

    mPlotDataBackgroundBuffer[y * 2 * cRadius + x] = iVal;
  }
  else
  {
    x = iX;
    y = iY;

    limit_symmetrically(x, cRadius - 1);
    limit_symmetrically(y, cRadius - 1);

    mPlotDataBackgroundBuffer[(y + cRadius - 1) * 2 * cRadius + x + cRadius - 1] = iVal;
  }
}

void IQDisplay::display_iq(const std::vector<cf32> & iIQ, const f32 iScale)
{
  if (iIQ.size() != mPoints.size())
  {
    mPoints.resize(iIQ.size(), { 0, 0 });
  }

  const f32 scale = std::pow(10.0f, 2.0f * iScale - 1.0f); // scale [0.0..1.0] to [0.1..10.0]
  const f32 scaleNormed = scale * mRadius;

  clean_screen_from_old_data_points();
  draw_cross();
  repaint_circle(scale);

  for (u32 i = 0; i < iIQ.size(); i++)
  {
    auto x = (i32)(scaleNormed * real(iIQ[i]));
    auto y = (i32)(scaleNormed * imag(iIQ[i]));

    mPoints[i] = std::complex<i32>(x, y);
    set_point(x, y, 100);
  }

  constexpr i32 elemSize = sizeof(decltype(mPlotDataBackgroundBuffer.back()));
  memcpy(mPlotDataDrawBuffer.data(), mPlotDataBackgroundBuffer.data(), 2 * 2 * cRadius * cRadius * elemSize);

  mPlotgrid->replot();
}

void IQDisplay::clean_screen_from_old_data_points()
{
  for (const auto & p : mPoints)
  {
    set_point(real(p), imag(p), 0);
  }
}

void IQDisplay::draw_cross()
{
  if (mMap1stQuad)
  {
    for (i32 i = 0; i < 2 * cRadius; i++)
    {
      set_point(0, i, 20); // vertical line
      set_point(i, 0, 20); // horizontal line
    }
  }
  else
  {
    for (i32 i = -(cRadius - 1); i < cRadius; i++)
    {
      set_point(0, i, 20); // vertical line
      set_point(i, 0, 20); // horizontal line
    }
  }
}

void IQDisplay::draw_circle(const f32 iScale, const i32 iVal)
{
  const i32 MAX_CIRCLE_POINTS = static_cast<i32>(180 * iScale); // per quarter

  for (i32 i = 0; i < MAX_CIRCLE_POINTS; ++i)
  {
    const f32 phase = 0.5f * (f32)M_PI * (f32)i / MAX_CIRCLE_POINTS;

    auto h = (i32)(mRadius * iScale * std::cos(phase));
    auto v = (i32)(mRadius * iScale * std::sin(phase));

    // as h and v covers only the top right segment, fill also other segments
    if (!mMap1stQuad)
    {
      set_point(-h, -v, iVal);
      set_point(-h, +v, iVal);
      set_point(+h, -v, iVal);
    }
    set_point(+h, +v, iVal);
  }
}

void IQDisplay::repaint_circle(const f32 iSize)
{
  if (iSize != mLastCircleSize)
  {
    draw_circle(mLastCircleSize, 0); // clear old circle
    mLastCircleSize = iSize;
  }
  draw_circle(iSize, 20);
}

void IQDisplay::select_plot_type(const EIqPlotType iPlotType)
{
  customize_plot(_get_plot_type_data(iPlotType));
}

void IQDisplay::set_map_1st_quad(bool iMap1stQuad)
{
  mMap1stQuad = iMap1stQuad;
  mRadius = (f32)cRadius * (iMap1stQuad ? 2.0f : 1.0f);
  std::fill(mPlotDataBackgroundBuffer.begin(), mPlotDataBackgroundBuffer.end(), 0.0);
}

void IQDisplay::customize_plot(const SCustPlot & iCustPlot)
{
  mPlotgrid->setToolTip(iCustPlot.ToolTip);
}

IQDisplay::SCustPlot IQDisplay::_get_plot_type_data(const EIqPlotType iPlotType)
{
  SCustPlot cp;
  cp.PlotType = iPlotType;

  switch (iPlotType)
  {
  case EIqPlotType::PHASE_CORR_CARR_NORMED:
    cp.ToolTip = "Shows the phase-corrected PI/4-DPSK decoded signal normed to each OFDM-carrier level.";
    cp.Name = "Phase Corr. Carr.";
    break;

  case EIqPlotType::PHASE_CORR_MEAN_NORMED:
    cp.ToolTip = "Shows the phase-corrected PI/4-DPSK decoded signal normed to the mean over all OFDM-carrier levels.";
    cp.Name = "Phase Corr. Mean";
    break;

  case EIqPlotType::RAW_MEAN_NORMED:
    cp.ToolTip = "Shows the real (no phase corrected) PI/4-DPSK decoded signal normed to the mean over all OFDM-carrier levels.";
    cp.Name = "Real Mean";
    break;

  case EIqPlotType::DC_OFFSET_FFT_100:
    cp.ToolTip = "Shows the DC offset of the input signal after OFDM-FFT at FFT-bin 0. See the effect of 'DC removal filter'. The trace is the distance to the last OFDM-symbol. The value is 100 times magnified.";
    cp.Name = "DC Off. (FFT) 100x";
    break;

  case EIqPlotType::DC_OFFSET_ADC_100:
    cp.ToolTip = "Shows the DC offset of the input signal after ADC (before DC removal filtering). The 'DC removal filter' has to be activated to see changes. The value is 100 times magnified.";
    cp.Name = "DC Off. (ADC) 100x";
    break;
  }
  return cp;
}

QStringList IQDisplay::get_plot_type_names()
{
  QStringList sl;

  // ATTENTION: use same sequence as in EIqPlotType
  sl << _get_plot_type_data(EIqPlotType::PHASE_CORR_CARR_NORMED).Name;
  sl << _get_plot_type_data(EIqPlotType::PHASE_CORR_MEAN_NORMED).Name;
  sl << _get_plot_type_data(EIqPlotType::RAW_MEAN_NORMED).Name;
  sl << _get_plot_type_data(EIqPlotType::DC_OFFSET_FFT_100).Name;
  sl << _get_plot_type_data(EIqPlotType::DC_OFFSET_ADC_100).Name;

  return sl;
}
