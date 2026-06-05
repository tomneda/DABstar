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

QString get_bg_style_sheet(const QColor & iBgBaseColor, const QColor & iFgBaseColor /*= "black"*/)
{
  constexpr f32 fac = 0.6f;
  const i32 r1 = iBgBaseColor.red();
  const i32 g1 = iBgBaseColor.green();
  const i32 b1 = iBgBaseColor.blue();
  const i32 r2 = (i32)((f32)r1 * fac);
  const i32 g2 = (i32)((f32)g1 * fac);
  const i32 b2 = (i32)((f32)b1 * fac);
  assert(r2 >= 0);
  assert(g2 >= 0);
  assert(b2 >= 0);
  const QColor BgBaseColor2(r2, g2, b2);

  // Disabled: blend the button color 35% toward a dark neutral so the
  // hue identity is still faintly visible but the button clearly looks inactive.
  constexpr f32 blend = 0.35f;
  constexpr i32 gray  = 0x40;
  const i32 rd1 = (i32)(r1 * blend + gray * (1.0f - blend));
  const i32 gd1 = (i32)(g1 * blend + gray * (1.0f - blend));
  const i32 bd1 = (i32)(b1 * blend + gray * (1.0f - blend));
  const QColor BgDisabled1(rd1, gd1, bd1);
  const QColor BgDisabled2((i32)((f32)rd1 * fac), (i32)((f32)gd1 * fac), (i32)((f32)bd1 * fac));

  std::stringstream ts; // QTextStream did not work well here?!
  ts << "QWidget { background-color: qlineargradient(x1:1, y1:0, x2:1, y2:1, stop:0 " << iBgBaseColor.name().toStdString()
     << ", stop:1 " << BgBaseColor2.name().toStdString() << "); color: " << iFgBaseColor.name().toStdString() << "; } "
     << "QWidget:disabled { background-color: qlineargradient(x1:1, y1:0, x2:1, y2:1, stop:0 " << BgDisabled1.name().toStdString()
     << ", stop:1 " << BgDisabled2.name().toStdString() << "); color: #707070; } "
     << "QToolTip { color: black; background-color: #ffffdc; border: 1px solid black; }"; // add original tooltip color again (works unfortunately only here, not global)
  // qDebug() <<  "*** Style sheet:" << ts.str().c_str();

  return { ts.str().c_str() };
}

QString get_combo_style_sheet(const QColor & iBgBaseColor, const QColor & iFgBaseColor /*= Qt::white*/)
{
  constexpr f32 fac = 0.6f;
  const i32 r1 = iBgBaseColor.red();
  const i32 g1 = iBgBaseColor.green();
  const i32 b1 = iBgBaseColor.blue();
  const i32 r2 = (i32)((f32)r1 * fac);
  const i32 g2 = (i32)((f32)g1 * fac);
  const i32 b2 = (i32)((f32)b1 * fac);
  const QColor BgBaseColor2(r2, g2, b2);

  constexpr f32 blend = 0.35f;
  constexpr i32 gray  = 0x40;
  const i32 rd1 = (i32)(r1 * blend + gray * (1.0f - blend));
  const i32 gd1 = (i32)(g1 * blend + gray * (1.0f - blend));
  const i32 bd1 = (i32)(b1 * blend + gray * (1.0f - blend));
  const QColor BgDisabled1(rd1, gd1, bd1);
  const QColor BgDisabled2((i32)((f32)rd1 * fac), (i32)((f32)gd1 * fac), (i32)((f32)bd1 * fac));

  // Selection highlight: brighten the base color slightly for the item view
  const i32 rs = std::min(255, (i32)(r1 * 1.3f));
  const i32 gs = std::min(255, (i32)(g1 * 1.3f));
  const i32 bs = std::min(255, (i32)(b1 * 1.3f));
  const QColor BgSelection(rs, gs, bs);

  const std::string grad    = "qlineargradient(x1:1, y1:0, x2:1, y2:1, stop:0 " + iBgBaseColor.name().toStdString() + ", stop:1 " + BgBaseColor2.name().toStdString() + ")";
  const std::string gradDis = "qlineargradient(x1:1, y1:0, x2:1, y2:1, stop:0 " + BgDisabled1.name().toStdString() + ", stop:1 " + BgDisabled2.name().toStdString() + ")";
  const std::string fg      = iFgBaseColor.name().toStdString();

  std::stringstream ts;
  ts << "QComboBox { background-color: " << grad << "; color: " << fg << "; border: 1px solid #555; border-radius: 3px; padding: 1px 4px; } "
     << "QComboBox:disabled { background-color: " << gradDis << "; color: #707070; } "
     << "QComboBox::drop-down { border: none; background: transparent; } "
     << "QComboBox QAbstractItemView { background-color: #2a2a2a; color: #e0e0e0; "
     <<   "selection-background-color: " << BgSelection.name().toStdString() << "; selection-color: " << fg << "; "
     <<   "border: 1px solid #555; } "
     << "QToolTip { color: black; background-color: #ffffdc; border: 1px solid black; }";

  return { ts.str().c_str() };
}
