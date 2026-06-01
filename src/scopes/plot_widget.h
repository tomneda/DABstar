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

#pragma once

#include "glob_data_types.h"
#include <QChartView>
#include <QValueAxis>
#include <QLineSeries>
#include <QMouseEvent>
#include <QWheelEvent>

// PlotWidget is a QChartView subclass that provides:
//   - Built-in zoom (Shift+wheel = X, Ctrl+wheel = Y) and
//   - Pan (Shift+drag = X, Ctrl+drag = Y)
//   - Right-click reset zoom (Shift+right = X, Ctrl+right = Y)

class PlotWidget : public QChartView
{
  Q_OBJECT

public:
  struct SRange
  {
    explicit SRange(const f64 iMinDefault = 0, const f64 iMaxDefault = 0,
                    const f64 iMinZoomOutDelta = 0, const f64 iMaxZoomOutDelta = 0)
      : MinDefault(iMinDefault)
      , MaxDefault(iMaxDefault)
      , MinZoomOutDelta(iMinZoomOutDelta)
      , MaxZoomOutDelta(iMaxZoomOutDelta)
    {}

    f64 MinDefault = 0;
    f64 MaxDefault = 0;
    f64 MinZoomOutDelta = 0;
    f64 MaxZoomOutDelta = 0;
  };

  explicit PlotWidget(QWidget * parent = nullptr);

  [[nodiscard]] QValueAxis * get_x_axis() const { return mpXAxis; }
  [[nodiscard]] QValueAxis * get_y_axis() const { return mpYAxis; }

  void set_x_range(f64 iMin, f64 iMax) const;
  void set_y_range(f64 iMin, f64 iMax) const;

  // Fixed mode: n evenly-spaced ticks (Qt divides range by n-1).
  void set_x_tick_count(const f64 iAnchor, int iCount) const;

  // Dynamic mode: ticks at iAnchor ± n*iInterval. Anchor and interval remain
  // fixed during zoom/pan — only the visible window moves. Use this whenever
  // the center point and step size are meaningful (spectrum, audio FFT, etc.).
  void set_x_tick_dynamic(f64 iAnchor, f64 iInterval) const;

  // Configure zoom-out limits (call once, then zoom/pan are bounded)
  void setup_x_zoom(const SRange & iRange);
  void setup_y_zoom(const SRange & iRange);
  void reset_x_zoom();
  void reset_y_zoom();

signals:
  void signal_plot_area_changed(int iLeftMargin, int iRightMargin);

protected:
  void wheelEvent(QWheelEvent * iopEvent) override;
  void mousePressEvent(QMouseEvent * iopEvent) override;
  void mouseReleaseEvent(QMouseEvent * iopEvent) override;
  void mouseMoveEvent(QMouseEvent * iopEvent) override;

private:
  static constexpr f64 cZoomFactor = 0.1;
  static constexpr f64 cMaxZoomFactor = 100.0;

  QValueAxis * mpXAxis = nullptr;
  QValueAxis * mpYAxis = nullptr;

  struct SAxisData
  {
    SRange range{};
    f64    minRange = 0;      // minimum span (max zoom-in)
    bool   allowChange = false;
    bool   isZoomed = false;
  };

  SAxisData mXData{};
  SAxisData mYData{};

  bool   mPanning = false;
  QPoint mLastPos{};

  void _init_axis_data(SAxisData & oData, const SRange & iRange) const;
  void _apply_x_default_range() const;
  void _update_y_label_format() const;
  void _zoom_axis(QValueAxis * iopAxis, SAxisData & ioData, f64 iPivot, f64 iScale) const;
  void _pan_axis(QValueAxis * iopAxis, const SAxisData & ioData, f64 iDeltaValue) const;
  i32 _nice_minor_tick_count(const f64 iMajorInterval, const int iMaxMinorSteps = 5) const;
};
