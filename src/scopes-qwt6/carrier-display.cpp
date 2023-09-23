//
// Created by Thomas Neder
//

#include "glob_defs.h"
#include "carrier-display.h"
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

  assert(iCustPlot.YValueElementNo >= 2);
  assert(iCustPlot.YTopValue > iCustPlot.YBottomValue);

  const double diffVal = (iCustPlot.YTopValue - iCustPlot.YBottomValue) / (iCustPlot.YValueElementNo - 1);

  // draw vertical ticks
  {
    QList<double> tickList;

    for (int32_t elemIdx = 0; elemIdx < iCustPlot.YValueElementNo; ++elemIdx)
    {
      tickList.push_back(iCustPlot.YBottomValue + elemIdx * diffVal);
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

    if (iCustPlot.MarkerYValueStep > 0)
    {
      const int32_t noMarkers = (iCustPlot.YValueElementNo + iCustPlot.MarkerYValueStep - 1) / iCustPlot.MarkerYValueStep;
      mQwtPlotMarkerVec.resize(noMarkers);

      for (int32_t markerIdx = 0; markerIdx < noMarkers; ++markerIdx)
      {
        mQwtPlotMarkerVec[markerIdx] = new QwtPlotMarker();
        QwtPlotMarker * const p = mQwtPlotMarkerVec[markerIdx];
        p->setLineStyle(QwtPlotMarker::HLine);
        if (iCustPlot.Style == SCustPlot::EStyle::LINES)
        {
          p->setLinePen(QColor(Qt::darkGray), 0.0, Qt::DashLine);
        }
        else
        {
          p->setLinePen(QColor(Qt::gray), 0.0, Qt::SolidLine);
        }
        p->setValue(0, iCustPlot.YBottomValue + diffVal * markerIdx * iCustPlot.MarkerYValueStep);
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
  case ECarrierPlotType::FOURQUADPHASE:
    cp.ToolTip = "Shows the 4 phase segments in degree for each OFDM carrier.";
    cp.Style  = SCustPlot::EStyle::DOTS;
    cp.Name = "4-quadr. Phase";
    cp.YBottomValue = -180.0;
    cp.YTopValue = 180.0;
    cp.YValueElementNo = 9;
    cp.MarkerYValueStep = 2;
    break;

  case ECarrierPlotType::MODQUAL:
    cp.ToolTip = "Shows the modulation quality in % for each OFDM carrier. This is used for soft bit weighting for the viterbi decoder.";
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "Modul. Quality";
    cp.YBottomValue = 0.0;
    cp.YTopValue = 100.0;
    cp.YValueElementNo = 6;
    cp.MarkerYValueStep = 1;
    break;

  case ECarrierPlotType::STDDEV:
    cp.ToolTip = "Shows the standard deviation of the absolute phase (mapped to first quadrant) in degrees of each OFDM carrier. This is similar to the modulation quality.";
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "Std-Deviation";
    cp.YBottomValue = 0.0;
    cp.YTopValue = 45.0;
    cp.YValueElementNo = 7;
    cp.MarkerYValueStep = 1;
    break;

  case ECarrierPlotType::RELPOWER:
    cp.ToolTip = "Shows the relative signal power to the overall medium signal power in dB of each OFDM carrier.";
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "Relative Power";
    cp.YBottomValue = -18.0;
    cp.YTopValue = 18.0;
    cp.YValueElementNo = 7;
    cp.MarkerYValueStep = 1;
    break;

  case ECarrierPlotType::MEANABSPHASE:
    cp.ToolTip = "Shows the mean of absolute phase in degree (mapped to first quadrant) of each OFDM carrier.";
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "Mean Abs. Phase";
    cp.YBottomValue = 30.0;
    cp.YTopValue = 60.0;
    cp.YValueElementNo = 7;
    cp.MarkerYValueStep = 1;
    break;

//  case ECarrierPlotType::MEANPHASE:
//    cp.ToolTip = "Shows the mean phase in degree of each OFDM carrier.";
//    cp.Style  = SCustPlot::EStyle::LINES;
//    cp.Name = "Mean Phase";
//    cp.YBottomValue = -30.0;
//    cp.YTopValue = 30.0;
//    cp.YValueElementNo = 7;
//    cp.MarkerYValueStep = 1;
//    break;

  case ECarrierPlotType::NULLTII:
    cp.ToolTip = "Shows the averaged null symbol with TII carriers.";
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "Null Symbol TII";
    cp.YBottomValue = 0.0;
    cp.YTopValue = 100.0;
    cp.YValueElementNo = 6;
    cp.MarkerYValueStep = 1;
    break;

  case ECarrierPlotType::SNR:
    cp.ToolTip = "Shows the Signal-Noise-Ratio (SNR) in dB of each OFDM carrier.";
    cp.Style  = SCustPlot::EStyle::LINES;
    cp.Name = "S/N-Ratio";
    cp.YBottomValue = 0.0;
    cp.YTopValue = 60.0;
    cp.YValueElementNo = 7;
    cp.MarkerYValueStep = 1;
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
  sl << _get_plot_type_data(ECarrierPlotType::MEANABSPHASE).Name;
  //sl << _get_plot_type_data(ECarrierPlotType::MEANPHASE).Name;
  sl << _get_plot_type_data(ECarrierPlotType::FOURQUADPHASE).Name;
  sl << _get_plot_type_data(ECarrierPlotType::RELPOWER).Name;
  sl << _get_plot_type_data(ECarrierPlotType::SNR).Name;
  sl << _get_plot_type_data(ECarrierPlotType::NULLTII).Name;

  return sl;
}
