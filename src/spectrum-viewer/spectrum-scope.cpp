#include  "spectrum-scope.h"
#include  <QSettings>
#include  <QColor>
#include  <QPen>
#include  <qwt.h>
#include  <qwt_plot.h>
#include  <qwt_plot_grid.h>

SpectrumScope::SpectrumScope(QwtPlot * dabScope, i32 displaySize, QSettings * dabSettings) :
  mSpectrumCurve(""),
  mpDabSettings(dabSettings),
  mDisplaySize(displaySize)
{
  mYValVec.resize(mDisplaySize);
  std::fill(mYValVec.begin(), mYValVec.end(), 0.0);
  dabSettings->beginGroup(SETTING_GROUP_NAME);
  dabSettings->endGroup();
  mpPlotgrid = dabScope;

  mpGrid = new QwtPlotGrid;
  mpGrid->setMinorPen(QPen(QColor(0x5e5c64), 0, Qt::DotLine));
  mpGrid->setMajorPen(QPen(QColor(0x5e5c64), 0, Qt::DotLine));
  mpGrid->enableXMin(true);
  mpGrid->enableYMin(false);
  mpGrid->attach(mpPlotgrid);

  mSpectrumCurve.setPen(QPen(QColor(0xdc8add), 2.0));
  mSpectrumCurve.setOrientation(Qt::Horizontal);
  mSpectrumCurve.attach(mpPlotgrid);
  mpPlotgrid->enableAxis(QwtPlot::yLeft);
}

SpectrumScope::~SpectrumScope()
{
  delete mpGrid;
}

void SpectrumScope::show_spectrum(const f64 * X_axis, const f64 * Y_value, const SpecViewLimits<f64> & iSpecViewLimits)
{
  mpPlotgrid->setAxisScale(QwtPlot::xBottom, (f64)X_axis[0], X_axis[mDisplaySize - 1]);
  mpPlotgrid->enableAxis(QwtPlot::xBottom);

  // weight with slider (scale) between global and local minimum and maximum level values
  const f64 yMax = (iSpecViewLimits.Glob.Max + 5.0) * (1.0 - mScale) + iSpecViewLimits.Loc.Max * mScale;
  const f64 yMin = iSpecViewLimits.Glob.Min * (1.0 - mScale) + iSpecViewLimits.Loc.Min * mScale;

  mpPlotgrid->setAxisScale(QwtPlot::yLeft, yMin, yMax);
  mSpectrumCurve.setSamples(X_axis, Y_value, mDisplaySize);
  mpPlotgrid->replot();
}

void SpectrumScope::slot_scaling_changed(i32 iScale)
{
  mScale = (f64)iScale / 100.0;
}

