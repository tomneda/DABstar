/*
 * Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "iqdisplay.h"
#include "glob_defs.h"
#include <QPainter>
#include <QPaintEvent>
#include <cmath>
#include <cstring>

IQDisplay::IQDisplay(QWidget * const parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent);
  mBackgroundImage = QImage(c2Radius, c2Radius, QImage::Format_RGB32);
  mImage = QImage(c2Radius, c2Radius, QImage::Format_RGB32);

  // IQ dot color: fully-opaque yellowish-white
  mDotColor = qRgb(255, 255, 178);

  // Per-row background gradient matching the global application stylesheet:
  //   top (y=0)        → rgb(58, 58, 58)
  //   bottom (y=cDim)  → rgb(18, 18, 18)
  for (i32 y = 0; y < c2Radius; ++y)
  {
    const float t = (float)y / (float)(c2Radius - 1);
    const int v = (int)(58.0f + t * (18.0f - 58.0f));
    mBgGradient[(size_t)y] = qRgb(v, v, v);
  }

  for (i32 y = 0; y < c2Radius; ++y)
  {
    const QRgb bg = mBgGradient[(size_t)y];
    const int r = (114 * 102 + 141 * qRed(bg))   / 255;
    const int g = (114 * 102 + 141 * qGreen(bg)) / 255;
    const int b = (114 * 224 + 141 * qBlue(bg))  / 255;
    mCrossGradient[(size_t)y] = qRgb(r, g, b);
  }

  _render_background(0.0f);
  select_plot_type(EIqPlotType::DEFAULT);
}

inline void IQDisplay::_set_base_point(const i32 iX, const i32 iY)
{
  i32 x, y;
  i32 bufX, bufY;

  if (mMap1stQuad)
  {
    if (iX < 0)
    {
      if (iY < 0) { x = -iX; y = -iY; }
      else        { x =  iY; y = -iX; }
    }
    else
    {
      if (iY < 0) { x = -iY; y =  iX; }
      else        { x =  iX; y =  iY; }
    }

    limit_symmetrically(x, c2Radius - 1);
    limit_symmetrically(y, c2Radius - 1);

    bufX = x;
    bufY = c2Radius - 1 - y;
  }
  else
  {
    x = iX;
    y = iY;

    limit_symmetrically(x, cRadius - 1);
    limit_symmetrically(y, cRadius - 1);

    bufX = x + cRadius - 1;
    bufY = cRadius - 1 - y;
  }

  reinterpret_cast<QRgb *>(mBackgroundImage.scanLine(bufY))[bufX] = mCrossGradient[(size_t)bufY];
}

void IQDisplay::_render_background(const f32 iScale)
{
  // 1. Fill background gradient
  for (i32 y = 0; y < c2Radius; ++y)
  {
    QRgb * const line = reinterpret_cast<QRgb *>(mBackgroundImage.scanLine(y));
    const QRgb bg = mBgGradient[(size_t)y];
    for (i32 x = 0; x < c2Radius; ++x)
    {
      line[x] = bg;
    }
  }

  // 2. Draw cross
  if (mMap1stQuad)
  {
    for (i32 i = 0; i < c2Radius; ++i)
    {
      _set_base_point(0, i);
      _set_base_point(i, 0);
    }
  }
  else
  {
    for (i32 i = -(cRadius - 1); i < cRadius; ++i)
    {
      _set_base_point(0, i);
      _set_base_point(i, 0);
    }
  }

  // 3. Draw circle
  if (iScale > 0.0f)
  {
    const i32 maxCirclePoints = (i32)(180 * iScale);
    for (i32 i = 0; i < maxCirclePoints; ++i)
    {
      const f32 phase = 0.5f * (f32)M_PI * (f32)i / maxCirclePoints;
      const auto h = (i32)(mRadius * iScale * std::cos(phase));
      const auto v = (i32)(mRadius * iScale * std::sin(phase));

      if (!mMap1stQuad)
      {
        _set_base_point(-h, -v);
        _set_base_point(-h, +v);
        _set_base_point(+h, -v);
      }
      _set_base_point(+h, +v);
    }
  }
}

void IQDisplay::display_iq(const std::vector<cf32> & iIQ, const f32 iScale)
{
  const f32 scale = std::pow(10.0f, 2.0f * iScale - 1.0f);
  if (scale != mLastCircleSize)
  {
    mLastCircleSize = scale;
    _render_background(scale);
  }

  const f32 scaleNormed = scale * mRadius;

  // Copy base image (gradient + cross + circle)
  std::memcpy(mImage.bits(), mBackgroundImage.constBits(), mImage.sizeInBytes());

  // Render dots directly onto mImage
  if (mMap1stQuad)
  {
    for (const auto & iq : iIQ)
    {
      const auto iX = (i32)(scaleNormed * real(iq));
      const auto iY = (i32)(scaleNormed * imag(iq));

      i32 x, y;
      if (iX < 0)
      {
        if (iY < 0) { x = -iX; y = -iY; }
        else        { x =  iY; y = -iX; }
      }
      else
      {
        if (iY < 0) { x = -iY; y =  iX; }
        else        { x =  iX; y =  iY; }
      }

      limit_symmetrically(x, c2Radius - 1);
      limit_symmetrically(y, c2Radius - 1);

      reinterpret_cast<QRgb *>(mImage.scanLine(c2Radius - 1 - y))[x] = mDotColor;
    }
  }
  else
  {
    for (const auto & iq : iIQ)
    {
      auto x = (i32)(scaleNormed * real(iq));
      auto y = (i32)(scaleNormed * imag(iq));

      limit_symmetrically(x, cRadius - 1);
      limit_symmetrically(y, cRadius - 1);

      reinterpret_cast<QRgb *>(mImage.scanLine(cRadius - 1 - y))[x + cRadius - 1] = mDotColor;
    }
  }

  update();
}

void IQDisplay::paintEvent(QPaintEvent * const /*ipEvent*/)
{
  QPainter painter(this);
  // Scale the 200×200 image to fill the widget (keeps square if widget is square)
  painter.drawImage(rect(), mImage);
}

void IQDisplay::select_plot_type(const EIqPlotType iPlotType)
{
  if (mPlotType != iPlotType)
  {
    mPlotType = iPlotType;
    const SCustPlot cp = _get_plot_type_data(iPlotType);
    setToolTip(cp.ToolTip);
  }
}

void IQDisplay::set_map_1st_quad(const bool iMap1stQuad)
{
  if (mMap1stQuad != iMap1stQuad)
  {
    mMap1stQuad = iMap1stQuad;
    mRadius = (f32)cRadius * (iMap1stQuad ? 2.0f : 1.0f);
    _render_background(mLastCircleSize > 0.0f ? mLastCircleSize : 0.0f);
  }
}

IQDisplay::SCustPlot IQDisplay::_get_plot_type_data(const EIqPlotType iPlotType)
{
  SCustPlot cp{};
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

  default:
    cp.ToolTip = "";
    cp.Name = "";
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
