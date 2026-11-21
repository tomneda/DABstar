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
#include <QFontMetricsF>
#include <QPainter>
#include <QPaintEvent>
#include <QRadialGradient>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <map>

static constexpr i32 cIconTextPixelSize = 16; // pixel size of the (not short) icon style text on a 32x32 button

IndicatorButton::IndicatorButton(QWidget* parent)
  : QPushButton(parent)
{
}

IndicatorButton::IndicatorButton(const QString& text, QWidget* parent)
  : QPushButton(text, parent)
{
}

IndicatorButton::IndicatorButton(const QIcon& icon, const QString& text, QWidget* parent)
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

void IndicatorButton::set_indicator_color(const QColor& iColor)
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

void IndicatorButton::set_icon_style_text(const bool iIconStyleText)
{
  if (mIconStyleText != iIconStyleText)
  {
    mIconStyleText = iIconStyleText;
    update();
  }
}

void IndicatorButton::set_sub_text(const QString& iSubText)
{
  if (mSubText != iSubText)
  {
    mSubText = iSubText;
    update();
  }
}

void IndicatorButton::set_activity_indicator_active(const bool iActive)
{
  if (mActivityIndicatorActive != iActive)
  {
    mActivityIndicatorActive = iActive;
    update();
  }
}

void IndicatorButton::trigger_activity_indicator(const i32 iDurationMs)
{
  if (mpActivityTimer == nullptr)
  {
    mpActivityTimer = new QTimer(this);
    mpActivityTimer->setSingleShot(true);
    connect(mpActivityTimer, &QTimer::timeout, this, [this]() { set_activity_indicator_active(false); });
  }

  set_activity_indicator_active(true);
  mpActivityTimer->start(iDurationMs); // a retrigger extends the active time
}

void IndicatorButton::set_activity_indicator_color(const QColor& iColor)
{
  if (mActivityIndicatorColor != iColor)
  {
    mActivityIndicatorColor = iColor;
    if (mActivityIndicatorActive)
    {
      update();
    }
  }
}

void IndicatorButton::set_common_icon_text_scale(const std::span<IndicatorButton* const>& iButtons)
{
  // find the widest text for each text length (number of letters), this determines the common horizontal scale of this length
  std::map<i32, std::pair<f32, QString>> widestTextPerLength; // text length -> (width, text)

  for (const IndicatorButton* const pButton : iButtons)
  {
    const QString text = pButton->text();
    const f32 textWidth = (f32)QFontMetricsF(pButton->_get_icon_text_font(cIconTextPixelSize)).horizontalAdvance(text);
    auto& [widestWidth, widestText] = widestTextPerLength[(i32)text.size()];

    if (textWidth > widestWidth)
    {
      widestWidth = textWidth;
      widestText = text;
    }
  }

  for (IndicatorButton* const pButton : iButtons)
  {
    pButton->mIconTextScaleRef = widestTextPerLength[(i32)pButton->text().size()].second;
    pButton->update();
  }
}

QFont IndicatorButton::_get_icon_text_font(const i32 iPixelSize) const
{
  QFont font = this->font();
  font.setPixelSize(iPixelSize);
  font.setBold(true);
  return font;
}

