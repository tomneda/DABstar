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
#include <QWidget>
#include <QPair>
#include <QVector>

// It paints a filled bar (horizontal or vertical) from lowerBound to value
// using a configurable gradient, optionally with a tick-mark scale.
//
// Usage in Qt Designer: promoted widget, base class QWidget, header "level_meter.h"
// Supported designer properties: orientation, lowerBound, upperBound, showScale, barVisible

class LevelMeter : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(f64 lowerBound READ get_lower_bound WRITE set_lower_bound)
  Q_PROPERTY(f64 upperBound READ get_upper_bound WRITE set_upper_bound)
  Q_PROPERTY(Qt::Orientation orientation READ get_orientation WRITE set_orientation)
  Q_PROPERTY(bool showScale READ get_show_scale WRITE set_show_scale)
  Q_PROPERTY(bool barVisible READ get_bar_visible WRITE set_bar_visible)

public:
  explicit LevelMeter(QWidget * parent = nullptr);

  [[nodiscard]] Qt::Orientation get_orientation() const { return mOrientation; }
  [[nodiscard]] f64 get_lower_bound() const { return mLower; }
  [[nodiscard]] f64 get_upper_bound() const { return mUpper; }
  [[nodiscard]] bool get_show_scale() const { return mShowScale; }
  [[nodiscard]] bool get_bar_visible() const { return mBarVisible; }

  void set_lower_bound(f64 iV);
  void set_upper_bound(f64 iV);
  void set_orientation(Qt::Orientation iO);
  void set_value(f64 iV);
  void set_show_scale(bool iV);
  void set_bar_visible(bool iV);

  // Set a gradient from a list of (relative_position [0..1], color) stops.
  void set_color_stops(const QVector<QPair<f64, u32>> & iStops);

  [[nodiscard]] QSize sizeHint() const override;
  [[nodiscard]] QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent * ipEvent) override;

private:
  f64 mLower = -22.0;
  f64 mUpper = 0.0;
  f64 mValue = -100.0;
  Qt::Orientation mOrientation = Qt::Horizontal;
  bool mShowScale = true;
  bool mBarVisible = true;

  QVector<QPair<f64, u32>> mStops;

  static constexpr f64 cMajorStep = 3.0;   // dB between labeled ticks
  static constexpr f64 cMinorStep = 1.0;   // dB between minor ticks

  [[nodiscard]] int _get_scale_area_size() const;
  [[nodiscard]] int _get_edge_margin() const;
  [[nodiscard]] QColor _get_stops_color(f64 iRelPos) const;
  inline QString _get_format_tick(f64 iV) const { return QString::number(static_cast<int>(std::round(iV))); }
  void _draw_scale(QPainter & ioPainter, const QRect & iScaleRect, int iMargin) const;
};
