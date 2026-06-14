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

#include <QObject>
#include <QWidget>
#include <QImage>
#include <array>
#include <vector>
#include "dab-constants.h"

// It is a plain QWidget (promoted in Qt Designer) showing a scrolling
// waterfall image. Call init() after setupUi() to provide the size parameters.

class WaterfallScope : public QWidget
{
Q_OBJECT
public:
  explicit WaterfallScope(QWidget * parent = nullptr);
  ~WaterfallScope() override = default;

  void init(i32 iDisplaySize, i32 iRasterSize);
  void show_waterfall(const f32 * ipY1_value, const SpecViewLimits<f32> & iSpecViewLimits);

public slots:
  void slot_scaling_changed(i32 iScale);
  void slot_set_horizontal_margins(int iLeft, int iRight);

protected:
  void paintEvent(QPaintEvent * ipEvent) override;

private:
  i32 mDisplaySize  = 0;
  i32 mRasterSize   = 0;
  f32 mScale        = 0.0f;
  int mLeftMargin   = 0;
  int mRightMargin  = 0;

  QImage mImage;
  std::array<QRgb, 256> mColorLut;

  void _build_color_lut();
};
