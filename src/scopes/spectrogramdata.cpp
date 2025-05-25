//
// Created by work on 7/2/23.
//

#include  <cstdio>
#include  <cstdlib>
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
