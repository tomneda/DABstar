//
// Created by Thomas Neder
//
#ifndef PHASE_CARR_DISP_H
#define PHASE_CARR_DISP_H

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

    ECarrierPlotType PlotType = ECarrierPlotType::PHASE;
    EStyle Style = EStyle::DOTS;
    const char * Name = "(dummy)";

    double StartValue = -180.0;
    double StopValue = 180.0;
    int32_t Segments = 8;

    double MarkerStartValue = -90.0;
    double MarkerStepValue = 10.0;
    int32_t MarkerLinesNo = 3;

    QString ToolTip;
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
