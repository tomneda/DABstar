/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original file originated from Qt-DAB but carried no copyright notice.
 */

#include  "spectrum-scope.h"
#include  "plot_widget.h"
#include  <QSettings>
#include  <QColor>
#include  <QPen>
#include  <QChart>

SpectrumScope::SpectrumScope(PlotWidget * pPlot, i32 displaySize, QSettings * /*dabSettings*/) :
  mDisplaySize(displaySize)
{
  mYValVec.resize(mDisplaySize);
  std::fill(mYValVec.begin(), mYValVec.end(), 0.0f);
  mpPlot = pPlot;

  mpCurve = new QLineSeries();
  mpCurve->setPen(QPen(QColor(0xdc8add), 2.0));
  mpCurve->setUseOpenGL(false);
  mpPlot->chart()->addSeries(mpCurve);
  mpCurve->attachAxis(mpPlot->get_x_axis());
  mpCurve->attachAxis(mpPlot->get_y_axis());

  mpPlot->get_x_axis()->setGridLineVisible(true);
  mpPlot->get_x_axis()->setMinorGridLineVisible(true);
  mpPlot->get_y_axis()->setGridLineVisible(true);
  mpPlot->get_y_axis()->setMinorGridLineVisible(false);
}

void SpectrumScope::show_spectrum(const f32 * X_axis, const f32 * Y_value, const SpecViewLimits<f32> & iSpecViewLimits)
{
  mpPlot->set_x_range(X_axis[0], X_axis[mDisplaySize - 1]);
  mpPlot->set_x_tick_dynamic(X_axis[mDisplaySize / 2], 512.0); // FFT "DC" frequency point
  // weight with slider (scale) between global and local minimum and maximum level values
  const f32 yMax = (iSpecViewLimits.Glob.Max + 5.0f) * (1.0f - mScale) + iSpecViewLimits.Loc.Max * mScale;
  const f32 yMin = iSpecViewLimits.Glob.Min * (1.0f - mScale) + iSpecViewLimits.Loc.Min * mScale;

  mpPlot->set_y_range(yMin, yMax);

  mPoints.clear();
  mPoints.reserve(mDisplaySize);
  for (i32 i = 0; i < mDisplaySize; i++)
  {
    mPoints.append(QPointF(X_axis[i], Y_value[i]));
  }
  mpCurve->replace(mPoints);
}

void SpectrumScope::slot_scaling_changed(i32 iScale)
{
  mScale = (f32)iScale / 100.0f;
}
