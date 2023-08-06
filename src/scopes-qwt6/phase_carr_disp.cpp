//
// Created by work on 05/08/23.
//

#include "glob_defs.h"
#include "phase_carr_disp.h"
#include <QPen>

PhaseVsCarrDisp::PhaseVsCarrDisp(QwtPlot * ipPlot) :
  mQwtPlot(ipPlot)
{
  //mDisplayColor = QColor(Qt::blue);
  mGridColor = QColor(Qt::lightGray);
  mCurveColor = QColor(Qt::yellow);

  //mQwtPlot->setCanvasBackground(mDisplayColor);
  mQwtPlot->enableAxis(QwtPlot::xBottom);
  mQwtPlot->setAxisScale(QwtPlot::yLeft, -180.0, 180.0);

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
  if (mDataSize != (int32_t)iPhaseVec.size())
  {
    mDataSize = (int32_t)iPhaseVec.size();
    _setup_x_axis();
  }

  mQwtPlotCurve.setSamples(mX_axis_vec.data(), iPhaseVec.data(), mDataSize);
  mQwtPlot->replot();
}

void PhaseVsCarrDisp::_setup_x_axis()
{
  const int32_t displaySizeHalf = mDataSize / 2;
  mX_axis_vec.resize(mDataSize);

  // the vector iPhaseVec does not contain data for the zero point, so skip the zero also in the x-vector
  for (int32_t i = 0; i < displaySizeHalf; i++)
  {
    mX_axis_vec.at(i) = (float)(i - displaySizeHalf);
    mX_axis_vec.at(i + displaySizeHalf) = (float)(i + 1);
  }

  mQwtPlot->setAxisScale(xBottom, mX_axis_vec[0], mX_axis_vec[mX_axis_vec.size() - 1]);
}

