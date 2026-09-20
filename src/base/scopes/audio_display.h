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
#pragma once

#include "dab_constants.h"
#include <array>
#include <QSettings>
#include <QObject>
#include <QLineSeries>
#include <QAreaSeries>
#include <QStringList>
#include <fftw3.h>

#define USE_C2C_FFT

class DabRadio;
class PlotWidget;
class QCategoryAxis;

enum class EAudioPlotMode
{
  FFT_SPECTRUM_UNFILLED,
  FFT_SPECTRUM_FILLED,
  LOG_BANDS,
  OFF
};

class AudioDisplay : public QObject
{
Q_OBJECT
public:
  AudioDisplay(DabRadio *, PlotWidget *, QSettings *);
  ~AudioDisplay() override;

  void plot_spectrum(const i16 *, i32, i32);
  void set_plot_mode(EAudioPlotMode iMode);
  [[nodiscard]] EAudioPlotMode get_plot_mode() const { return mPlotMode; }
  [[nodiscard]] static QStringList get_plot_mode_names();

private:
  struct SBandLimit
  {
    i32 startBin;
    i32 endBin;
  };

  static constexpr char SETTING_GROUP_NAME[] = "audioDisplay";
  static constexpr i32 cSpectrumSize = 512;
  static constexpr i32 cNormalizer = 32378;
  static constexpr i32 cDisplaySize = cSpectrumSize / 2;  // we use only the right half of the FFT

  static constexpr std::array<f32, 7> cBorderFrequencies = {
    62.5f, 125.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f
  };

  DabRadio * const mpRadioInterface;
  QSettings * const mpDabSettings;
  PlotWidget * const mpPlot;
  QLineSeries * mpLineSeries = nullptr;
  QLineSeries * mpUpperCurve = nullptr;
  QLineSeries * mpLowerCurve = nullptr;
  QAreaSeries * mpAreaSeries = nullptr;
  QCategoryAxis * mpCategoryAxis = nullptr;

  EAudioPlotMode mPlotMode = EAudioPlotMode::FFT_SPECTRUM_UNFILLED;
  bool mPlotModeChanged = false;

  std::array<f32, cDisplaySize>  mXDispBuffer{};
  std::array<f32, cDisplaySize>  mYDispBuffer{};
  std::array<f32, 8>             mYBand8Buffer{};
  std::array<SBandLimit, 8>      mBandLimits8{};
  std::array<f32, cSpectrumSize> mWindow;

  static_assert(sizeof(fftwf_complex) == sizeof(cf32));

#ifdef USE_C2C_FFT
  alignas(64) std::array<cf32, cSpectrumSize> mFftInBuffer;
  alignas(64) std::array<cf32, cSpectrumSize> mFftOutBuffer;
  fftwf_plan mFftPlan{fftwf_plan_dft_1d(cSpectrumSize, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_FORWARD, FFTW_ESTIMATE)};
#else
  alignas(64) std::array<f32,  cSpectrumSize> mFftInBuffer;
  alignas(64) std::array<cf32, cSpectrumSize/2 + 1> mFftOutBuffer;
  fftwf_plan mFftPlan{fftwf_plan_dft_r2c_1d(cSpectrumSize, mFftInBuffer.data(), (fftwf_complex *)mFftOutBuffer.data(), FFTW_ESTIMATE)};
#endif

  i32 mSampleRateLast = 0;

  void _setup_x_axis(i32 iSampleRate);
  void _calculate_band_limits(i32 iSampleRate);
  [[nodiscard]] static QString _get_plot_mode_tool_tip(EAudioPlotMode iMode);
};
