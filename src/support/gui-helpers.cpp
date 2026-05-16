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

  std::stringstream ts; // QTextStream did not work well here?!
  ts << "QWidget { background-color: qlineargradient(x1:1, y1:0, x2:1, y2:1, stop:0 " << iBgBaseColor.name().toStdString()
     << ", stop:1 " << BgBaseColor2.name().toStdString() << "); color: " << iFgBaseColor.name().toStdString() << "; } "
     << "QToolTip { color: black; background-color: #ffffdc; border: 1px solid black; }"; // add original tooltip color again (works unfortunately only here, not global)
  // qDebug() <<  "*** Style sheet:" << ts.str().c_str();

  return { ts.str().c_str() };
}
