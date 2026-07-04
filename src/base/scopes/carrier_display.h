/*
 * Copyright (c) 2023 by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#pragma once

#include "glob_enums.h"
#include "plot_widget.h"
#include <QLineSeries>
#include <QScatterSeries>
#include <vector>


class CarrierDisp : public QObject
{
Q_OBJECT
public:
  explicit CarrierDisp(PlotWidget * plot);
  ~CarrierDisp() override = default;

  struct SCustPlot
  {
    enum class EStyle { DOTS, LINES, STICKS };

    ECarrierPlotType PlotType;
    EStyle Style;
    const char * Name;
    QString ToolTip;

    f64 YTopValue;
    f64 YBottomValue;
    f64 YTopValueRangeExt = 0;
    f64 YBottomValueRangeExt = 0;
    i32 YValueElementNo = 0;
    i32 MarkerYValueStep = 0;
    bool DrawXGrid = true;
    bool DrawYGrid = true;
    bool DrawTiiSegments = false;
  };

  void display_carrier_plot(const std::vector<f32> & iYValVec);
  void select_plot_type(const ECarrierPlotType iPlotType);
  static QStringList get_plot_type_names();

private:
  PlotWidget * const mpPlot = nullptr;
  QLineSeries * mpLineSeries = nullptr;
  QScatterSeries * mpDotSeries = nullptr;

  std::vector<QLineSeries *> mpMarkerLines;    // horizontal Y-value markers
  std::vector<QLineSeries *> mpTiiLines;       // vertical TII segment lines

  std::vector<f32> mX_axis_vec;
  i32 mDataSize = 0;
  ECarrierPlotType mPlotType = ECarrierPlotType::DEFAULT;
  bool mPlotTypeChanged = false;
  SCustPlot::EStyle mCurrentStyle = SCustPlot::EStyle::LINES;

  void _customize_plot(const SCustPlot & iCustPlot);
  static SCustPlot _get_plot_type_data(const ECarrierPlotType iPlotType);
  void _setup_x_axis();
  void _clear_marker_lines();
  void _clear_tii_lines();
};
