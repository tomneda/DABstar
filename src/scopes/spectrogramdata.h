/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2010, 2011, 2012
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
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

#include "glob_data_types.h"
#include  <qwt_raster_data.h>

#if !defined(QWT_VERSION)
  error "QWT_VERSION not defined"
#endif

class SpectrogramData : public QwtRasterData
{
public:
  const f64 * const mpData;  // pointer to actual data
  const f64 mLeft;           // index of left most element in raster
  const f64 mWidth;          // raster width
  const f64 mHeight;         // raster height
  const i32 mDataWidth;     // width of matrix
  const i32 mDataHeight;    // for now == raster height
#if ((QWT_VERSION >> 8) >= 0x0602)
  f64 mzMin = 0.0;
  f64 mzMax = 1.0;
#endif

  SpectrogramData(const f64 * ipData, i32 iLeft, i32 iWidth, i32 iHeight, i32 iDatawidth);
  ~SpectrogramData() override = default;

  void set_min_max_z_value(const f64 izMin, const f64 izMax);

  void initRaster(const QRectF & x, const QSize & raster) override;

  f64 value(f64 iX, f64 iY) const override;

  // unfortunately the older Qwt interface provides no override-able virtual interface interval() -> do it another way
#if ((QWT_VERSION >> 8) >= 0x0602)
  QwtInterval interval(Qt::Axis x) const override;
#endif
};

#endif