void IndicatorButton::_draw_fitted_text(QPainter& iPainter, const QString& iText, const i32 iPixelSize, const f32 iBaseline, const f32 iWidth,
                                        const QString& iScaleRef /*= {}*/) const
{
  const QFont font = _get_icon_text_font(iPixelSize);
  const QFontMetricsF fontMetrics(font);
  const f32 textWidth = (f32)fontMetrics.horizontalAdvance(iText);

  if (textWidth <= 0.0f)
  {
    return;
  }

  // like 'textLength' with 'lengthAdjust="spacingAndGlyphs"' in SVG: the text is scaled horizontally to the given width,
  // with a given reference text the scale is taken from this text (the text itself must still fit into the width)
  const f32 fitWidth = std::max(textWidth, iScaleRef.isEmpty() ? 0.0f : (f32)fontMetrics.horizontalAdvance(iScaleRef));
  const f32 scaleX = iWidth / fitWidth;

  // Each character is painted separately with its left edge snapped to a whole pixel. Otherwise the same letter
  // (e.g. an "I" with a stem of ~1.5 pixels) would be antialiased differently, depending on its sub-pixel position
  // within the text, and would look thinner or thicker on different buttons.
  const f32 posX0 = (f32)width() / 2.0f - textWidth * scaleX / 2.0f;

  iPainter.save();
  iPainter.setFont(font);

  for (i32 charIdx = 0; charIdx < iText.size(); ++charIdx)
  {
    const f32 charOffset = (f32)fontMetrics.horizontalAdvance(iText.left(charIdx)); // includes kerning to the previous characters
    const f32 posX = std::round(posX0 + charOffset * scaleX);

    iPainter.save();
    iPainter.translate(posX, iBaseline);
    iPainter.scale(scaleX, 1.0);
    iPainter.drawText(QPointF(0.0, 0.0), iText.mid(charIdx, 1));
    iPainter.restore();
  }

  iPainter.restore();
}

void IndicatorButton::_paint_icon_style_text()
{
  QStylePainter painter(this);
  QStyleOptionButton option;
  initStyleOption(&option);
  option.text.clear(); // the text is painted below
  painter.drawControl(QStyle::CE_PushButton, option);

  painter.setRenderHint(QPainter::TextAntialiasing);
  painter.setPen(option.palette.color(QPalette::ButtonText));

  // the sizes and positions refer to a 32x32 button (taken over from the former SVG text icons)
  if (mSubText.isEmpty())
  {
    const bool isShortText = (text().size() <= 2); // e.g. "EL"
    _draw_fitted_text(painter, text(), isShortText ? 20 : cIconTextPixelSize, isShortText ? 23.0f : 22.0f, 26.0f, mIconTextScaleRef);
  }
  else
  {
    _draw_fitted_text(painter, text(), 13, 14.0f, 24.0f);

    QFont font = this->font();
    font.setPixelSize(10);
    painter.setFont(font);
    painter.drawText(QRectF(0.0, 17.0, width(), 13.0), Qt::AlignHCenter | Qt::AlignVCenter, mSubText);
  }
}

void IndicatorButton::paintEvent(QPaintEvent* event)
{
  if (mIconStyleText)
  {
    _paint_icon_style_text();
  }
  else
  {
    QPushButton::paintEvent(event);
  }

  if (mIndicatorActive || mActivityIndicatorActive)
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (mIndicatorActive)
    {
      _paint_indicator(painter, (f32)width() - mIndicatorMarginRight - mIndicatorDiameter, mIndicatorColor);
    }

    if (mActivityIndicatorActive)
    {
      _paint_indicator(painter, mIndicatorMarginRight, mActivityIndicatorColor); // mirrored position on the left side
    }
  }
}

void IndicatorButton::_paint_indicator(QPainter& iPainter, const f32 iPosX, const QColor& iColor) const
{
  const qreal d = mIndicatorDiameter;
  const qreal x = iPosX;
  const qreal y = mIndicatorMarginTop;

  if (d > 0.0 && x >= 0.0 && y >= 0.0)
  {
    const QRectF dotRect(x, y, d, d);

    // Subtle radial gradient for LED jewel finish
    QRadialGradient gradient(QPointF(x + (d * 0.35), y + (d * 0.35)), d * 0.65);
    gradient.setColorAt(0.0, iColor.lighter(160));
    gradient.setColorAt(0.65, iColor);
    gradient.setColorAt(1.0, iColor.darker(140));

    iPainter.setPen(Qt::NoPen);
    iPainter.setBrush(gradient);
    iPainter.drawEllipse(dotRect);
  }
}
