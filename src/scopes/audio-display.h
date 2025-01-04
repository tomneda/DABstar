/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2019
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB.
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

#ifndef    AUDIO_DISPLAY_H
#define    AUDIO_DISPLAY_H

#include  "dab-constants.h"
#include  <array>
#include  <QSettings>
#include  <QObject>
#include  <qwt_plot.h>
#include  <qwt_plot_marker.h>
#include  <qwt_plot_grid.h>
#include  <qwt_plot_curve.h>
#include  <qwt_color_map.h>
#include  <qwt_scale_widget.h>
#include  <fftw3.h>

class RadioInterface;

class AudioDisplay : public QObject
{
Q_OBJECT
public:
  AudioDisplay(RadioInterface *, QwtPlot *, QSettings *);
  ~AudioDisplay() override;

  void create_spectrum(const int16_t *, int, int);

private:
  static constexpr char SETTING_GROUP_NAME[] = "audioDisplay";
  static constexpr int32_t cSpectrumSize = 512;
  static constexpr int32_t cNormalizer = 32378;
  static constexpr int32_t cDisplaySize = cSpectrumSize / 2;  // we use only the right half of the FFT

  RadioInterface * const mpRadioInterface;
  QSettings * const mpDabSettings;
  QwtPlot * const pPlotGrid;
  QwtPlotCurve mSpectrumCurve{""};
  QwtPlotGrid mGrid;

  std::array<float, cDisplaySize>  mXDispBuffer{};
  std::array<float, cDisplaySize>  mYDispBuffer{};
  std::array<float, cSpectrumSize> mWindow;

  std::array<float, cSpectrumSize> mFftInBuffer;
  std::array<cmplx, cSpectrumSize> mFftOutBuffer;
  fftwf_plan mFftPlan{fftwf_plan_dft_r2c_1d(cSpectrumSize, mFftInBuffer.data(), (fftwf_complex *)mFftOutBuffer.data(), FFTW_ESTIMATE)};

  QColor GridColor;
  QColor mCurveColor;
  int32_t mSampleRateLast = 0;

private slots:
  void _slot_rightMouseClick(const QPointF &);
};

#endif

