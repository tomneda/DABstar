//
// Created by work on 7/2/23.
//

#include  <cstdio>
#include  <cstdlib>
#include  <qwt_interval.h>
#include  <QPen>
#include  <cmath>
#include "spectrogramdata.h"


SpectrogramData::SpectrogramData(const double * ipData, int32_t iLeft, int32_t iWidth, int32_t iHeight, int32_t iDatawidth) :
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

double SpectrogramData::value(const double iX, const double iY) const
{
  const int32_t idxX = (int32_t)((iX - mLeft) / mWidth * (mDataWidth - 1));
  const int32_t idxY = (int32_t)(iY / mHeight * (mDataHeight - 1));

  assert(idxX >= 0);
  assert(idxX < mDataWidth);
  assert(idxY >= 0);
  assert(idxY < mDataHeight);

  return mpData[idxY * mDataWidth + idxX];
}

void SpectrogramData::set_min_max_z_value(const double izMin, const double izMax)
{
#if ((QWT_VERSION >> 8) < 0x0602)
  setInterval(Qt::ZAxis, QwtInterval(izMin, izMax));
#else
  mzMin = izMin;
  mzMax = izMax;
#endif
}
