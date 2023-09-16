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
    bool UseDots = true; // or connection line

    double StartValue = -180.0;
    double StopValue = 180.0;
    int32_t Segments = 8; // each 45.0

    double MarkerStartValue = -90.0;
    double MarkerStopValue = 90.0;
    int32_t MarkerSegments = 3; // each 90.0
  };

  void disp_carrier_plot(const std::vector<float> & iPhaseVec);
  void customize_plot(const SCustPlot & iCustPlot);

private:
  QwtPlot * const mQwtPlot = nullptr;
  QwtPlotCurve mQwtPlotCurve;
  std::vector<QwtPlotMarker *> mQwtPlotMarkerVec;
  int32_t mDataSize = 0;
  std::vector<float> mX_axis_vec;

  void _setup_x_axis();
};

#endif // PHASE_CARR_DISP_H
