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

#include "waterfall_scope.h"
#include <QPainter>
#include <QPaintEvent>
#include <algorithm>
#include <cstring>

WaterfallScope::WaterfallScope(QWidget * const parent)
  : QWidget(parent)
{
  _build_color_lut();
}

void WaterfallScope::init(const i32 iDisplaySize, const i32 iRasterSize)
{
  mDisplaySize = iDisplaySize;
  mRasterSize  = iRasterSize;
  mImage = QImage(iDisplaySize, iRasterSize, QImage::Format_RGB32);
  mImage.fill(Qt::black);
}

void WaterfallScope::show_waterfall(const f32 * ipY1_value, const SpecViewLimits<f32> & iSpecViewLimits)
{
  if (mDisplaySize == 0 || mRasterSize == 0) return;

  // Compute dynamic scale range
  const f32 yMax = (iSpecViewLimits.Glob.Max + 5.0f) * (1.0f - mScale) + iSpecViewLimits.Loc.Max * mScale;
  const f32 yMin = iSpecViewLimits.Glob.Min * (1.0f - mScale) + iSpecViewLimits.Loc.Min * mScale;
  const f32 yRange = yMax - yMin;
  if (yRange <= 0.0f) return;

  // Shift image down by one row (new data goes at row 0)
  const i32 rowBytes = mDisplaySize * (i32)sizeof(QRgb);
  for (i32 row = mRasterSize - 1; row > 0; --row)
  {
    memcpy(mImage.scanLine(row), mImage.scanLine(row - 1), (size_t)rowBytes);
  }

  // Map new data row to colours at row 0
  QRgb * const topRow = reinterpret_cast<QRgb *>(mImage.scanLine(0));
  for (i32 x = 0; x < mDisplaySize; ++x)
  {
    const f32 t = std::clamp((ipY1_value[x] - yMin) / yRange, 0.0f, 1.0f);
    const i32 lutIdx = (i32)(t * 255.0f);
    topRow[x] = mColorLut[(size_t)lutIdx];
  }

  update();
}

void WaterfallScope::slot_scaling_changed(const i32 iScale)
{
  mScale = (f32)iScale / 100.0f;
}

void WaterfallScope::slot_set_horizontal_margins(const int iLeft, const int iRight)
{
  mLeftMargin  = iLeft;
  mRightMargin = iRight;
  update();
}

void WaterfallScope::paintEvent(QPaintEvent * const /*ipEvent*/)
{
  QPainter painter(this);
  painter.fillRect(rect(), Qt::black);

  if (!mImage.isNull())
  {
    painter.drawImage(rect().adjusted(mLeftMargin, 0, -mRightMargin, 0), mImage);
  }
}

void WaterfallScope::_build_color_lut()
{
  // Colour stops
  //   0.0 → black
  //   0.2 → dark blue
  //   0.4 → blue
  //   0.6 → yellow
  //   0.8 → red
  //   1.0 → white
  struct Stop { f32 pos; int r, g, b; };
  static constexpr Stop stops[] = {
    { 0.0f, 0,   0,   0   },
    { 0.2f, 0,   0,   128 },
    { 0.4f, 0,   0,   255 },
    { 0.6f, 255, 255, 0   },
    { 0.8f, 255, 0,   0   },
    { 1.0f, 255, 255, 255 },
  };
  constexpr i32 nStops = (i32)(sizeof(stops) / sizeof(stops[0]));

  for (i32 i = 0; i < 256; ++i)
  {
    const f32 t = (f32)i / 255.0f;

    // find segment
    i32 seg = 0;
    for (i32 s = 1; s < nStops - 1; ++s)
    {
      if (t >= stops[s].pos) seg = s;
    }

    const f32 t0 = stops[seg].pos;
    const f32 t1 = stops[seg + 1].pos;
    const f32 frac = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;

    const int r = (int)(stops[seg].r + frac * (stops[seg + 1].r - stops[seg].r));
    const int g = (int)(stops[seg].g + frac * (stops[seg + 1].g - stops[seg].g));
    const int b = (int)(stops[seg].b + frac * (stops[seg + 1].b - stops[seg].b));

    mColorLut[(size_t)i] = qRgb(r, g, b);
  }
}
