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
  , mxMin(ixMin)
  , mxMax(ixMax)
  , myMin(iyMin)
  , myMax(iyMax)
{
  mpQwtPlot->canvas()->installEventFilter(this);

  mAllowXChange = (mxMin < mxMax);
  mAllowYChange = (myMin < myMax);
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
      mpQwtPlot->setAxisScale(QwtPlot::xBottom, mxMin, mxMax);
    }

    if (mAllowYChange)
    {
      mpQwtPlot->setAxisScale(QwtPlot::yLeft, myMin, myMax);
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

void CustQwtZoomPan::_handle_mouse_move(QMouseEvent * event)
{
  if (mPanning)
  {
    const QPoint delta = event->pos() - mLastPos;

    if (mAllowXChange)
    {
      const double xMin = mpQwtPlot->axisScaleDiv(QwtPlot::xBottom).lowerBound();
      const double xMax = mpQwtPlot->axisScaleDiv(QwtPlot::xBottom).upperBound();
      const double xRange = xMax - xMin;
      const double dx = -delta.x() * (xRange / mpQwtPlot->canvas()->width());
      mpQwtPlot->setAxisScale(QwtPlot::xBottom, xMin + dx, xMax + dx);
    }

    if (mAllowYChange)
    {
      const double yMin = mpQwtPlot->axisScaleDiv(QwtPlot::yLeft).lowerBound();
      const double yMax = mpQwtPlot->axisScaleDiv(QwtPlot::yLeft).upperBound();
      const double yRange = yMax - yMin;
      const double dy =  delta.y() * (yRange / mpQwtPlot->canvas()->height());
      mpQwtPlot->setAxisScale(QwtPlot::yLeft,   yMin + dy, yMax + dy);
    }

    mLastPos = event->pos();

    mpQwtPlot->replot();
  }
}

void CustQwtZoomPan::_handle_wheel_event(QWheelEvent * event)
{
  constexpr double cZoomFactor = 0.1; // zoom factor for mouse wheel
  const double zoomScale = (event->angleDelta().y() > 0) ? 1 - cZoomFactor : 1 + cZoomFactor;

  if (mAllowXChange)
  {
    const double xMin = mpQwtPlot->axisScaleDiv(QwtPlot::xBottom).lowerBound();
    const double xMax = mpQwtPlot->axisScaleDiv(QwtPlot::xBottom).upperBound();
    const double xRange = xMax - xMin;
    const double centerX = (xMin + xMax) / 2.0;
    const double newXRange = xRange * zoomScale;
    mpQwtPlot->setAxisScale(QwtPlot::xBottom, centerX - newXRange / 2.0, centerX + newXRange / 2.0);
  }

  if (mAllowYChange)
  {
    const double yMin = mpQwtPlot->axisScaleDiv(QwtPlot::yLeft).lowerBound();
    const double yMax = mpQwtPlot->axisScaleDiv(QwtPlot::yLeft).upperBound();
    const double yRange = yMax - yMin;
    const double centerY = (yMin + yMax) / 2.0;
    const double newYRange = yRange * zoomScale;
    mpQwtPlot->setAxisScale(QwtPlot::yLeft,   centerY - newYRange / 2.0, centerY + newYRange / 2.0);
  }

  mpQwtPlot->replot();
}
