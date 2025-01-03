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

#ifndef CUSTOMZOOMPAN_H
#define CUSTOMZOOMPAN_H

#include <QObject>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPoint>
#include <qwt_plot.h>

class CustQwtZoomPan : public QObject
{
  Q_OBJECT

public:
  explicit CustQwtZoomPan(QwtPlot * ipPlot,
                          int32_t ixMin = 0, int32_t ixMax = 0,
                          int32_t iyMin = 0, int32_t iyMax = 0);
  ~CustQwtZoomPan() override = default;

  void reset_zoom();
  void set_x_range(double ixMin, double ixMax);
  void set_y_range(double iyMin, double iyMax);

protected:
  bool eventFilter(QObject * object, QEvent * event) override;

private:
  static constexpr double cMaxZoomFactor = 100.0;

  QwtPlot * const mpQwtPlot;
  struct SInitData
  {
    double Min = 0;
    double Max = 0;
    double MinNrPoints = 0;
    bool IsZoomed = false;
    bool AllowChange = false;
  };

  SInitData mDataX{};
  SInitData mDataY{};
  bool mPanning = false;
  QPoint mLastPos{};

  void _handle_mouse_press(const QMouseEvent * event);
  void _handle_mouse_release(const QMouseEvent * event);
  void _handle_mouse_move(const QMouseEvent * event);
  void _handle_wheel_event(const QWheelEvent * event);
};

#endif // CUSTOMZOOMPAN_H
