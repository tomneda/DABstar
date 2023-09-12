//
// Created by Thomas Neder
//

#include "glob_defs.h"
#include "phase_carr_disp.h"
#include <qwt_scale_div.h>
#include <QPen>
#include <QList>

PhaseVsCarrDisp::PhaseVsCarrDisp(QwtPlot * ipPlot) :
  mQwtPlot(ipPlot)
{
  // draw vertical ticks
  QList<double> tickList = { -180.0, -135.0, -90.0, -45.0, 0.0, 45.0, 90.0, 135.0, 180.0 };
  mQwtPlot->setAxisScaleDiv(QwtPlot::yLeft, QwtScaleDiv(-180.0, 180.0, QList<double>(), QList<double>(), tickList) );

  // draw horizontal marker lines at -90, 0 , 90 degrees
  for (uint32_t idx = 0; idx < mQwtPlotMarkerVec.size(); ++idx)
  {
    QwtPlotMarker & m = mQwtPlotMarkerVec[idx];
    m.setLinePen(QPen(Qt::gray));
    m.setLineStyle(QwtPlotMarker::HLine);
    m.setValue(0, -90.0 + 90.0 * idx);
    m.attach(mQwtPlot);
  }

  mQwtPlotCurve.setPen(QPen(Qt::yellow, 2.0));
  mQwtPlotCurve.setOrientation(Qt::Horizontal);
  //mQwtPlotCurve.setStyle(QwtPlotCurve::Dots);
  mQwtPlotCurve.attach(mQwtPlot);

  mQwtPlot->replot();
}

void PhaseVsCarrDisp::disp_phase_carr_plot(const std::vector<float> & iPhaseVec)
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
    mX_axis_vec[i] = (float)(i - displaySizeHalf);
    mX_axis_vec[i + displaySizeHalf] = (float)(i + 1);
  }

  mQwtPlot->setAxisScale(QwtPlot::xBottom, mX_axis_vec[0], mX_axis_vec[mX_axis_vec.size() - 1]);
}

