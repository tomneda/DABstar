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
  struct SRange
  {
    explicit SRange(const double iMinDefault = 0, const double iMaxDefault = 0,
                    const double iMinZoomOutDelta = 0, const double iMaxZoomOutDelta = 0)
      : MinDefault(iMinDefault)
      , MaxDefault(iMaxDefault)
      , MinZoomOutDelta(iMinZoomOutDelta)
      , MaxZoomOutDelta(iMaxZoomOutDelta) {}

    double MinDefault = 0;
    double MaxDefault = 0;
    double MinZoomOutDelta = 0;
    double MaxZoomOutDelta = 0;
  };


  explicit CustQwtZoomPan(QwtPlot * ipPlot, const SRange & iRangeX = SRange(), const SRange & iRangeY = SRange());
  ~CustQwtZoomPan() override = default;

  void reset_x_zoom();
  void reset_y_zoom();
  void set_x_range(const SRange & iRange);
  void set_y_range(const SRange & iRange);

protected:
  bool eventFilter(QObject * object, QEvent * event) override;

private:
  static constexpr double cMaxZoomFactor = 100.0;

  QwtPlot * const mpQwtPlot;

  struct SInitData
  {
    SRange Range{};
    double MinNrPoints = 0;
    bool IsZoomed = false;
    bool AllowChange = false;
  };

  SInitData mDataX{};
  SInitData mDataY{};
  bool mPanning = false;
  QPoint mLastPos{};

  void _set_range(SInitData & oInitData, const SRange & iRange);
  void _handle_mouse_press(const QMouseEvent * event);
  void _handle_mouse_release(const QMouseEvent * event);
  void _handle_mouse_move(const QMouseEvent * event);
  void _handle_wheel_event(const QWheelEvent * event);
};

#endif // CUSTOMZOOMPAN_H
