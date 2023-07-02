/*
 *    Copyright (C) 2010, 2011, 2012
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the SDR-J.
 *    Many of the ideas as implemented in SDR-J are derived from
 *    other work, made available through the GNU general Public License. 
 *    All copyrights of the original authors are recognized.
 *
 *    SDR-J is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    SDR-J is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with SDR-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef  SPECTROGRAM_H
#define  SPECTROGRAM_H

#include  <qwt_raster_data.h>

class SpectrogramData : public QwtRasterData
{
public:
  const double * const data;   // pointer to actual data
  const int left;    // index of left most element in raster
  const int width;    // raster width
  const int height;    // rasterheigth
  const int datawidth;  // width of matrix
  const int dataheight;  // for now == rasterheigth
  const double max;

  SpectrogramData(double * ipData, int iLeft, int iWidth, int iHeight, int iDatawidth, double iMax);
  ~SpectrogramData() override = default;

  void initRaster(const QRectF & x, const QSize & raster) override;
  [[nodiscard]] QwtInterval interval(Qt::Axis x) const override;
  [[nodiscard]] double value(double x, double y) const override;
};

#endif

