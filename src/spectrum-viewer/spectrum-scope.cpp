#include  "spectrum-scope.h"
#include  <QSettings>
#include  <QColor>
#include  <QPen>
#include  <qwt.h>
#include  <qwt_plot.h>
#include  <qwt_plot_grid.h>
#include  <qwt_color_map.h>
#include  <qwt_plot_zoomer.h>
#include  <qwt_picker_machine.h>
#include  "color-selector.h"

SpectrumScope::SpectrumScope(QwtPlot * dabScope, int32_t displaySize, QSettings * dabSettings) :
  mSpectrumCurve(""),
  mpDabSettings(dabSettings),
  mDisplaySize(displaySize)
{
  mYValVec.resize(mDisplaySize);
  std::fill(mYValVec.begin(), mYValVec.end(), 0.0);
  dabSettings->beginGroup(SETTING_GROUP_NAME);
  QString colorString = dabSettings->value("gridColor", "#5e5c64").toString();
  mGridColor = QColor(colorString);
  colorString = dabSettings->value("curveColor", "#dc8add").toString();
  mCurveColor = QColor(colorString);
  const bool brush = dabSettings->value("brush", 0).toInt() == 1;
  dabSettings->endGroup();
  mpPlotgrid = dabScope;

  mpGrid = new QwtPlotGrid;
  mpGrid->setMinorPen(QPen(mGridColor, 0, Qt::DotLine));
  mpGrid->setMajorPen(QPen(mGridColor, 0, Qt::DotLine));
  mpGrid->enableXMin(true);
  mpGrid->enableYMin(true);
  mpGrid->attach(mpPlotgrid);

  mpLmPicker = new QwtPlotPicker(dabScope->canvas());
  QwtPickerMachine * lpickerMachine = new QwtPickerClickPointMachine();

  mpLmPicker->setStateMachine(lpickerMachine);
  mpLmPicker->setMousePattern(QwtPlotPicker::MouseSelect1, Qt::RightButton);

  connect(mpLmPicker, qOverload<const QPointF &>(&QwtPlotPicker::selected), this, &SpectrumScope::slot_right_mouse_click);

  mSpectrumCurve.setPen(QPen(mCurveColor, 2.0));
  mSpectrumCurve.setOrientation(Qt::Horizontal);

  if (brush)
  {
    QBrush ourBrush(mCurveColor);
    ourBrush.setStyle(Qt::Dense3Pattern);
    mSpectrumCurve.setBrush(ourBrush);
  }

  mSpectrumCurve.attach(mpPlotgrid);
  mpPlotgrid->enableAxis(QwtPlot::yLeft);
}

SpectrumScope::~SpectrumScope()
{
  delete mpGrid;
}

void SpectrumScope::show_spectrum(const double * X_axis, const double * Y_value, const SpecViewLimits<double> & iSpecViewLimits)
{
  mpPlotgrid->setAxisScale(QwtPlot::xBottom, (double)X_axis[0], X_axis[mDisplaySize - 1]);
  mpPlotgrid->enableAxis(QwtPlot::xBottom);

  // weight with slider (scale) between global and local minimum and maximum level values
  const double yMax = (iSpecViewLimits.GlobMax + 5.0) * (1.0 - mScale) + iSpecViewLimits.LocMax * mScale;
  const double yMin = iSpecViewLimits.GlobMin * (1.0 - mScale) + iSpecViewLimits.LocMin * mScale;

  mpPlotgrid->setAxisScale(QwtPlot::yLeft, yMin, yMax);
  mSpectrumCurve.setSamples(X_axis, Y_value, mDisplaySize);
  mpPlotgrid->replot();
}

void SpectrumScope::slot_right_mouse_click(const QPointF & point)
{
  (void)point;

  if (!ColorSelector::show_dialog(mGridColor, ColorSelector::GRIDCOLOR)) return;
  if (!ColorSelector::show_dialog(mCurveColor, ColorSelector::CURVECOLOR)) return;

  mpDabSettings->beginGroup(SETTING_GROUP_NAME);
  mpDabSettings->setValue("gridColor", mGridColor.name());
  mpDabSettings->setValue("curveColor", mCurveColor.name());
  mpDabSettings->endGroup();

  mSpectrumCurve.setPen(QPen(mCurveColor, 2.0));
  mpGrid->setMinorPen(QPen(mGridColor, 0, Qt::DotLine));
  mpGrid->setMajorPen(QPen(mGridColor, 0, Qt::DotLine));
  mpGrid->enableXMin(true);
  mpGrid->enableYMin(true);
}

void SpectrumScope::slot_scaling_changed(int iScale)
{
  mScale = (double)iScale / 100.0;
}

