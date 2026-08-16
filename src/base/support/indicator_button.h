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
#include <QPushButton>
#include <QColor>

class IndicatorButton : public QPushButton
{
  Q_OBJECT
  Q_PROPERTY(bool indicatorActive READ is_indicator_active WRITE set_indicator_active NOTIFY signal_indicator_active_changed)
  Q_PROPERTY(QColor indicatorColor READ get_indicator_color WRITE set_indicator_color)
  Q_PROPERTY(f32 indicatorDiameter READ get_indicator_diameter WRITE set_indicator_diameter)
  Q_PROPERTY(f32 indicatorMarginRight READ get_indicator_margin_right WRITE set_indicator_margin_right)
  Q_PROPERTY(f32 indicatorMarginTop READ get_indicator_margin_top WRITE set_indicator_margin_top)

public:
  explicit IndicatorButton(QWidget * parent = nullptr);
  explicit IndicatorButton(const QString & text, QWidget * parent = nullptr);
  IndicatorButton(const QIcon & icon, const QString & text, QWidget * parent = nullptr);
  ~IndicatorButton() override = default;

  void set_indicator_active(bool iActive);
  [[nodiscard]] bool is_indicator_active() const { return mIndicatorActive; }

  void set_indicator_color(const QColor & iColor);
  [[nodiscard]] const QColor & get_indicator_color() const { return mIndicatorColor; }

  void set_indicator_diameter(f32 iDiameter);
  [[nodiscard]] f32 get_indicator_diameter() const { return mIndicatorDiameter; }

  void set_indicator_margin_right(f32 iMargin);
  [[nodiscard]] f32 get_indicator_margin_right() const { return mIndicatorMarginRight; }

  void set_indicator_margin_top(f32 iMargin);
  [[nodiscard]] f32 get_indicator_margin_top() const { return mIndicatorMarginTop; }

public slots:
  void slot_set_indicator_active(bool iActive) { set_indicator_active(iActive); }

signals:
  void signal_indicator_active_changed(bool iActive);

protected:
  void paintEvent(QPaintEvent * event) override;

private:
  bool mIndicatorActive = false;
  QColor mIndicatorColor{0xFF, 0xD0, 0x00}; // Vivid Amber Yellow LED
  f32 mIndicatorDiameter = 6.0f;
  f32 mIndicatorMarginRight = 3.0f;
  f32 mIndicatorMarginTop = 3.0f;
};
