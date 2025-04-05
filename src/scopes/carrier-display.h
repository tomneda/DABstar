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

#ifndef PHASE_CARR_DISP_H
#define PHASE_CARR_DISP_H

#include "glob_enums.h"
#include <qwt.h>
#include <qwt_plot.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <vector>
#include "cust_qwt_zoom_pan.h"


class CarrierDisp : public QObject
{
Q_OBJECT
public:
  explicit CarrierDisp(QwtPlot * plot);
  ~CarrierDisp() override = default;

  struct SCustPlot
  {
    enum class EStyle { DOTS, LINES, STICKS };

    ECarrierPlotType PlotType;
    EStyle Style;
    const char * Name;
    QString ToolTip;

    double YTopValue;
    double YBottomValue;
    double YTopValueRangeExt = 0;    // this zoom range extension value must be zero or positive
    double YBottomValueRangeExt = 0; // this zoom range extension value must be zero or negative
    int32_t YValueElementNo = 0;
    int32_t MarkerYValueStep = 0; // if not each Y value a marker should set (0 = to set no marker)
    bool DrawXGrid = true;
    bool DrawYGrid = true;
    bool DrawTiiSegments = false;
  };

  void display_carrier_plot(const std::vector<float> & iYValVec);
  void select_plot_type(const ECarrierPlotType iPlotType);
  static QStringList get_plot_type_names();

private:
  QwtPlot * const mpQwtPlot = nullptr;
  CustQwtZoomPan mZoomPan;
  QwtPlotCurve mQwtPlotCurve;
  QwtPlotGrid mQwtGrid;
  std::vector<QwtPlotMarker *> mQwtPlotMarkerVec;
  std::vector<QwtPlotMarker *> mQwtPlotTiiMarkerVec;
  std::vector<float> mX_axis_vec;
  int32_t mDataSize = 0;
  ECarrierPlotType mPlotType = ECarrierPlotType::DEFAULT;
  bool mPlotTypeChanged = false;

  void _customize_plot(const SCustPlot & iCustPlot);
  static SCustPlot _get_plot_type_data(const ECarrierPlotType iPlotType);
  void _setup_x_axis();
};

#endif // PHASE_CARR_DISP_H
