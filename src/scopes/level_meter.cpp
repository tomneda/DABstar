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

#include "level_meter.h"
#include <QPainter>
#include <QPaintEvent>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>

// Default gradient: black → dark blue → green → yellow → red
static QVector<QPair<f64, u32>> default_stops()
{
  return
  {
    { 0.00, 0x000000 },
    { 0.25, 0x000080 }, 
    { 0.55, 0x00FF00 },
    { 0.80, 0xFFFF00 },
    { 1.00, 0xFF0000 },
  };
}

LevelMeter::LevelMeter(QWidget * const parent)
  : QWidget(parent)
{
  mStops = default_stops();
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void LevelMeter::set_lower_bound(const f64 iV)
{
  mLower = iV;
  update();
}

void LevelMeter::set_upper_bound(const f64 iV)
{
  mUpper = iV;
  update();
}

void LevelMeter::set_orientation(const Qt::Orientation iO)
{
  mOrientation = iO;
  update();
}

void LevelMeter::set_value(const f64 iV)
{
  mValue = iV;
  update();
}

void LevelMeter::set_show_scale(const bool iV)
{
  mShowScale = iV;
  updateGeometry();
  update();
}

void LevelMeter::set_bar_visible(const bool iV)
{
  mBarVisible = iV;
  updateGeometry();
  update();
}

void LevelMeter::set_color_stops(const QVector<QPair<f64, u32>> & iStops)
{
  mStops = iStops;
  update();
}

// Returns how many pixels the scale area needs on the bar's trailing edge.
i32 LevelMeter::_get_scale_area_size() const
{
  if (!mShowScale) return 0;
  // tick line + small gap + text height
  return fontMetrics().height() + 6;
}

// Returns the inset margin (pixels) applied at both ends of the bar/scale axis
// so that extreme labels are not clipped at the widget border.
i32 LevelMeter::_get_edge_margin() const
{
  const f64 range = mUpper - mLower;
  if (range <= 0.0 || !mShowScale) return 0;

  const f64 firstMajor = std::ceil(mLower / cMajorStep - 1e-9) * cMajorStep;
  const f64 lastMajor  = std::floor(mUpper / cMajorStep + 1e-9) * cMajorStep;

  const QFontMetrics fm = fontMetrics();

  if (mOrientation == Qt::Horizontal)
  {
    const i32 halfFirst = (fm.horizontalAdvance(_get_format_tick(firstMajor)) + 1) / 2;
    const i32 halfLast  = (fm.horizontalAdvance(_get_format_tick(lastMajor))  + 1) / 2;
    return std::max(halfFirst, halfLast);
  }
  else // Vertical: top/bottom margin = half a text line height
  {
    return (fm.height() + 1) / 2;
  }
}

QSize LevelMeter::sizeHint() const
{
  const i32 scale = _get_scale_area_size();
  const i32 barThick = mBarVisible ? 8 : 0;

  if (mOrientation == Qt::Horizontal)
    return QSize(200, barThick + scale);
  else
    return QSize(barThick + scale, 100);
}

QSize LevelMeter::minimumSizeHint() const
{
  return sizeHint();
}

void LevelMeter::_draw_scale(QPainter & ioPainter, const QRect & iScaleRect, const i32 iMargin) const
{
  const f64 range = mUpper - mLower;
  if (range <= 0.0) return;

  const f64 firstMajor = std::ceil(mLower / cMajorStep - 1e-9) * cMajorStep;
  const f64 firstMinor = std::ceil(mLower / cMinorStep - 1e-9) * cMinorStep;

  const QFontMetrics fm = fontMetrics();
  const i32 textH = fm.height();
  constexpr i32 cMajorTickLen = 4;
  constexpr i32 cMinorTickLen = 2;

  ioPainter.setPen(QPen(Qt::white, 1));

  // Helper: map a value in [mLower..mUpper] to a pixel coordinate within the
  // scale rect, respecting the edge margin on both sides.
  auto toPixel = [&](const f64 val) -> i32
  {
    const f64 pos = (val - mLower) / range;
    if (mOrientation == Qt::Horizontal)
      return iScaleRect.left() + iMargin + (i32)(pos * (iScaleRect.width() - 2 * iMargin - 1));
    else
      return iScaleRect.bottom() - iMargin - (i32)(pos * (iScaleRect.height() - 2 * iMargin - 1));
  };

  // --- Minor ticks (no labels) ---
  const i32 maxMinorIdx = (i32)std::ceil((mUpper - firstMinor) / cMinorStep) + 2;
  for (i32 i = 0; i < maxMinorIdx; ++i)
  {
    const f64 tick = firstMinor + i * cMinorStep;
    if (tick < mLower - cMinorStep * 0.01) continue;
    if (tick > mUpper + cMinorStep * 0.01) break;

    const f64 relToMajor = std::fmod(std::abs(tick - firstMajor) + 1e12 * cMajorStep, cMajorStep);
    if (relToMajor < cMajorStep * 1e-5 || relToMajor > cMajorStep * (1.0 - 1e-5)) continue;

    const i32 px = toPixel(tick);
    if (mOrientation == Qt::Horizontal)
      ioPainter.drawLine(px, iScaleRect.top(), px, iScaleRect.top() + cMinorTickLen);
    else
      ioPainter.drawLine(iScaleRect.left(), px, iScaleRect.left() + cMinorTickLen, px);
  }

  // --- Major ticks with labels ---
  for (f64 tick = firstMajor; tick <= mUpper + cMajorStep * 0.01; tick += cMajorStep)
  {
    if (tick < mLower - cMajorStep * 0.01) continue;

    const i32 px = toPixel(tick);
    const QString lbl = _get_format_tick(tick);

    if (mOrientation == Qt::Horizontal)
    {
      ioPainter.drawLine(px, iScaleRect.top(), px, iScaleRect.top() + cMajorTickLen);
      const i32 tw = fm.horizontalAdvance(lbl);
      ioPainter.drawText(px - tw / 2, iScaleRect.top() + cMajorTickLen + 1 + textH - fm.descent(), lbl);
    }
    else // Vertical — bottom = lower, top = upper
    {
      ioPainter.drawLine(iScaleRect.left(), px, iScaleRect.left() + cMajorTickLen, px);
      ioPainter.drawText(iScaleRect.left() + cMajorTickLen + 2, px + textH / 4 - fm.descent(), lbl);
    }
  }
}

QColor LevelMeter::_get_stops_color(const f64 iRelPos) const
{
  if (mStops.size() < 2) return { Qt::gray };

  const f64 t = std::clamp(iRelPos, 0.0, 1.0);

  i32 seg = mStops.size() - 2;
  for (i32 s = 0; s < mStops.size() - 1; ++s)
  {
    if (t <= mStops[s + 1].first) { seg = s; break; }
  }

  const f64 t0 = mStops[seg].first;
  const f64 t1 = mStops[seg + 1].first;
  const QColor c0(mStops[seg].second);
  const QColor c1(mStops[seg + 1].second);

  const f64 frac = (t1 > t0) ? std::clamp((t - t0) / (t1 - t0), 0.0, 1.0) : 0.0;
  return {
    static_cast<i32>(c0.red()   + frac * (c1.red()   - c0.red())),
    static_cast<i32>(c0.green() + frac * (c1.green() - c0.green())),
    static_cast<i32>(c0.blue()  + frac * (c1.blue()  - c0.blue()))
  };
}

void LevelMeter::paintEvent(QPaintEvent * const /*ipEvent*/)
{
  QPainter painter(this);
  const QRect r = rect();

  painter.fillRect(r, Qt::black);

  // Split rect into bar area and scale area
  const i32 scaleSize = _get_scale_area_size();
  QRect barRect  = r;
  QRect scaleRect;

  if (mShowScale && scaleSize > 0)
  {
    if (mOrientation == Qt::Horizontal)
    {
      const i32 barH = r.height() - scaleSize;
      barRect   = QRect(r.left(), r.top(), r.width(), std::max(barH, 0));
      scaleRect = QRect(r.left(), r.top() + barH, r.width(), scaleSize);
    }
    else
    {
      const i32 barW = r.width() - scaleSize;
      barRect   = QRect(r.left(), r.top(), std::max(barW, 0), r.height());
      scaleRect = QRect(r.left() + barW, r.top(), scaleSize, r.height());
    }
  }

  // Edge margin: inset on both sides so extreme labels aren't clipped,
  // and the bar is aligned to the same coordinate system as the ticks.
  const i32 margin = _get_edge_margin();

  // Draw bar
  if (mBarVisible && barRect.height() > 0 && barRect.width() > 0)
  {
    const f64 range = mUpper - mLower;
    if (range > 0.0)
    {
      const f64 clampedValue = std::clamp(mValue, mLower, mUpper);
      const f64 fillFraction = (clampedValue - mLower) / range;

      if (mOrientation == Qt::Horizontal)
      {
        const i32 totalW = barRect.width() - 2 * margin;
        const i32 fillW  = static_cast<i32>(fillFraction * totalW);

        for (i32 x = 0; x < fillW; ++x)
        {
          const f64 rel = static_cast<f64>(x) / totalW;
          painter.setPen(_get_stops_color(rel));
          const i32 px = barRect.left() + margin + x;
          painter.drawLine(px, barRect.top(), px, barRect.bottom());
        }
      }
      else // Vertical — bottom = lower, top = upper
      {
        const i32 totalH = barRect.height() - 2 * margin;
        const i32 fillH  = static_cast<i32>(fillFraction * totalH);

        for (i32 y = 0; y < fillH; ++y)
        {
          const f64 rel = static_cast<f64>(y) / totalH;
          painter.setPen(_get_stops_color(rel));
          const i32 screenY = barRect.bottom() - margin - y;
          painter.drawLine(barRect.left(), screenY, barRect.right(), screenY);
        }
      }
    }
  }

  // Draw scale ticks and labels
  if (mShowScale && scaleSize > 0 && scaleRect.isValid())
  {
    painter.setFont(font());
    _draw_scale(painter, scaleRect, margin);
  }
}
