/*
 * Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
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

#include "plot_widget.h"
#include <QChart>
#include <QGraphicsLayout>
#include <QPainter>
#include <cmath>

PlotWidget::PlotWidget(QWidget * const parent)
  : QChartView(parent)
{
  QLinearGradient bgGradient(0, 0, 0, 1);
  bgGradient.setCoordinateMode(QGradient::ObjectBoundingMode);
  bgGradient.setColorAt(0.0, QColor(58, 58, 58));
  bgGradient.setColorAt(1.0, QColor(18, 18, 18));

  auto * const ch = new QChart();
  ch->setBackgroundBrush(bgGradient);
  ch->legend()->hide();
  ch->setMargins(QMargins(2, 2, 2, 2));
  ch->layout()->setContentsMargins(0, 0, 0, 0);

  mpXAxis = new QValueAxis();
  mpXAxis->setLabelFormat("%.0f");
  mpXAxis->setLabelsColor(Qt::lightGray);
  mpXAxis->setGridLineColor(QColor(0x5e5c64));
  mpXAxis->setMinorGridLinePen(Qt::NoPen);
  mpXAxis->setMinorGridLineVisible(false);
  mpXAxis->setTickCount(7);
  mpXAxis->setMinorTickCount(1);

  mpYAxis = new QValueAxis();
  mpYAxis->setLabelFormat("%.0f");
  mpYAxis->setLabelsColor(Qt::lightGray);
  mpYAxis->setGridLineColor(QColor(0x5e5c64));
  mpYAxis->setMinorGridLineColor(QColor(0x3a3840));

  ch->addAxis(mpXAxis, Qt::AlignBottom);
  ch->addAxis(mpYAxis, Qt::AlignLeft);

  setChart(ch);
  setRenderHint(QPainter::Antialiasing, false);
  setMouseTracking(true);

  connect(ch, &QChart::plotAreaChanged, this, [this](const QRectF & iPlotArea)
  {
    const i32 left = (i32)std::round(iPlotArea.left());
    const i32 right = (i32)std::round(width() - iPlotArea.right());
    emit signal_plot_area_changed(left, right);
  });
}

void PlotWidget::set_x_range(const f64 iMin, const f64 iMax) const
{
  mpXAxis->setRange(iMin, iMax);
  mpXAxis->setTickType(QValueAxis::TicksDynamic);
  mpXAxis->setTickAnchor(0.0);
  mXTickAuto = true;
  _reapply_auto_ticks();
}

void PlotWidget::set_y_range(const f64 iMin, const f64 iMax) const
{
  mpYAxis->setRange(iMin, iMax);
  mpYAxis->setTickType(QValueAxis::TicksDynamic);
  mpYAxis->setTickAnchor(0.0);
  mYTickAuto = true;
  _reapply_auto_ticks();
}

void PlotWidget::_update_y_label_format() const
{
  f64 interval;
  if (mpYAxis->tickType() == QValueAxis::TicksDynamic)
  {
    interval = mpYAxis->tickInterval();
  }
  else
  {
    const f64 span = mpYAxis->max() - mpYAxis->min();
    interval = span / std::max(mpYAxis->tickCount() - 1, 1);
  }
  if (interval <= 0.0) return;
  const i32 decimals = std::max(0, (i32)-std::floor(std::log10(interval)));
  mpYAxis->setLabelFormat(QString::asprintf("%%.%df", decimals));
}

void PlotWidget::set_x_tick_count(const f64 iAnchor, const i32 iCount) const
{
  mXTickAuto = false;
  mpXAxis->setTickType(QValueAxis::TicksFixed);
  mpXAxis->setTickCount(iCount);
  mpXAxis->setTickAnchor(iAnchor);
}

void PlotWidget::set_x_tick_dynamic(const f64 iAnchor, const f64 iInterval) const
{
  mXTickAuto = false;
  mpXAxis->setTickType(QValueAxis::TicksDynamic);
  mpXAxis->setTickInterval(iInterval);
  mpXAxis->setTickAnchor(iAnchor);
  mpXAxis->setMinorTickCount(_nice_minor_tick_count(iInterval));
}

void PlotWidget::set_y_tick_count(const f64 iAnchor, const i32 iCount) const
{
  mYTickAuto = false;
  mpYAxis->setTickType(QValueAxis::TicksFixed);
  mpYAxis->setTickCount(iCount);
  mpYAxis->setTickAnchor(iAnchor);
}

void PlotWidget::set_y_tick_dynamic(const f64 iAnchor, const f64 iInterval) const
{
  mYTickAuto = false;
  mpYAxis->setTickType(QValueAxis::TicksDynamic);
  mpYAxis->setTickInterval(iInterval);
  mpYAxis->setTickAnchor(iAnchor);
  mpYAxis->setMinorTickCount(_nice_minor_tick_count(iInterval));
  _update_y_label_format();
}

void PlotWidget::setup_x_zoom(const SRange & iRange)
{
  _init_axis_data(mXData, iRange);
  if (!mXData.isZoomed)
  {
    _apply_x_default_range();
  }
}

void PlotWidget::setup_y_zoom(const SRange & iRange)
{
  _init_axis_data(mYData, iRange);
  if (!mYData.isZoomed)
  {
    _apply_y_default_range();
  }
}

void PlotWidget::reset_x_zoom()
{
  mXData.isZoomed = false;
  if (mXData.allowChange)
  {
    _apply_x_default_range();
  }
}

void PlotWidget::_apply_x_default_range() const
{
  set_x_range(mXData.range.MinDefault, mXData.range.MaxDefault);
}

void PlotWidget::_apply_y_default_range() const
{
  set_y_range(mYData.range.MinDefault, mYData.range.MaxDefault);
}

void PlotWidget::reset_y_zoom()
{
  mYData.isZoomed = false;
  if (mYData.allowChange)
  {
    _apply_y_default_range();
  }
}

void PlotWidget::_init_axis_data(SAxisData & oData, const SRange & iRange) const
{
  oData.range = iRange;
  const f64 span = iRange.MaxDefault - iRange.MinDefault;
  oData.minRange = span / cMaxZoomFactor;
  oData.allowChange = (span > 0.0);
}

void PlotWidget::_zoom_axis(QValueAxis * const iopAxis, SAxisData & ioData, const f64 iPivot, const f64 iScale) const
{
  if (!ioData.allowChange) return;

  const f64 curMin = iopAxis->min();
  const f64 curMax = iopAxis->max();
  const f64 curRange = curMax - curMin;

  const f64 newRange = std::max(curRange * iScale, ioData.minRange);
  const f64 t = (iPivot - curMin) / curRange;  // relative position of pivot within current range
  f64 newMin = iPivot - newRange * t;
  f64 newMax = iPivot + newRange * (1.0 - t);

  // Clamp to zoom-out limits
  const f64 hardMin = ioData.range.MinDefault + ioData.range.MinZoomOutDelta;
  const f64 hardMax = ioData.range.MaxDefault + ioData.range.MaxZoomOutDelta;
  if (newMin < hardMin) newMin = hardMin;
  if (newMax > hardMax) newMax = hardMax;

  iopAxis->setRange(newMin, newMax);

  // For auto-managed axes, recompute major interval for the new span so that
  // zooming in adds denser ticks and zooming out removes them.
  // For manually-overridden axes, just keep minor ticks consistent.
  const bool isAutoAxis = (iopAxis == mpXAxis) ? mXTickAuto : mYTickAuto;
  if (isAutoAxis)
  {
    _reapply_auto_ticks();
  }
  else
  {
    const f64 majorInterval = (iopAxis->tickType() == QValueAxis::TicksDynamic)
                                ? iopAxis->tickInterval()
                                : (newMax - newMin) / std::max(iopAxis->tickCount() - 1, 1);
    iopAxis->setMinorTickCount(_nice_minor_tick_count(majorInterval));
    if (iopAxis == mpYAxis)
    {
      _update_y_label_format();
    }
  }
}

void PlotWidget::_pan_axis(QValueAxis * const iopAxis, const SAxisData & ioData, const f64 iDeltaValue) const
{
  if (!ioData.allowChange) return;

  const f64 newMin = iopAxis->min() - iDeltaValue;
  const f64 newMax = iopAxis->max() - iDeltaValue;

  const f64 hardMin = ioData.range.MinDefault + ioData.range.MinZoomOutDelta;
  const f64 hardMax = ioData.range.MaxDefault + ioData.range.MaxZoomOutDelta;

  if (newMin >= hardMin && newMax <= hardMax)
  {
    iopAxis->setRange(newMin, newMax);
  }
}

void PlotWidget::wheelEvent(QWheelEvent * const iopEvent)
{
  const QPointF screenPos = iopEvent->position();
  const QPointF chartValue = chart()->mapToValue(screenPos);
  const f64 zoomScale = (iopEvent->angleDelta().y() > 0) ? 1.0 - cZoomFactor : 1.0 + cZoomFactor;

  if ((iopEvent->modifiers() & Qt::ShiftModifier) != 0)
  {
    mXData.isZoomed = true;
    _zoom_axis(mpXAxis, mXData, chartValue.x(), zoomScale);
  }

  if ((iopEvent->modifiers() & Qt::ControlModifier) != 0)
  {
    mYData.isZoomed = true;
    _zoom_axis(mpYAxis, mYData, chartValue.y(), zoomScale);
  }

  iopEvent->accept();
}

void PlotWidget::mousePressEvent(QMouseEvent * const iopEvent)
{
  if (iopEvent->button() == Qt::LeftButton)
  {
    mPanning = true;
    mLastPos = iopEvent->pos();
  }
  else if (iopEvent->button() == Qt::RightButton)
  {
    if ((iopEvent->modifiers() & Qt::ShiftModifier) != 0)
    {
      reset_x_zoom();
    }
    if ((iopEvent->modifiers() & Qt::ControlModifier) != 0)
    {
      reset_y_zoom();
    }
  }

  QChartView::mousePressEvent(iopEvent);
}

void PlotWidget::mouseReleaseEvent(QMouseEvent * const iopEvent)
{
  if (iopEvent->button() == Qt::LeftButton)
  {
    mPanning = false;
  }
  QChartView::mouseReleaseEvent(iopEvent);
}

void PlotWidget::mouseMoveEvent(QMouseEvent * const iopEvent)
{
  if (mPanning)
  {
    const QPointF curValue = chart()->mapToValue(iopEvent->pos());
    const QPointF lastValue = chart()->mapToValue(mLastPos);
    const QPointF delta = curValue - lastValue;

    if ((iopEvent->modifiers() & Qt::ShiftModifier) != 0)
    {
      mXData.isZoomed = true;
      _pan_axis(mpXAxis, mXData, delta.x());
    }

    if ((iopEvent->modifiers() & Qt::ControlModifier) != 0)
    {
      mYData.isZoomed = true;
      _pan_axis(mpYAxis, mYData, delta.y());
    }

    mLastPos = iopEvent->pos();
  }

  QChartView::mouseMoveEvent(iopEvent);
}

void PlotWidget::resizeEvent(QResizeEvent * const iopEvent)
{
  QChartView::resizeEvent(iopEvent);
  _reapply_auto_ticks();
}

// Recomputes tick intervals for auto-managed axes based on the current plot-area
// pixel size. Falls back to 5 target intervals when called before first layout.
void PlotWidget::_reapply_auto_ticks() const
{
  const QRectF plotArea = chart()->plotArea();

  if (mXTickAuto && mpXAxis->tickType() == QValueAxis::TicksDynamic)
  {
    const f64 span = mpXAxis->max() - mpXAxis->min();
    const i32 target = plotArea.isEmpty()
                         ? 5
                         : std::max(2, static_cast<i32>(plotArea.width() / cMinPixelsPerXTick));
    const f64 interval = _nice_major_interval(span, target);
    mpXAxis->setTickInterval(interval);
    mpXAxis->setMinorTickCount(_nice_minor_tick_count(interval));
  }

  if (mYTickAuto && mpYAxis->tickType() == QValueAxis::TicksDynamic)
  {
    const f64 span = mpYAxis->max() - mpYAxis->min();
    const i32 target = plotArea.isEmpty()
                         ? 5
                         : std::max(2, static_cast<i32>(plotArea.height() / cMinPixelsPerYTick));
    f64 interval = _nice_major_interval(span, target);
    // Guarantee at least 2 major ticks visible. With TicksDynamic anchored at 0,
    // ticks land at n*interval — the range may contain only one depending on alignment.
    // Halve the interval until at least 2 ticks fall inside [axMin, axMax].
    const f64 axMin = mpYAxis->min();
    const f64 axMax = mpYAxis->max();
    while (interval > 0.0)
    {
      const i32 nVisible = (i32)std::floor(axMax / interval) - (i32)std::ceil(axMin / interval) + 1;
      if (nVisible >= 2) break;
      interval /= 2.0;
    }
    mpYAxis->setTickInterval(interval);
    mpYAxis->setMinorTickCount(_nice_minor_tick_count(interval));
    _update_y_label_format();
  }
}

// Rounds rangeSpan/iTargetIntervals UP to the nearest {2,5,10}×10^n value.
// iTargetIntervals=5 matches Qwt's default, giving identical major tick spacing.
f64 PlotWidget::_nice_major_interval(const f64 iRangeSpan, const i32 iTargetIntervals /*= 5*/) const
{
  if (iRangeSpan <= 0.0 || iTargetIntervals <= 0) return iRangeSpan;
  const f64 v = iRangeSpan / static_cast<f64>(iTargetIntervals) * (1.0 - 1e-6);
  const f64 p = std::floor(std::log10(v));
  const f64 frac = std::pow(10.0, std::log10(v) - p);  // ∈ [1, 10)

  // Halving loop — identical to Qwt: n/2 uses integer division, so the
  // effective thresholds are frac≤2→n=2, frac≤5→n=5, frac>5→n=10,
  // i.e. the raw interval is always rounded UP to the next nice number.
  i32 n = 10;
  while (n > 1 && frac <= static_cast<f64>(n / 2)) n /= 2;

  return static_cast<f64>(n) * std::pow(10.0, p);
}

// Computes minor tick count between two major ticks using the same {1, 2, 5}×10^n
// scaling as the major ticks. Works for any scale (including intervals < 1).
i32 PlotWidget::_nice_minor_tick_count(const f64 iMajorInterval, const i32 iMaxMinorSteps /*= 5*/) const
{
  if (iMajorInterval <= 0.0 || iMaxMinorSteps <= 0) return 0;

  const f64 ideal = iMajorInterval / iMaxMinorSteps;
  const f64 lx = std::log10(ideal);
  const f64 p = std::floor(lx);
  const f64 frac = std::pow(10.0, lx - p);   // ∈ [1, 10)

  // Halving loop (integer division): rounds frac down to {1, 2, 5, 10}
  i32 n = 10;
  while (n > 1 && frac <= (f64)(n / 2)) n /= 2;

  const f64 minorStep = (f64)n * std::pow(10.0, p);

  return std::clamp((i32)(std::ceil(iMajorInterval / minorStep)) - 1, 0, iMaxMinorSteps - 1);
}
