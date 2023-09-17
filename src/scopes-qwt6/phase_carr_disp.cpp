//
// Created by Thomas Neder
//

#include "glob_defs.h"
#include "phase_carr_disp.h"
#include <qwt_scale_div.h>
#include <QPen>
#include <QList>

CarrierDisp::CarrierDisp(QwtPlot * ipPlot) :
  mQwtPlot(ipPlot)
{
  mQwtPlotCurve.setPen(QPen(Qt::yellow, 2.0));
  mQwtPlotCurve.setOrientation(Qt::Horizontal);
  mQwtPlotCurve.attach(mQwtPlot);

  select_plot_type(EPlotType::PHASE);

  mQwtPlot->replot();
}

void CarrierDisp::disp_carrier_plot(const std::vector<float> & iPhaseVec)
{
  if (mDataSize != (int32_t)iPhaseVec.size())
  {
    mDataSize = (int32_t)iPhaseVec.size();
    _setup_x_axis();
  }

  mQwtPlotCurve.setSamples(mX_axis_vec.data(), iPhaseVec.data(), mDataSize);
  mQwtPlot->replot();
}

void CarrierDisp::select_plot_type(const CarrierDisp::EPlotType iPlotType)
{
  customize_plot(_get_plot_type_data(iPlotType));
}

void CarrierDisp::customize_plot(const SCustPlot & iCustPlot)
{
  // draw vertical ticks
  {
    assert(iCustPlot.Segments > 0);
    const double diffVal = (iCustPlot.StopValue - iCustPlot.StartValue) / iCustPlot.Segments;

    QList<double> tickList;

    for (int32_t segment = 0; segment <= iCustPlot.Segments; ++segment)
    {
      const double tickVal = iCustPlot.StartValue + segment * diffVal;
      tickList.push_back(tickVal);
    }

    mQwtPlot->setAxisScaleDiv(QwtPlot::yLeft, QwtScaleDiv(tickList[0], tickList[tickList.size() - 1], QList<double>(), QList<double>(), tickList));
  }

  // draw horizontal marker lines at -90, 0 , 90 degrees
  {
    // delete old markers
    for (auto & p : mQwtPlotMarkerVec)
    {
      p->detach();
      delete p;
      p = nullptr;
    }

    if (iCustPlot.MarkerSegments >= 2)
    {
      const double diffVal = (iCustPlot.MarkerStopValue - iCustPlot.MarkerStartValue) / (iCustPlot.MarkerSegments - 1);

      mQwtPlotMarkerVec.resize(iCustPlot.MarkerSegments);

      for (int32_t segment = 0; segment < iCustPlot.MarkerSegments; ++segment)
      {
        mQwtPlotMarkerVec[segment] = new QwtPlotMarker();
        QwtPlotMarker * const p = mQwtPlotMarkerVec[segment];
        p->setLinePen(QPen(Qt::gray));
        p->setLineStyle(QwtPlotMarker::HLine);
        p->setValue(0, iCustPlot.MarkerStartValue + diffVal * segment);
        p->attach(mQwtPlot);
      }
    }
  }

  mQwtPlotCurve.setStyle(iCustPlot.Style == SCustPlot::EStyle::DOTS ? QwtPlotCurve::Dots :  QwtPlotCurve::Lines);
}

void CarrierDisp::_setup_x_axis()
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

CarrierDisp::SCustPlot CarrierDisp::_get_plot_type_data(const EPlotType iPlotType)
{
  SCustPlot cp;
  cp.PlotType = iPlotType;

  switch (iPlotType)
  {
  case EPlotType::PHASE:
    cp.Style  = SCustPlot::EStyle::DOTS;
    cp.Name = "Phase";
    cp.StartValue = -180.0;
    cp.StopValue = 180.0;
    cp.Segments = 8; // each 45.0
    cp.MarkerStartValue = -90.0;
    cp.MarkerStopValue = 90.0;
    cp.MarkerSegments = 3; // each 90.0
    break;

  case EPlotType::MODQUAL:
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "ModQual";
    cp.StartValue = 0.0;
    cp.StopValue = 100.0;
    cp.Segments = 5; // each 20.0
    cp.MarkerStartValue = 10.0;
    cp.MarkerStopValue = 90.0;
    cp.MarkerSegments = 8; // each 10.0
    break;

  case EPlotType::NULLTII:
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "NullTII";
    cp.StartValue = -100.0;
    cp.StopValue = 0.0;
    cp.Segments = 5; // each 20.0
    cp.MarkerStartValue = -90.0;
    cp.MarkerStopValue = -10.0;
    cp.MarkerSegments = 8; // each -10.0
    break;
  }

  return cp;
}

QStringList CarrierDisp::get_plot_type_names()
{
  QStringList sl;
  SCustPlot cp;
  sl << _get_plot_type_data(EPlotType::PHASE).Name;
  sl << _get_plot_type_data(EPlotType::MODQUAL).Name;
  sl << _get_plot_type_data(EPlotType::NULLTII).Name;
  return sl;
}
