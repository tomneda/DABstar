//
// Created by Thomas Neder
//
#ifndef PHASE_CARR_DISP_H
#define PHASE_CARR_DISP_H

#include "glob_enums.h"
#include <qcolor.h>
#include <qwt.h>
#include <qwt_plot.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_curve.h>
#include <vector>

class CarrierDisp
{
public:
  explicit CarrierDisp(QwtPlot * plot);
  ~CarrierDisp() = default;

  struct SCustPlot
  {
    enum class EStyle { DOTS, LINES };

    ECarrierPlotType PlotType;
    EStyle Style;
    const char * Name;
    const char * ToolTip;

    double YBottomValue;
    double YTopValue;
    int32_t YValueElementNo;

    int32_t MarkerYValueStep = 1; // if not each Y value a marker should set (0 = no set no marker)
  };

  void disp_carrier_plot(const std::vector<float> & iPhaseVec);
  void customize_plot(const SCustPlot & iCustPlot);
  void select_plot_type(const ECarrierPlotType iPlotType);
  static QStringList get_plot_type_names() ;

private:
  QwtPlot * const mQwtPlot = nullptr;
  QwtPlotCurve mQwtPlotCurve;
  std::vector<QwtPlotMarker *> mQwtPlotMarkerVec;
  int32_t mDataSize = 0;
  std::vector<float> mX_axis_vec;

  static SCustPlot _get_plot_type_data(const ECarrierPlotType iPlotType);
  void _setup_x_axis();
};

#endif // PHASE_CARR_DISP_H
