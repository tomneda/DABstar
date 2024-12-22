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
  explicit CustQwtZoomPan(QwtPlot * plot);
  ~CustQwtZoomPan() override = default;

protected:
  bool eventFilter(QObject * object, QEvent * event) override;

private:
  QwtPlot * const mpQwtPlot;
  bool mPanning = false;
  QPoint mLastPos{};

  void _handle_mouse_press(QMouseEvent * event);
  void _handle_mouse_release(QMouseEvent * event);
  void _handle_mouse_move(QMouseEvent * event);
  void _handle_wheel_event(QWheelEvent * event);
};

#endif // CUSTOMZOOMPAN_H
