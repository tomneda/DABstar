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
#include "gui-helpers.h"
#include "glob_data_types.h"
#include <sstream>
#include <cassert>
// #include <QDebug>


static void _calc_adjusted_bg_colors(QColor & oBgBaseColor1, QColor & oBgBaseColor2, const QColor & iBgBaseColor, const f32 iRefLuminance)
{
  constexpr f32 cGradientFactor = 0.69f;

  static_assert(cGradientFactor >= 0.0f && cGradientFactor <= 1.0f, "Gradient factor must be between 0 and 1");

  f32 r1 = iBgBaseColor.red();
  f32 g1 = iBgBaseColor.green();
  f32 b1 = iBgBaseColor.blue();
  // const f32 luminance = (r1 * 0.299f + g1 * 0.587f + b1 * 0.114f) / 255.0f; // BT.601
  const f32 luminance = (r1 * 0.2126f + g1 * 0.7152f + b1 * 0.0722f) / 255.0f; // BT.709
  const f32 corrFac = iRefLuminance / luminance;

  r1 = r1 * corrFac;
  g1 = g1 * corrFac;
  b1 = b1 * corrFac;

  // is any value > 255?
  f32 corrClip = 1.0f;
  corrClip = std::min(255.0f / (f32)r1, corrClip);
  corrClip = std::min(255.0f / (f32)g1, corrClip);
  corrClip = std::min(255.0f / (f32)b1, corrClip);

  // qDebug() << "luminance=" << luminance << corrFac << corrClip;

  if (corrClip < 1.0f)
  {
    r1 = r1 * corrClip;
    g1 = g1 * corrClip;
    b1 = b1 * corrClip;
    // qDebug() << "Clamped color components" << r1 << g1 << b1 << corrClip;
  }
  oBgBaseColor1 = QColor(r1, g1, b1);

  const i32 r2 = (i32)(r1 * cGradientFactor);
  const i32 g2 = (i32)(g1 * cGradientFactor);
  const i32 b2 = (i32)(b1 * cGradientFactor);
  oBgBaseColor2 = QColor(r2, g2, b2);
}

static void _calc_blended_bg_colors(QColor & oBgDisabled1, QColor & oBgDisabled2, const QColor & iBgBaseColor1, const QColor & iBgBaseColor2)
{
  // Disabled: blend the button color 35% toward a dark neutral so the
  // hue identity is still faintly visible but the button clearly looks inactive.
  constexpr f32 cBlendFactor = 0.35f;
  constexpr i32 cBaseGrayValue  = 0x40;

  auto weight_color = [](const i32 iColValue) ->i32
  {
    return (i32)(iColValue * cBlendFactor + cBaseGrayValue * (1.0f - cBlendFactor));
  };

  const i32 rd1 = weight_color(iBgBaseColor1.red());
  const i32 gd1 = weight_color(iBgBaseColor1.green());
  const i32 bd1 = weight_color(iBgBaseColor1.blue());
  const i32 rd2 = weight_color(iBgBaseColor2.red());
  const i32 gd2 = weight_color(iBgBaseColor2.green());
  const i32 bd2 = weight_color(iBgBaseColor2.blue());

  oBgDisabled1 = QColor(rd1, gd1, bd1);
  oBgDisabled2 = QColor(rd2, gd2, bd2);
}

QString get_bg_style_sheet(const QColor & iBgBaseColor, const QColor & iFgBaseColor /*= "black"*/, const f32 iRefLuminance /*= 0.60f*/)
{
  QColor bgBaseColor1, bgBaseColor2, bgDisabled1, bgDisabled2;

  _calc_adjusted_bg_colors(bgBaseColor1, bgBaseColor2, iBgBaseColor, iRefLuminance);
  _calc_blended_bg_colors(bgDisabled1, bgDisabled2, bgBaseColor1, bgBaseColor2);

  const std::string grad    = "qlineargradient(x1:1, y1:0, x2:1, y2:1, stop:0 " + bgBaseColor1.name().toStdString() + ", stop:1 " + bgBaseColor2.name().toStdString() + ")";
  const std::string gradDis = "qlineargradient(x1:1, y1:0, x2:1, y2:1, stop:0 " + bgDisabled1.name().toStdString() + ", stop:1 " + bgDisabled2.name().toStdString() + ")";

  std::stringstream ts; // QTextStream did not work well here?!
  ts << "QWidget { background-color: " << grad << "; color: " << iFgBaseColor.name().toStdString() << "; } "
     << "QWidget:disabled { background-color: " << gradDis << "; color: #707070; } "
     << "QToolTip { color: black; background-color: #ffffdc; border: 1px solid black; }"; // add original tooltip color again (works unfortunately only here, not global)
  // qDebug() <<  "*** Style sheet:" << ts.str().c_str();

  return { ts.str().c_str() };
}

QString get_combo_style_sheet(const QColor & iBgBaseColor, const QColor & iFgBaseColor /*= Qt::white*/, f32 iRefLuminance /*= 0.60f*/)
{
  QColor bgBaseColor1, bgBaseColor2, bgDisabled1, bgDisabled2;

  _calc_adjusted_bg_colors(bgBaseColor1, bgBaseColor2, iBgBaseColor, iRefLuminance);
  _calc_blended_bg_colors(bgDisabled1, bgDisabled2, bgBaseColor1, bgBaseColor2);

  // Selection highlight: brighten the base color slightly for the item view
  constexpr f32 cSelectionFactor = 1.3f;
  const i32 rs = std::min(255, (i32)(bgBaseColor1.red()   * cSelectionFactor));
  const i32 gs = std::min(255, (i32)(bgBaseColor1.green() * cSelectionFactor));
  const i32 bs = std::min(255, (i32)(bgBaseColor1.blue()  * cSelectionFactor));
  const QColor bgSelection(rs, gs, bs);

  const std::string grad    = "qlineargradient(x1:1, y1:0, x2:1, y2:1, stop:0 " + bgBaseColor1.name().toStdString() + ", stop:1 " + bgBaseColor2.name().toStdString() + ")";
  const std::string gradDis = "qlineargradient(x1:1, y1:0, x2:1, y2:1, stop:0 " + bgDisabled1.name().toStdString() + ", stop:1 " + bgDisabled2.name().toStdString() + ")";
  const std::string fg      = iFgBaseColor.name().toStdString();

  std::stringstream ts;
  ts << "QComboBox { background-color: " << grad << "; color: " << fg << "; border: 1px solid #555; border-radius: 3px; padding: 1px 4px; } "
     << "QComboBox:disabled { background-color: " << gradDis << "; color: #707070; } "
     << "QComboBox::drop-down { border: none; background: transparent; } "
     << "QComboBox QAbstractItemView { background-color: #2a2a2a; color: #e0e0e0; "
     <<   "selection-background-color: " << bgSelection.name().toStdString() << "; selection-color: " << fg << "; "
     <<   "border: 1px solid #555; } "
     << "QToolTip { color: black; background-color: #ffffdc; border: 1px solid black; }";

  return { ts.str().c_str() };
}
