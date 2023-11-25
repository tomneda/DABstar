/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2008, 2009, 2010
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

#ifndef  WATERFALLSCOPE_H
#define  WATERFALLSCOPE_H

#include <QObject>
#include <qwt_plot_spectrogram.h>
#include <cstdint>
#include "spectrogramdata.h"
#include "dab-constants.h"

class QwtLinearColorMap;

class WaterfallScope : public QObject, public QwtPlotSpectrogram
{
Q_OBJECT
public:
  WaterfallScope(QwtPlot *, int, int);
  ~WaterfallScope() override;

  void show_waterfall(const double *, const double *, const SpecViewLimits<double> & iSpecViewLimits);

private:
  QwtPlot * const mpPlotgrid;
  const int32_t mDisplaySize;
  const int32_t mRasterSize;
  int32_t mOrig = 0;
  int32_t mWidth = 0;
  SpectrogramData * mpWaterfallData;
  QwtLinearColorMap * mpColorMap;
  std::vector<double> mPlotDataVec;
  double mScale = 0.0;

  void _gen_color_map(const int32_t iStyleNr);
public slots:
  void slot_scaling_changed(int);
};

#endif
