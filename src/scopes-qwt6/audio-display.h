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
#include  <QSettings>
#include  <QFrame>
#include  <QObject>
#include  <qwt.h>
#include  <qwt_plot.h>
#include  <qwt_plot_marker.h>
#include  <qwt_plot_grid.h>
#include  <qwt_plot_curve.h>
#include  <qwt_plot_marker.h>
#include  <qwt_color_map.h>
#include  <qwt_plot_zoomer.h>
#include  <qwt_plot_textlabel.h>
#include  <qwt_plot_panner.h>
#include  <qwt_plot_layout.h>
#include  <qwt_picker_machine.h>
#include  <qwt_scale_widget.h>
#include  <QBrush>
#include  "fft-handler.h"

class RadioInterface;

class AudioDisplay : public QObject
{
Q_OBJECT
public:
  AudioDisplay(RadioInterface *, QwtPlot *, QSettings *);
  ~AudioDisplay() override;

  void create_spectrum(int16_t *, int, int);

private:
  static constexpr char SETTING_GROUP_NAME[] = "audioDisplay";
  static constexpr int32_t spectrumSize = 2048;
  static constexpr int32_t normalizer = 16 * 2048;

  RadioInterface * myRadioInterface;
  QSettings * dabSettings;
  QwtPlot * plotGrid;
  QwtPlotCurve spectrumCurve{""};
  QwtPlotGrid grid;
  int32_t displaySize = 512;

  std::array<double, 512> displayBuffer;
  std::array<float, 4 * 512> Window;
  cmplx * spectrumBuffer;
  fftHandler fft{ 2048, false };
  QwtPlotPicker * lm_picker;
  QColor gridColor;
  QColor curveColor;
  bool brush;

  void _view_spectrum(double *, double *, double, int);
  float _get_db(float);

private slots:
  void _slot_rightMouseClick(const QPointF &);
};

#endif

