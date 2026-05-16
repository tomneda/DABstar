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

#include  <qwt_interval.h>
#include  <QPen>
#include  <cmath>
#include "spectrogramdata.h"


SpectrogramData::SpectrogramData(const f64 * ipData, i32 iLeft, i32 iWidth, i32 iHeight, i32 iDatawidth) :
  QwtRasterData(),
  mpData(ipData),
  mLeft(iLeft),
  mWidth(iWidth),
  mHeight(iHeight),
  mDataWidth(std::abs(iDatawidth)),
  mDataHeight(std::abs(iHeight))
{
#if ((QWT_VERSION >> 8) < 0x0602)
  setInterval(Qt::XAxis, QwtInterval(iLeft, iLeft + iWidth));
  setInterval(Qt::YAxis, QwtInterval(0, iHeight));
#endif
  assert(mWidth != 0);
  assert(mHeight != 0);
}

void SpectrogramData::initRaster(const QRectF & x, const QSize & raster)
{
  (void)x;
  (void)raster;
}

#if ((QWT_VERSION >> 8) >= 0x0602)
QwtInterval SpectrogramData::interval(Qt::Axis x) const
{
  if (x == Qt::XAxis)
  {
    return {mLeft, mLeft + mWidth};
  }

  if (x == Qt::YAxis)
  {
    return {0, mHeight};
  }

  return {mzMin, mzMax};
}
#endif

f64 SpectrogramData::value(const f64 iX, const f64 iY) const
{
  const i32 idxX = (i32)((iX - mLeft) / mWidth * (mDataWidth - 1));
  const i32 idxY = (i32)(iY / mHeight * (mDataHeight - 1));

  assert(idxX >= 0);
  assert(idxX < mDataWidth);
  assert(idxY >= 0);
  assert(idxY < mDataHeight);

  return mpData[idxY * mDataWidth + idxX];
}

void SpectrogramData::set_min_max_z_value(const f64 izMin, const f64 izMax)
{
#if ((QWT_VERSION >> 8) < 0x0602)
  setInterval(Qt::ZAxis, QwtInterval(izMin, izMax));
#else
  mzMin = izMin;
  mzMax = izMax;
#endif
}
