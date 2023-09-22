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

  select_plot_type(ECarrierPlotType::MODQUAL);

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

void CarrierDisp::select_plot_type(const ECarrierPlotType iPlotType)
{
  customize_plot(_get_plot_type_data(iPlotType));
}

void CarrierDisp::customize_plot(const SCustPlot & iCustPlot)
{
  mQwtPlot->setToolTip(iCustPlot.ToolTip);

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

    if (iCustPlot.MarkerLinesNo > 0)
    {
      mQwtPlotMarkerVec.resize(iCustPlot.MarkerLinesNo);

      for (int32_t segment = 0; segment < iCustPlot.MarkerLinesNo; ++segment)
      {
        mQwtPlotMarkerVec[segment] = new QwtPlotMarker();
        QwtPlotMarker * const p = mQwtPlotMarkerVec[segment];
        p->setLinePen(QPen(Qt::gray));
        p->setLineStyle(QwtPlotMarker::HLine);
        if (iCustPlot.Style == SCustPlot::EStyle::LINES)
        {
          p->setLinePen(QColor(Qt::darkGray), 0.0, Qt::DashLine);
        }
        p->setValue(0, iCustPlot.MarkerStartValue + iCustPlot.MarkerStepValue * segment);
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

CarrierDisp::SCustPlot CarrierDisp::_get_plot_type_data(const ECarrierPlotType iPlotType)
{
  SCustPlot cp;
  cp.PlotType = iPlotType;

  switch (iPlotType)
  {
  case ECarrierPlotType::PHASE:
    cp.ToolTip = "Shows the 4 phase segments in degree for each OFDM carrier.";
    cp.Style  = SCustPlot::EStyle::DOTS;
    cp.Name = "4-quadr. Phase";
    cp.StartValue = -180.0;
    cp.StopValue = 180.0;
    cp.Segments = 8; // each 45.0
    cp.MarkerStartValue = -90.0;
    cp.MarkerStepValue = 90.0;
    cp.MarkerLinesNo = 3;
    break;

  case ECarrierPlotType::MODQUAL:
    cp.ToolTip = "Shows the modulation quality in % for each OFDM carrier. This is used for soft bit weighting for the viterbi decoder.";
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "Modul. Quality";
    cp.StartValue = 0.0;
    cp.StopValue = 100.0;
    cp.Segments = 5; // each 20.0
    cp.MarkerStartValue = 10.0;
    cp.MarkerStepValue = 10.0;
    cp.MarkerLinesNo = 9;
    break;

  case ECarrierPlotType::STDDEV:
    cp.ToolTip = "Shows the standard deviation of the absolute phase (mapped to first quadrant) in degrees of each OFDM carrier. This is similar to the modulation quality.";
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "Std-Deviation";
    cp.StartValue = 0.0;
    cp.StopValue = 45.0;
    cp.Segments = 6; 
    cp.MarkerStartValue = 7.5;
    cp.MarkerStepValue = 7.5;
    cp.MarkerLinesNo = 5;
    break;

  case ECarrierPlotType::RELLEVEL:
    cp.ToolTip = "Shows the relative level to the overall medium level in dB of each OFDM carrier.";
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "Relative Level";
    cp.StartValue = -12.0;
    cp.StopValue = 12.0;
    cp.Segments = 8; // each 3.0
    cp.MarkerStartValue = -9.0;
    cp.MarkerStepValue =  3.0;
    cp.MarkerLinesNo = 7;
    break;

  case ECarrierPlotType::MEANABSPHASE:
    cp.ToolTip = "Shows the mean of absolute phase in degree (mapped to first quadrant) of each OFDM carrier.";
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "Mean Abs. Phase";
    cp.StartValue = 30.0;
    cp.StopValue = 60.0;
    cp.Segments = 4;
    cp.MarkerStartValue = 35.0;
    cp.MarkerStepValue =  5.0;
    cp.MarkerLinesNo = 5;
    break;

  case ECarrierPlotType::MEANPHASE:
    cp.ToolTip = "Shows the mean phase in degree of each OFDM carrier.";
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "Mean Phase";
    cp.StartValue = 30.0;
    cp.StopValue = -30.0;
    cp.Segments = 7;
    cp.MarkerStartValue = -30.0;
    cp.MarkerStepValue =  10.0;
    cp.MarkerLinesNo = 7;
    break;

  case ECarrierPlotType::NULLTII:
    cp.ToolTip = "Shows the averaged null symbol with TII carriers.";
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "Null Symbol TII";
    cp.StartValue = 0.0;
    cp.StopValue = 100.0;
    cp.Segments = 5; // each 20.0
    cp.MarkerStartValue = 10.0;
    cp.MarkerStepValue = 10.0;
    cp.MarkerLinesNo = 9; 
    break;
  }

  return cp;
}

QStringList CarrierDisp::get_plot_type_names()
{
  QStringList sl;
  SCustPlot cp;
  // ATTENTION: use same sequence as in ECarrierPlotType
  sl << _get_plot_type_data(ECarrierPlotType::MODQUAL).Name;
  sl << _get_plot_type_data(ECarrierPlotType::STDDEV).Name;
  sl << _get_plot_type_data(ECarrierPlotType::RELLEVEL).Name;
  sl << _get_plot_type_data(ECarrierPlotType::MEANABSPHASE).Name;
  sl << _get_plot_type_data(ECarrierPlotType::MEANPHASE).Name;
  sl << _get_plot_type_data(ECarrierPlotType::PHASE).Name;
  sl << _get_plot_type_data(ECarrierPlotType::NULLTII).Name;
  return sl;
}
