//
// Created by Thomas Neder
//
#ifndef PHASE_CARR_DISP_H
#define PHASE_CARR_DISP_H

#include <QObject>
#include <qcolor.h>
#include <qwt.h>
#include <qwt_plot.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_curve.h>
#include <vector>

class PhaseVsCarrDisp : public QwtPlot
{
  Q_OBJECT
public:
  PhaseVsCarrDisp(QwtPlot * plot);
  ~PhaseVsCarrDisp() override = default;

  void disp_phase_carr_plot(const std::vector<float> && iPhaseVec);

private:
  QwtPlot * const mQwtPlot = nullptr;
  QwtPlotGrid	mQwtGrid;
  QwtPlotCurve mQwtPlotCurve;
  QColor mDisplayColor;
  QColor mGridColor;
  QColor mCurveColor;
};

#endif // PHASE_CARR_DISP_H
