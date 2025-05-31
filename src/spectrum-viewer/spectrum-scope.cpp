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

SpectrumScope::SpectrumScope(QwtPlot * dabScope, i32 displaySize, QSettings * dabSettings) :
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
  mpGrid->enableYMin(false);
  mpGrid->attach(mpPlotgrid);

  mpLmPicker = new QwtPlotPicker(dabScope->canvas());
  QwtPickerMachine * lpickerMachine = new QwtPickerClickPointMachine();

  mpLmPicker->setStateMachine(lpickerMachine);
  mpLmPicker->setMousePattern(QwtPlotPicker::MouseSelect1, Qt::RightButton);

#ifdef _WIN32
  // It is strange, the non-macro based variant seems not working on windows, so use the macro-base version here.
  connect(mpLmPicker, SIGNAL(selected(const QPointF&)), this, SLOT(slot_right_mouse_click(const QPointF &)));
#else
  // The non macro-based variant is type-secure so it should be preferred.
  // Clang-glazy mentioned that QwtPlotPicker::selected would be no signal, but it is?!
  connect(mpLmPicker, qOverload<const QPointF &>(&QwtPlotPicker::selected), this, &SpectrumScope::slot_right_mouse_click);
#endif

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

void SpectrumScope::slot_scaling_changed(i32 iScale)
{
  mScale = (f64)iScale / 100.0;
}

