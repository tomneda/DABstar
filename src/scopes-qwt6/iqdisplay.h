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
 *    This file is part of the Qt-DAB
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
#ifndef  IQDISPLAY_H
#define  IQDISPLAY_H

#include  "glob_enums.h"
#include  "dab-constants.h"
#include  <vector>
#include  <qwt.h>
#include  <qwt_slider.h>
#include  <qwt_plot.h>
#include  <qwt_plot_curve.h>
#include  <qwt_plot_marker.h>
#include  <qwt_plot_grid.h>
#include  <qwt_dial.h>
#include  <qwt_dial_needle.h>
#include  <qwt_plot_spectrogram.h>
#include  <qwt_color_map.h>
#include  <qwt_plot_spectrogram.h>
#include  <qwt_scale_widget.h>
#include  <qwt_scale_draw.h>
#include  <qwt_plot_zoomer.h>
#include  <qwt_plot_panner.h>
#include  <qwt_plot_layout.h>

class SpectrogramData;

class IQDisplay : public QObject, public QwtPlotSpectrogram
{
Q_OBJECT
public:
  IQDisplay(QwtPlot * plot);
  ~IQDisplay() override;

  struct SCustPlot
  {
    EIqPlotType PlotType;
    const char * Name;
    const char * ToolTip;
  };

  void display_iq(const std::vector<cmplx> & z, float iScaleValue, float iScaleCircle);
  void customize_plot(const SCustPlot & iCustPlot);
  void select_plot_type(const EIqPlotType iPlotType);
  static QStringList get_plot_type_names();

private:
  static constexpr int32_t RADIUS = 100;

  float mLastCircleSize{ 0 };
  QwtPlot * mPlotgrid{ nullptr };
  SpectrogramData * mIQData = nullptr;

  std::vector<std::complex<int32_t>> mPoints;
  std::vector<double> mPlotDataBackgroundBuffer;
  std::vector<double> mPlotDataDrawBuffer;

  void set_point(int, int, int);
  void clean_screen_from_old_data_points();
  void draw_cross();
  void draw_circle(float ref, int val);
  void repaint_circle(float size);

  static SCustPlot _get_plot_type_data(const EIqPlotType iPlotType);
};

#endif

