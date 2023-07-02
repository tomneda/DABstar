//
// Created by work on 7/2/23.
//

#include  <cstdio>
#include  <cstdlib>
#include  <qwt_interval.h>
#include  <QPen>
#include  <cmath>
#include "spectrogramdata.h"


SpectrogramData::SpectrogramData(double * ipData, int iLeft, int iWidth, int iHeight, int iDatawidth, double iMax) :
  QwtRasterData(),
  data(ipData),
  left(iLeft),
  width(iWidth),
  height(iHeight),
  datawidth(iDatawidth),
  dataheight(iHeight),
  max(iMax)
{
  assert(width > 0);
  assert(height > 0);
#if defined QWT_VERSION && ((QWT_VERSION >> 8) < 0x0602)
  setInterval (Qt::XAxis, QwtInterval (left, left + width));
  setInterval (Qt::YAxis, QwtInterval (0, height));
  setInterval (Qt::ZAxis, QwtInterval (0, max));
#endif
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
    return QwtInterval(left, left + width);
  }

  if (x == Qt::YAxis)
  {
    return QwtInterval(0, height);
  }

  return QwtInterval(0, max);
}

double SpectrogramData::value(double x, double y) const
{
  //fprintf (stderr, "x = %f, y = %f\n", x, y);
  x = x - left;
  x = x / width * (datawidth - 1);
  y = y / height * (dataheight - 1);
 
  return data[(int)y * datawidth + (int)x];
}
