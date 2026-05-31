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

#include "glob_enums.h"
#include "dab-constants.h"
#include <vector>
#include <array>
#include <QWidget>
#include <QImage>

// It is a plain QWidget (promoted in Qt Designer) that renders the IQ
// constellation directly using QPainter + a pre-built QImage pixel buffer.

class IQDisplay : public QWidget
{
Q_OBJECT
public:
  explicit IQDisplay(QWidget * parent = nullptr);
  ~IQDisplay() override = default;

  struct SCustPlot
  {
    EIqPlotType PlotType;
    const char * Name;
    const char * ToolTip;
  };

  void display_iq(const std::vector<cf32> & iIQ, f32 iScale);
  void select_plot_type(EIqPlotType iPlotType);
  void set_map_1st_quad(bool iMap1stQuad);
  static QStringList get_plot_type_names();

protected:
  void paintEvent(QPaintEvent * ipEvent) override;

private:
  static constexpr i32 cRadius  = 100;
  static constexpr i32 c2Radius = 2 * cRadius;  // image width and height

  f32 mLastCircleSize = 0;
  bool mMap1stQuad = false;
  f32 mRadius = (f32)cRadius;

  std::vector<std::complex<i32>> mPoints;
  std::vector<u8> mPixelBuffer;       // cDim×cDim uint8 values (0=bg, 20=cross/circle, 100=dot)
  QImage mImage;                      // rendered image, rebuilt on each display_iq() call
  QRgb mDotColor{};                   // IQ dot color (value 100): yellowish-white
  std::array<QRgb, c2Radius> mBgGradient;    // per-row background gradient (top→bottom)
  std::array<QRgb, c2Radius> mCrossGradient; // per-row cross/circle color

  void _set_point(i32 iX, i32 iY, u8 iVal);
  void _clean_screen_from_old_data_points();
  void _draw_cross();
  void _draw_circle(f32 iScale, u8 iVal);
  void _repaint_circle(f32 iSize);
  void _rebuild_image();
  static SCustPlot _get_plot_type_data(EIqPlotType iPlotType);
};
