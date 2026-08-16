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

#include "indicator_button.h"
#include <QPainter>
#include <QPaintEvent>
#include <QRadialGradient>

IndicatorButton::IndicatorButton(QWidget * parent)
  : QPushButton(parent)
{
}

IndicatorButton::IndicatorButton(const QString & text, QWidget * parent)
  : QPushButton(text, parent)
{
}

IndicatorButton::IndicatorButton(const QIcon & icon, const QString & text, QWidget * parent)
  : QPushButton(icon, text, parent)
{
}

void IndicatorButton::set_indicator_active(const bool iActive)
{
  if (mIndicatorActive != iActive)
  {
    mIndicatorActive = iActive;
    update();
    emit signal_indicator_active_changed(mIndicatorActive);
  }
}

void IndicatorButton::set_indicator_color(const QColor & iColor)
{
  if (mIndicatorColor != iColor)
  {
    mIndicatorColor = iColor;
    if (mIndicatorActive)
    {
      update();
    }
  }
}

void IndicatorButton::set_indicator_diameter(const f32 iDiameter)
{
  if (mIndicatorDiameter != iDiameter)
  {
    mIndicatorDiameter = iDiameter;
    if (mIndicatorActive)
    {
      update();
    }
  }
}

void IndicatorButton::set_indicator_margin_right(const f32 iMargin)
{
  if (mIndicatorMarginRight != iMargin)
  {
    mIndicatorMarginRight = iMargin;
    if (mIndicatorActive)
    {
      update();
    }
  }
}

void IndicatorButton::set_indicator_margin_top(const f32 iMargin)
{
  if (mIndicatorMarginTop != iMargin)
  {
    mIndicatorMarginTop = iMargin;
    if (mIndicatorActive)
    {
      update();
    }
  }
}

void IndicatorButton::paintEvent(QPaintEvent * event)
{
  QPushButton::paintEvent(event);

  if (mIndicatorActive)
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal d = mIndicatorDiameter;
    const qreal rightMargin = mIndicatorMarginRight;
    const qreal topMargin = mIndicatorMarginTop;
    const qreal x = width() - rightMargin - d;
    const qreal y = topMargin;

    if (d > 0.0 && x >= 0.0 && y >= 0.0)
    {
      const QRectF dotRect(x, y, d, d);

      // Subtle radial gradient for LED jewel finish
      QRadialGradient gradient(QPointF(x + (d * 0.35), y + (d * 0.35)), d * 0.65);
      gradient.setColorAt(0.0, mIndicatorColor.lighter(160));
      gradient.setColorAt(0.65, mIndicatorColor);
      gradient.setColorAt(1.0, mIndicatorColor.darker(140));

      painter.setPen(Qt::NoPen);
      painter.setBrush(gradient);
      painter.drawEllipse(dotRect);
    }
  }
}
