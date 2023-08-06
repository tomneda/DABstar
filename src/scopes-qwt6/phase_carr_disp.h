//
// Created by Thomas Neder
//
#ifndef PHASE_CARR_DISP_H
#define PHASE_CARR_DISP_H

#include <QColor>
#include <qcolor.h>
#include <qwt.h>
#include <qwt_plot.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_curve.h>
#include <vector>

class PhaseVsCarrDisp
{
public:
  PhaseVsCarrDisp(QwtPlot * plot);
  ~PhaseVsCarrDisp() = default;

  void disp_phase_carr_plot(const std::vector<float> && iPhaseVec);

private:
  QwtPlot * const mQwtPlot = nullptr;
  QwtPlotGrid	mQwtGrid;
  QwtPlotCurve mQwtPlotCurve;
  QColor mGridColor;
  QColor mCurveColor;
  int32_t mDataSize = 0;
  std::vector<float> mX_axis_vec;

  void _setup_x_axis();
};

#endif // PHASE_CARR_DISP_H
