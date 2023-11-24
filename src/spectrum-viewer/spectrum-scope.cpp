#include  "spectrum-scope.h"
#include  <QSettings>
#include  <QColor>
#include  <QPen>
#include  "color-selector.h"

SpectrumScope::SpectrumScope(QwtPlot * dabScope, int32_t displaySize, QSettings * dabSettings) :
  mSpectrumCurve(""),
  mpDabSettings(dabSettings),
  mDisplaySize(displaySize)
{
  mYValVec.resize(mDisplaySize);
  std::fill(mYValVec.begin(), mYValVec.end(), 0.0);

  QString colorString = "black";
  bool brush;

  dabSettings->beginGroup(SETTING_GROUP_NAME);
  colorString = dabSettings->value("gridColor", "#5e5c64").toString();
  mGridColor = QColor(colorString);
  colorString = dabSettings->value("curveColor", "#dc8add").toString();
  mCurveColor = QColor(colorString);
  brush = dabSettings->value("brush", 0).toInt() == 1;
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
  //mSpectrumCurve.setBaseline(get_db(0));

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

void SpectrumScope::show_spectrum(const double * X_axis, const double * Y_value)
{
  mpPlotgrid->setAxisScale(QwtPlot::xBottom, (double)X_axis[0], X_axis[mDisplaySize - 1]);
  mpPlotgrid->enableAxis(QwtPlot::xBottom);

  static double valdBMinMean = -90.0;
  static double valdBMaxMean = -35.0;

  double valdBMin =  1000.0;
  double valdBMax = -1000.0;

  for (uint16_t i = 0; i < mDisplaySize; i++)
  {
    const double val = 20.0 * std::log10(Y_value[i] + 1.0e-6f);
    if (val > valdBMax) valdBMax = val;
    if (val < valdBMin) valdBMin = val;
    mYValVec[i] = val;
  }

  mean_filter(valdBMaxMean, valdBMax, 0.1);
  mean_filter(valdBMinMean, valdBMin, 0.1);

  // avoid scaling jumps if <= -100 occurs in the display
  if (valdBMinMean < -99.0)
  {
    valdBMinMean = -99.0;
  }

  mpPlotgrid->setAxisScale(QwtPlot::yLeft, valdBMinMean, valdBMaxMean + mTopOffs);
  //mSpectrumCurve.setBaseline(valdBMinMean);
  mSpectrumCurve.setSamples(X_axis, mYValVec.data(), mDisplaySize);
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

void SpectrumScope::set_bit_depth(int32_t d)
{
  mBitDepth = d;
  mNormalizer = get_range_from_bit_depth(d);
}

void SpectrumScope::slot_scaling_changed(int iScale)
{
  mTopOffs = 0.6 * (50 - iScale) + 5.0;
}

