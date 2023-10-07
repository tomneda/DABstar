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
#include  "iqdisplay.h"
#include  "spectrogramdata.h"

IQDisplay::IQDisplay(QwtPlot * plot)
  : QwtPlotSpectrogram()
{
  auto * const colorMap = new QwtLinearColorMap(QColor(0, 0, 255, 20), QColor(255, 255, 178, 255));

  setRenderThreadCount(1);
  mPlotgrid = plot;
  setColorMap(colorMap);
  mPlotDataBackgroundBuffer.resize(2 * RADIUS * 2 * RADIUS, 0.0);
  mPlotDataDrawBuffer.resize(2 * RADIUS * 2 * RADIUS, 0.0);
  mIQData = new SpectrogramData(mPlotDataDrawBuffer.data(), 0, 2 * RADIUS, 2 * RADIUS, 2 * RADIUS, 50.0);
  setData(mIQData);
  plot->enableAxis(QwtPlot::xBottom, false);
  plot->enableAxis(QwtPlot::yLeft, false);
  setDisplayMode(QwtPlotSpectrogram::ImageMode, true);

  select_plot_type(EIqPlotType::DEFAULT);
  attach(mPlotgrid);

  mPlotgrid->replot();
}

IQDisplay::~IQDisplay()
{
  detach();
  // delete mIQData;
}

inline void IQDisplay::set_point(int x, int y, int val)
{
  mPlotDataBackgroundBuffer[(y + RADIUS - 1) * 2 * RADIUS + x + RADIUS - 1] = val;
}

void IQDisplay::display_iq(const std::vector<cmplx> & z, float iScale)
{
  if (z.size() != mPoints.size())
  {
    mPoints.resize(z.size(), { 0, 0 });
  }

  const float scaleNormed = iScale * RADIUS;

  clean_screen_from_old_data_points();
  draw_cross();
  repaint_circle(iScale);

  for (uint32_t i = 0; i < z.size(); i++)
  {
    auto x = (int32_t)(scaleNormed * real(z[i]));
    auto y = (int32_t)(scaleNormed * imag(z[i]));

    limit_symmetrically(x, RADIUS - 1);
    limit_symmetrically(y, RADIUS - 1);

    mPoints[i] = std::complex<int32_t>(x, y);
    set_point(x, y, 100);
  }

  constexpr int32_t elemSize = sizeof(decltype(mPlotDataBackgroundBuffer.back()));
  memcpy(mPlotDataDrawBuffer.data(), mPlotDataBackgroundBuffer.data(), 2 * 2 * RADIUS * RADIUS * elemSize);

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
  for (int32_t i = -(RADIUS - 1); i < RADIUS; i++)
  {
    set_point(0, i, 20); // vertical line
    set_point(i, 0, 20); // horizontal line
  }
}

void IQDisplay::draw_circle(float scale, int val)
{
  const int32_t MAX_CIRCLE_POINTS = static_cast<int32_t>(180 * scale); // per quarter

  for (int32_t i = 0; i < MAX_CIRCLE_POINTS; ++i)
  {
    const float phase = 0.5f * (float)M_PI * (float)i / MAX_CIRCLE_POINTS;

    auto h = (int32_t)(RADIUS * scale * cosf(phase));
    auto v = (int32_t)(RADIUS * scale * sinf(phase));

    limit_symmetrically(h, RADIUS - 1);
    limit_symmetrically(v, RADIUS - 1);

    // as h and v covers only the top right segment, fill also other segments
    set_point(-h, -v, val);
    set_point(-h, +v, val);
    set_point(+h, -v, val);
    set_point(+h, +v, val);
  }
}

void IQDisplay::repaint_circle(float size)
{
  if (size != mLastCircleSize)
  {
    draw_circle(mLastCircleSize, 0); // clear old circle
    mLastCircleSize = size;
  }
  draw_circle(size, 20);
}

void IQDisplay::select_plot_type(const EIqPlotType iPlotType)
{
  customize_plot(_get_plot_type_data(iPlotType));
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
  case EIqPlotType::RAW_MEAN_NORMED:
    cp.ToolTip = "Shows the real (no phase corrected) PI/4-DPSK decoded signal normed to the mean over all OFDM-carrier levels.";
    cp.Name = "Real Mean";
    break;

  case EIqPlotType::PHASE_CORR_MEAN_NORMED:
    cp.ToolTip = "Shows the phase-corrected PI/4-DPSK decoded signal normed to the mean over all OFDM-carrier levels.";
    cp.Name = "Phase Corr. Mean";
    break;

  case EIqPlotType::PHASE_CORR_CARR_NORMED:
    cp.ToolTip = "Shows the phase-corrected PI/4-DPSK decoded signal normed to each OFDM-carrier level.";
    cp.Name = "Phase Corr. Carr.";
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

  return sl;
}
