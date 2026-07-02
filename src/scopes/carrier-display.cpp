/*
 * Copyright (c) 2023 by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "glob_defs.h"
#include "carrier-display.h"
#include <QPen>
#include <QChart>

CarrierDisp::CarrierDisp(PlotWidget * ipPlot)
  : mpPlot(ipPlot)
{
  // Line series for LINES and STICKS styles
  mpLineSeries = new QLineSeries();
  mpLineSeries->setPen(QPen(Qt::yellow, 2.0));
  mpLineSeries->setUseOpenGL(false);
  mpPlot->chart()->addSeries(mpLineSeries);
  mpLineSeries->attachAxis(mpPlot->get_x_axis());
  mpLineSeries->attachAxis(mpPlot->get_y_axis());

  // Scatter series for DOTS style
  mpDotSeries = new QScatterSeries();
  mpDotSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);
  mpDotSeries->setMarkerSize(3.0);
  mpDotSeries->setColor(Qt::yellow);
  mpDotSeries->setBorderColor(Qt::transparent);
  mpDotSeries->setUseOpenGL(false);
  mpPlot->chart()->addSeries(mpDotSeries);
  mpDotSeries->attachAxis(mpPlot->get_x_axis());
  mpDotSeries->attachAxis(mpPlot->get_y_axis());

  mpPlot->get_x_axis()->setGridLineVisible(true);
  mpPlot->get_x_axis()->setMinorGridLineVisible(true);
  mpPlot->get_y_axis()->setGridLineVisible(true);
  mpPlot->get_y_axis()->setMinorGridLineVisible(false);

  mpPlot->setup_x_zoom(PlotWidget::SRange(-1536.0 / 2, 1536.0 / 2));

  select_plot_type(ECarrierPlotType::DEFAULT);
}

void CarrierDisp::display_carrier_plot(const std::vector<f32> & iYValVec)
{
  if (mPlotTypeChanged)
  {
    mPlotTypeChanged = false;
    _customize_plot(_get_plot_type_data(mPlotType));
  }

  if (mDataSize != (i32)iYValVec.size())
  {
    mDataSize = (i32)iYValVec.size();
    _setup_x_axis();
  }

  QList<QPointF> pts;
  pts.reserve(mDataSize);
  for (i32 i = 0; i < mDataSize; i++)
  {
    pts.append(QPointF(mX_axis_vec[i], iYValVec[i]));
  }

  if (mCurrentStyle == SCustPlot::EStyle::DOTS)
  {
    mpDotSeries->replace(pts);
    mpLineSeries->clear();
  }
  else if (mCurrentStyle == SCustPlot::EStyle::STICKS)
  {
    QList<QPointF> stickPts;
    stickPts.reserve(mDataSize * 3);
    for (i32 i = 0; i < mDataSize; i++)
    {
      stickPts.append(QPointF(mX_axis_vec[i], 0.0));
      stickPts.append(QPointF(mX_axis_vec[i], iYValVec[i]));
      stickPts.append(QPointF(mX_axis_vec[i], qQNaN())); // "lift" the pen
    }
    mpLineSeries->replace(stickPts);
    mpDotSeries->clear();
  }
  else
  {
    mpLineSeries->replace(pts);
    mpDotSeries->clear();
  }
}

void CarrierDisp::select_plot_type(const ECarrierPlotType iPlotType)
{
  mPlotType = iPlotType;
  mPlotTypeChanged = true;
}

void CarrierDisp::_clear_marker_lines()
{
  for (auto * p : mpMarkerLines)
  {
    mpPlot->chart()->removeSeries(p);
    delete p;
  }
  mpMarkerLines.clear();
}

void CarrierDisp::_clear_tii_lines()
{
  for (auto * p : mpTiiLines)
  {
    mpPlot->chart()->removeSeries(p);
    delete p;
  }
  mpTiiLines.clear();
}

void CarrierDisp::_customize_plot(const SCustPlot & iCustPlot)
{
  mpPlot->setToolTip(iCustPlot.ToolTip);
  mCurrentStyle = iCustPlot.Style;

  assert(iCustPlot.YTopValue > iCustPlot.YBottomValue);

  mpPlot->setup_y_zoom(PlotWidget::SRange(iCustPlot.YBottomValue, iCustPlot.YTopValue,
                                          iCustPlot.YBottomValueRangeExt, iCustPlot.YTopValueRangeExt));
  mpPlot->reset_x_zoom();
  mpPlot->reset_y_zoom();

  _clear_marker_lines();
  _clear_tii_lines();

  mpPlot->get_x_axis()->setGridLineVisible(iCustPlot.DrawXGrid);
  mpPlot->get_y_axis()->setGridLineVisible(iCustPlot.DrawYGrid);

  // Horizontal marker lines at specific Y values
  if (iCustPlot.MarkerYValueStep > 0)
  {
    assert(iCustPlot.YValueElementNo >= 2);
    const f64 diffVal = (iCustPlot.YTopValue - iCustPlot.YBottomValue) / (iCustPlot.YValueElementNo - 1);
    const i32 noMarkers = (iCustPlot.YValueElementNo + iCustPlot.MarkerYValueStep - 1) / iCustPlot.MarkerYValueStep;

    for (i32 markerIdx = 0; markerIdx < noMarkers; ++markerIdx)
    {
      const f64 yVal = iCustPlot.YBottomValue + diffVal * markerIdx * iCustPlot.MarkerYValueStep;
      auto * line = new QLineSeries();
      line->setPen(iCustPlot.Style == SCustPlot::EStyle::DOTS
                   ? QPen(Qt::gray,     0.0, Qt::SolidLine)
                   : QPen(Qt::darkGray, 0.0, Qt::DashLine));
      line->append(-1e9, yVal);
      line->append(+1e9, yVal);
      mpPlot->chart()->addSeries(line);
      line->attachAxis(mpPlot->get_x_axis());
      line->attachAxis(mpPlot->get_y_axis());
      mpMarkerLines.push_back(line);
    }
  }

  // Vertical TII segment boundary lines
  if (iCustPlot.DrawTiiSegments)
  {
    std::array<f64, 4 * 8 - 1> xPoints;
    i32 c = 0;
    for (i32 j = -15; j <= -1; ++j) xPoints[c++] = 48.0 * j - 0.5;
    xPoints[c++] = 0;
    for (i32 j =   1; j <= 15; ++j) xPoints[c++] = 48.0 * j + 0.5;
    assert(c == (i32)xPoints.size());

    const f64 yLineMin = iCustPlot.YBottomValue + iCustPlot.YBottomValueRangeExt - 1.0;
    const f64 yLineMax = iCustPlot.YTopValue    + iCustPlot.YTopValueRangeExt    + 1.0;

    for (i32 i = 0; i < (i32)xPoints.size(); ++i)
    {
      auto * vline = new QLineSeries();
      const QColor col = ((i - 15) % 8 == 0) ? QColor(0x5555BB) : QColor(0xAA4444);
      vline->setPen(QPen(col, 0.0, Qt::SolidLine));
      vline->append(xPoints[i], yLineMin);
      vline->append(xPoints[i], yLineMax);
      mpPlot->chart()->addSeries(vline);
      vline->attachAxis(mpPlot->get_x_axis());
      vline->attachAxis(mpPlot->get_y_axis());
      mpTiiLines.push_back(vline);
    }
  }
}

void CarrierDisp::_setup_x_axis()
{
  const i32 displaySizeHalf = mDataSize / 2;
  mX_axis_vec.resize(mDataSize);

  for (i32 i = 0; i < displaySizeHalf; i++)
  {
    mX_axis_vec[i] = (f32)(i - displaySizeHalf);
    mX_axis_vec[i + displaySizeHalf] = (f32)(i + 1);
  }

  mpPlot->set_x_range(mX_axis_vec[0], mX_axis_vec[mX_axis_vec.size() - 1]);
}

CarrierDisp::SCustPlot CarrierDisp::_get_plot_type_data(const ECarrierPlotType iPlotType)
{
  SCustPlot cp;
  cp.PlotType = iPlotType;

  switch (iPlotType)
  {
  case ECarrierPlotType::SB_WEIGHT:
    cp.ToolTip = "Shows the soft-bit weight in percentage for each OFDM carrier.<p>This is used for the viterbi decoder data input.</p>";
    cp.Style = SCustPlot::EStyle::LINES;
    cp.Name = "Soft-Bit Weight";
    cp.YTopValue = 100.0;
    cp.YBottomValue = 0.0;
    break;

  case ECarrierPlotType::EVM_PER:
    cp.ToolTip = "Shows the EVM (Error Vector Magnitude) in percentage of each OFDM carrier.";
    cp.Style = SCustPlot::EStyle::LINES;
    cp.Name = "EVM (%)";
    cp.YTopValue = 100.0;
    cp.YBottomValue = 0.0;
    break;

  case ECarrierPlotType::EVM_DB:
    cp.ToolTip = "Shows the EVM (Error Vector Magnitude) in dB of each OFDM carrier.";
    cp.Style = SCustPlot::EStyle::LINES;
    cp.Name = "EVM (dB)";
    cp.YTopValue = 0.0;
    cp.YBottomValue = -36.0;
    cp.YBottomValueRangeExt = -24.0;
    break;

  case ECarrierPlotType::STD_DEV:
    cp.ToolTip = "Shows the standard deviation of the absolute phase (mapped to first quadrant) in degrees of each OFDM carrier.";
    cp.Style = SCustPlot::EStyle::LINES;
    cp.Name = "Phase Std-Dev.";
    cp.YTopValue = 45.0;
    cp.YBottomValue = 0.0;
    break;

  case ECarrierPlotType::PHASE_ERROR:
    cp.ToolTip = "Shows the corrected phase error in degree of each OFDM carrier.";
    cp.Style = SCustPlot::EStyle::LINES;
    cp.Name = "Corr. Phase Error";
    cp.YTopValue = 60.0;
    cp.YBottomValue = -60.0;
    break;

  case ECarrierPlotType::PRS_PHASE:
    cp.ToolTip = "Shows the phase in degree of the decoded PRS symbols.";
    cp.Style = SCustPlot::EStyle::DOTS;
    cp.Name = "PRS Phase";
    cp.YTopValue = 180.0;
    cp.YBottomValue = -180.0;
    cp.YValueElementNo = 9;
    cp.MarkerYValueStep = 2;
    cp.DrawYGrid = false;
    break;

  case ECarrierPlotType::PRS_PHASE_UNWRAP:
    cp.ToolTip = "Shows the unwrapped phase in degree of the decoded PRS symbols.";
    cp.Style = SCustPlot::EStyle::LINES;
    cp.Name = "PRS Phase unwrapped";
    cp.YTopValue = 180.0*100;
    cp.YBottomValue = -180.0*100;
    cp.YTopValueRangeExt = 180.0*1000;
    cp.YBottomValueRangeExt = -180.0*1000;
    cp.YValueElementNo = 9;
    cp.MarkerYValueStep = 2;
    cp.DrawYGrid = false;
    break;

  case ECarrierPlotType::FOUR_QUAD_PHASE:
    cp.ToolTip = "Shows the 4 phase segments in degree for each OFDM carrier.";
    cp.Style = SCustPlot::EStyle::DOTS;
    cp.Name = "4-quadr. Phase";
    cp.YTopValue = 180.0;
    cp.YBottomValue = -180.0;
    cp.YValueElementNo = 9;
    cp.MarkerYValueStep = 2;
    cp.DrawYGrid = false;
    break;

  case ECarrierPlotType::REL_POWER:
    cp.ToolTip = "Shows the relative signal power to the overall medium signal power in dB of each OFDM carrier.";
    cp.Style = SCustPlot::EStyle::LINES;
    cp.Name = "Relative Power";
    cp.YTopValue = 12.0;
    cp.YTopValueRangeExt = 6.0;
    cp.YBottomValue = -24.0;
    cp.YBottomValueRangeExt = -26.0;
    break;

  case ECarrierPlotType::SNR:
    cp.ToolTip = "Shows the Signal-Noise-Ratio (SNR) in dB of each OFDM carrier.";
    cp.Style = SCustPlot::EStyle::LINES;
    cp.Name = "S/N-Ratio";
    cp.YTopValue = 60.0;
    cp.YBottomValue = 0.0;
    break;

  case ECarrierPlotType::NULL_TII_LIN:
    cp.ToolTip = "Shows the averaged null symbol level with TII carriers in percentage of the maximum peak.";
    cp.Style = SCustPlot::EStyle::STICKS;
    cp.Name = "Null Sym. TII (%)";
    cp.YTopValue = 100.0;
    cp.YBottomValue = 0.0;
    cp.DrawTiiSegments = true;
    cp.DrawXGrid = false;
    break;

  case ECarrierPlotType::NULL_TII_LOG:
    cp.ToolTip = "Shows the averaged null symbol level with TII carriers in dB above noise level.";
    cp.Style = SCustPlot::EStyle::STICKS;
    cp.Name = "Null Sym. TII (dB)";
    cp.YTopValue = 50.0;
    cp.YTopValueRangeExt = 40.0;
    cp.YBottomValue = 0.0;
    cp.DrawTiiSegments = true;
    cp.DrawXGrid = false;
    break;

  case ECarrierPlotType::NULL_NO_TII:
    cp.ToolTip = "Shows the averaged null symbol level without TII carriers in percentage of the maximum peak.";
    cp.Style = SCustPlot::EStyle::LINES;
    cp.Name = "Null Sym. no TII";
    cp.YTopValue = 100.0;
    cp.YBottomValue = 0.0;
    break;

  case ECarrierPlotType::NULL_OVR_POW:
    cp.ToolTip = "Shows the averaged null symbol (without TII) power relation to the averaged overall carrier power in dB."
                 "<p>This reveals disturbing (non-DAB) signals which can degrade decoding quality.</p>";
    cp.Style = SCustPlot::EStyle::LINES;
    cp.Name = "Null Sym. ovr. Pow.";
    cp.YTopValue = 6.0;
    cp.YBottomValue = -48.0;
    cp.YBottomValueRangeExt = -18.0;
    break;
  }

  cp.ToolTip += "<p>The carrier at index 0 is not really existing. The value is interpolated between the two neighbor carriers.</p>"
                "<p><b>Zooming</b><br>"
                "Press <b>&lt;CTRL&gt;</b> key for <b>vertical</b> zooming, panning and reset zoom.<br>"
                "Press <b>&lt;SHIFT&gt;</b> key for <b>horizontal</b> zooming, panning and reset zoom.<br>"
                "Use mouse wheel for zooming.<br>"
                "Press left mouse button for panning.<br>Press right mouse button to reset zoom.</p>";

  return cp;
}

QStringList CarrierDisp::get_plot_type_names()
{
  QStringList sl;

  // ATTENTION: use same sequence as in ECarrierPlotType
  sl << _get_plot_type_data(ECarrierPlotType::SB_WEIGHT).Name;
  sl << _get_plot_type_data(ECarrierPlotType::EVM_PER).Name;
  sl << _get_plot_type_data(ECarrierPlotType::EVM_DB).Name;
  sl << _get_plot_type_data(ECarrierPlotType::STD_DEV).Name;
  sl << _get_plot_type_data(ECarrierPlotType::PHASE_ERROR).Name;
  sl << _get_plot_type_data(ECarrierPlotType::PRS_PHASE).Name;
  sl << _get_plot_type_data(ECarrierPlotType::PRS_PHASE_UNWRAP).Name;
  sl << _get_plot_type_data(ECarrierPlotType::FOUR_QUAD_PHASE).Name;
  sl << _get_plot_type_data(ECarrierPlotType::REL_POWER).Name;
  sl << _get_plot_type_data(ECarrierPlotType::SNR).Name;
  sl << _get_plot_type_data(ECarrierPlotType::NULL_TII_LIN).Name;
  sl << _get_plot_type_data(ECarrierPlotType::NULL_TII_LOG).Name;
  sl << _get_plot_type_data(ECarrierPlotType::NULL_NO_TII).Name;
  sl << _get_plot_type_data(ECarrierPlotType::NULL_OVR_POW).Name;

  return sl;
}
