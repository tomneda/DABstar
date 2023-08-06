//
// Created by work on 05/08/23.
//

#include "glob_defs.h"
#include "phase_carr_disp.h"
#include <QPen>

PhaseVsCarrDisp::PhaseVsCarrDisp(QwtPlot * ipPlot) :
  mQwtPlot(ipPlot)
{
  mGridColor = QColor(Qt::lightGray);
  mDisplayColor = QColor(Qt::blue);
  mCurveColor = QColor(Qt::yellow);

  mQwtPlot->setCanvasBackground(mDisplayColor);


  mQwtGrid.setMajorPen(QPen(mGridColor, 0, Qt::DotLine));
  mQwtGrid.setMinorPen(QPen(mGridColor, 0, Qt::DotLine));
  mQwtGrid.enableXMin(true);
  mQwtGrid.enableYMin(true);
  mQwtGrid.attach(mQwtPlot);

  mQwtPlotCurve.setPen(QPen(mCurveColor, 2.0));
  mQwtPlotCurve.setOrientation(Qt::Horizontal);
  mQwtPlotCurve.setRenderHint( QwtPlotItem::RenderAntialiased, false );
  mQwtPlotCurve.setStyle(QwtPlotCurve::Dots);
  mQwtPlotCurve.attach(mQwtPlot);

  mQwtPlot->replot();
}

void PhaseVsCarrDisp::disp_phase_carr_plot(const std::vector<float> && iPhaseVec)
{
  const auto displaySize = (int32_t)iPhaseVec.size();
  std::vector<float> x_axis_vec(displaySize);
  std::vector<float> y_axis_vec(displaySize);

  // TODO: how to handle zero?
  for (int32_t i = -displaySize / 2; i < displaySize / 2; i++)
  {
    x_axis_vec.at(i + displaySize / 2) = (float)i;  // carrier number
  }
  
  for (int32_t i = 0; i < displaySize; i++)
  {
    y_axis_vec.at(i) = iPhaseVec.at(i);
  }

  //mQwtPlotCurve.setBaseline(0);
  //mQwtPlotCurve.setPaintAttribute(QwtPlotCurve::ImageBuffer);

  mQwtPlot->setAxisScale(QwtPlot::xBottom, x_axis_vec[0], x_axis_vec[x_axis_vec.size() - 1]);
  mQwtPlot->enableAxis(QwtPlot::xBottom);
  mQwtPlot->setAxisScale(QwtPlot::yLeft, -180.0, 180.0);

  mQwtPlotCurve.setSamples(x_axis_vec.data(), iPhaseVec.data(), displaySize);
  mQwtPlot->replot();
}

