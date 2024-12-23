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
#include <QwtScaleDiv>
#include <qwt_plot.h>

CustQwtZoomPan::CustQwtZoomPan(QwtPlot * ipPlot, const int32_t ixMin, const int32_t ixMax, const int32_t iyMin, const int32_t iyMax)
  : QObject(ipPlot)
  , mpQwtPlot(ipPlot)
{
  assert(mpQwtPlot != nullptr);
  assert(ixMin <= ixMax);
  assert(iyMin <= iyMax);
  
  mDataX.Min = ixMin;
  mDataX.Max = ixMax;
  mDataX.MinNrPoints = (ixMax - ixMin) / 100.0;

  mDataY.Min = iyMin;
  mDataY.Max = iyMax;
  mDataY.MinNrPoints = (ixMax - iyMin) / 100.0;

  mpQwtPlot->canvas()->installEventFilter(this);

  mAllowXChange = (mDataX.Min < mDataX.Max);
  mAllowYChange = (mDataY.Min < mDataY.Max);
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
    if (mAllowXChange)
    {
      mpQwtPlot->setAxisScale(QwtPlot::xBottom, mDataX.Min, mDataX.Max);
    }

    if (mAllowYChange)
    {
      mpQwtPlot->setAxisScale(QwtPlot::yLeft, mDataY.Min, mDataY.Max);
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
    const double dx = -iDelta * (curRange / mpQwtPlot->canvas()->width());
    const double newMin = curMin + dx;
    const double newMax = curMax + dx;

    if (newMin >= iData.Min && newMax <= iData.Max)
    {
      mpQwtPlot->setAxisScale(iAxisId, newMin, newMax);
    }
  };

  if (mPanning)
  {
    const QPoint delta = event->pos() - mLastPos;

    if (mAllowXChange)
    {
      set_axis_scale(QwtPlot::xBottom, mDataX, delta.x());
    }

    if (mAllowYChange)
    {
      set_axis_scale(QwtPlot::yLeft, mDataY, delta.y());
    }

    mLastPos = event->pos();
    mpQwtPlot->replot();
  }
}

void CustQwtZoomPan::_handle_wheel_event(const QWheelEvent * const event) const
{
  auto set_axis_scale = [this](const QwtAxisId iAxisId, const SInitData &iData, const double iZoomScale, const double iRelMousePos)
  {
    const double curMin = mpQwtPlot->axisScaleDiv(iAxisId).lowerBound();
    const double curMax = mpQwtPlot->axisScaleDiv(iAxisId).upperBound();
    const double curRange = curMax - curMin;
    const double newRange = std::max(curRange * iZoomScale, iData.MinNrPoints);
    const double curValAtMousePos = curMin + iRelMousePos * curRange;
    double newMin = curValAtMousePos - newRange * iRelMousePos;
    double newMax = curValAtMousePos + newRange * (1.0 - iRelMousePos);
    if (newMin < iData.Min) newMin = iData.Min;
    if (newMax > iData.Max) newMax = iData.Max;
    mpQwtPlot->setAxisScale(iAxisId, newMin, newMax);
  };

  constexpr double cZoomFactor = 0.1; // zoom factor for mouse wheel
  const double zoomScale = (event->angleDelta().y() > 0) ? 1 - cZoomFactor : 1 + cZoomFactor;

  const QPointF pos = event->position();

  if (mAllowXChange)
  {
    set_axis_scale(QwtPlot::xBottom, mDataX, zoomScale, pos.x() / mpQwtPlot->canvas()->width());
  }

  if (mAllowYChange)
  {
    set_axis_scale(QwtPlot::yLeft, mDataY, zoomScale, pos.y() / mpQwtPlot->canvas()->height());
  }

  mpQwtPlot->replot();
}
