//
// Created by work on 7/2/23.
//

#include  <cstdio>
#include  <cstdlib>
#include  <qwt_interval.h>
#include  <QPen>
#include  <cmath>
#include "spectrogramdata.h"


SpectrogramData::SpectrogramData(const double * ipData, int32_t iLeft, int32_t iWidth, int32_t iHeight, int32_t iDatawidth, double iMax) :
  QwtRasterData(),
  mpData(ipData),
  mLeft(iLeft),
  mWidth(iWidth),
  mHeight(iHeight),
  mDataWidth(std::abs(iDatawidth)),
  mDataHeight(std::abs(iHeight)),
  mMax(iMax)
{
  assert(mWidth != 0);
  assert(mHeight != 0);
}

void SpectrogramData::initRaster(const QRectF & x, const QSize & raster)
{
  (void)x;
  (void)raster;
}

QwtInterval SpectrogramData::interval(Qt::Axis x) const
{
  if (x == Qt::XAxis)
  {
    return QwtInterval(mLeft, mLeft + mWidth);
  }

  if (x == Qt::YAxis)
  {
    return QwtInterval(0, mHeight);
  }

  return QwtInterval(0, mMax);
}

double SpectrogramData::value(double x, double y) const
{
  //fprintf (stdout, "x = %f, y = %f\n", x, y);
  x = x - mLeft;
  x = x / mWidth * (mDataWidth - 1);
  y = y / mHeight * (mDataHeight - 1);

//  assert(x >= 0);
//  assert(x < mDataWidth);
//  assert(y >= 0);
//  assert(y < mDataHeight);

  if (x < 0 || x >= mDataWidth ||
      y < 0 || y >= mDataHeight)
  {
    return 0.0;
  }

  return mpData[(int)y * mDataWidth + (int)x];
}
