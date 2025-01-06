/*
 * Copyright (c) 2024 by Thomas Neder (https://github.com/tomneda)
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

#include "cust_qwt_zoom_pan.h"
#include <QWheelEvent>
#include <QPoint>
#include <qwt_scale_div.h>
#include <qwt_plot.h>

CustQwtZoomPan::CustQwtZoomPan(QwtPlot * ipPlot, const SRange & iRangeX, const SRange & iRangeY)
  : QObject(ipPlot)
  , mpQwtPlot(ipPlot)
{
  assert(mpQwtPlot != nullptr);

  set_x_range(iRangeX);
  set_y_range(iRangeY);
  reset_x_zoom();
  reset_y_zoom();

  mpQwtPlot->canvas()->installEventFilter(this);
}

void CustQwtZoomPan::reset_x_zoom()
{
  mDataX.IsZoomed = false;

  if (mDataX.AllowChange)
  {
    mpQwtPlot->setAxisScale(QwtPlot::xBottom, mDataX.Range.MinDefault, mDataX.Range.MaxDefault);
  }

  mpQwtPlot->replot();
}

void CustQwtZoomPan::reset_y_zoom()
{
  mDataY.IsZoomed = false;

  if (mDataY.AllowChange)
  {
    mpQwtPlot->setAxisScale(QwtPlot::yLeft, mDataY.Range.MinDefault, mDataY.Range.MaxDefault);
  }

  mpQwtPlot->replot();
}

void CustQwtZoomPan::_set_range(SInitData & oInitData, const SRange & iRange)
{
  assert(iRange.MinZoomOutDelta <= 0); // MinZoomOutDelta must be zero or negative
  assert(iRange.MaxZoomOutDelta >= 0); // MaxZoomOutDelta must be zero or positive
  assert(iRange.MinDefault + iRange.MinZoomOutDelta <= iRange.MaxDefault + iRange.MaxZoomOutDelta);
  oInitData.Range = iRange;
  oInitData.MinNrPoints = std::round((iRange.MaxDefault - iRange.MinDefault) / cMaxZoomFactor);
  oInitData.AllowChange = (oInitData.Range.MinDefault < oInitData.Range.MaxDefault); // if MinDefault and MaxDefault are zero, no zooming is wanted
}

void CustQwtZoomPan::set_x_range(const SRange & iRange)
{
  _set_range(mDataX, iRange);

  if (mDataX.AllowChange && !mDataX.IsZoomed)
  {
    mpQwtPlot->setAxisScale(QwtPlot::xBottom, mDataX.Range.MinDefault, mDataX.Range.MaxDefault);
    mpQwtPlot->replot();
  }
}

void CustQwtZoomPan::set_y_range(const SRange & iRange)
{
  _set_range(mDataY, iRange);

  if (mDataY.AllowChange && !mDataY.IsZoomed)
  {
    mpQwtPlot->setAxisScale(QwtPlot::yLeft, mDataY.Range.MinDefault, mDataY.Range.MaxDefault);
    mpQwtPlot->replot();
  }
}

bool CustQwtZoomPan::eventFilter(QObject * object, QEvent * event)
{
  if (object == mpQwtPlot->canvas())
  {
    switch (event->type())
    {
    case QEvent::MouseButtonPress:
      _handle_mouse_press(static_cast<QMouseEvent*>(event));
      break;
    case QEvent::MouseButtonRelease:
      _handle_mouse_release(static_cast<QMouseEvent*>(event));
      break;
    case QEvent::MouseMove:
      _handle_mouse_move(static_cast<QMouseEvent*>(event));
      break;
    case QEvent::Wheel:
      _handle_wheel_event(static_cast<QWheelEvent*>(event));
      break;
    default:;
    }
  }
  return QObject::eventFilter(object, event);
}

void CustQwtZoomPan::_handle_mouse_press(const QMouseEvent * const event)
{
  if (event->button() == Qt::LeftButton)
  {
    mPanning = true;
    mLastPos = event->pos();
  }
  else if (event->button() == Qt::RightButton)
  {
    if ((event->modifiers() & Qt::ShiftModifier) != 0)
    {
      reset_x_zoom();
    }

    if ((event->modifiers() & Qt::ControlModifier) != 0)
    {
      reset_y_zoom();
    }
  }
}

void CustQwtZoomPan::_handle_mouse_release(const QMouseEvent * const event)
{
  if (event->button() == Qt::LeftButton)
  {
    mPanning = false;
  }
}

void CustQwtZoomPan::_handle_mouse_move(const QMouseEvent * const event)
{
  auto set_axis_scale = [this](const QwtAxisId iAxisId, const SInitData &iData, const double iDelta)
  {
    const double curMin = mpQwtPlot->axisScaleDiv(iAxisId).lowerBound();
    const double curMax = mpQwtPlot->axisScaleDiv(iAxisId).upperBound();
    const double curRange = curMax - curMin;
    const double dx = iDelta * curRange;
    const double newMin = curMin + dx;
    const double newMax = curMax + dx;

    if (newMin >= iData.Range.MinDefault + iData.Range.MinZoomOutDelta &&
        newMax <= iData.Range.MaxDefault + iData.Range.MaxZoomOutDelta)
    {
      mpQwtPlot->setAxisScale(iAxisId, newMin, newMax);
    }
  };

  if (mPanning)
  {
    const QPoint delta = event->pos() - mLastPos;

    if ((event->modifiers() & Qt::ShiftModifier) != 0)
    {
      if (mDataX.AllowChange)
      {
        mDataX.IsZoomed = true;
        set_axis_scale(QwtPlot::xBottom, mDataX, -(double)delta.x() / mpQwtPlot->canvas()->width());
      }
    }

    if ((event->modifiers() & Qt::ControlModifier) != 0)
    {
      if (mDataY.AllowChange)
      {
        mDataY.IsZoomed = true;
        set_axis_scale(QwtPlot::yLeft, mDataY, (double)delta.y() / mpQwtPlot->canvas()->height());
      }
    }
    mLastPos = event->pos();
    mpQwtPlot->replot();
  }
}

void CustQwtZoomPan::_handle_wheel_event(const QWheelEvent * const event)
{
  auto set_axis_scale = [this](const QwtAxisId iAxisId, const SInitData & iData, const double iZoomScale, const double iRelMousePos)
  {
    const double curMin = mpQwtPlot->axisScaleDiv(iAxisId).lowerBound();
    const double curMax = mpQwtPlot->axisScaleDiv(iAxisId).upperBound();
    const double curRange = curMax - curMin;
    const double newRange = std::max(curRange * iZoomScale, iData.MinNrPoints);
    const double curValAtMousePos = curMin + iRelMousePos * curRange;
    double newMin = curValAtMousePos - newRange * iRelMousePos;
    double newMax = curValAtMousePos + newRange * (1.0 - iRelMousePos);
    if (newMin < iData.Range.MinDefault + iData.Range.MinZoomOutDelta) newMin = iData.Range.MinDefault + iData.Range.MinZoomOutDelta;
    if (newMax > iData.Range.MaxDefault + iData.Range.MaxZoomOutDelta) newMax = iData.Range.MaxDefault + iData.Range.MaxZoomOutDelta;
    mpQwtPlot->setAxisScale(iAxisId, newMin, newMax);
  };

  constexpr double cZoomFactor = 0.1; // zoom factor for mouse wheel
  const double zoomScale = (event->angleDelta().y() > 0) ? 1 - cZoomFactor : 1 + cZoomFactor;

  const QPointF pos = event->position();

  if ((event->modifiers() & Qt::ShiftModifier) != 0)
  {
    if (mDataX.AllowChange)
    {
      mDataX.IsZoomed = true;
      set_axis_scale(QwtPlot::xBottom, mDataX, zoomScale, pos.x() / mpQwtPlot->canvas()->width());
    }
  }

  if ((event->modifiers() & Qt::ControlModifier) != 0)
  {
    if (mDataY.AllowChange)
    {
      mDataY.IsZoomed = true;
      set_axis_scale(QwtPlot::yLeft, mDataY, zoomScale, 1.0 - pos.y() / mpQwtPlot->canvas()->height());
    }
  }
  mpQwtPlot->replot();
}
