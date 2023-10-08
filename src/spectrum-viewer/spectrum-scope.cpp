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

  dabSettings->beginGroup("spectrumViewer");
  colorString = dabSettings->value("gridColor", "white").toString();
  mGridColor = QColor(colorString);
  colorString = dabSettings->value("curveColor", "white").toString();
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

  connect(mpLmPicker, qOverload<const QPointF &>(&QwtPlotPicker::selected), this, &SpectrumScope::rightMouseClick);

  mSpectrumCurve.setPen(QPen(mCurveColor, 2.0));
  mSpectrumCurve.setOrientation(Qt::Horizontal);
  mSpectrumCurve.setBaseline(get_db(0));

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

void SpectrumScope::show_spectrum(const double * X_axis, const double * Y_value, int32_t amplification)
{
  const double factor = (double)amplification / 100.0; // amplification is between [1..100], so factor ]0..100]
  const double levelBottomdB = get_db(0.0); // eg. about -42 (dB)
  const double levelTopdB = (1.0 - factor) * levelBottomdB;

  mpPlotgrid->setAxisScale(QwtPlot::xBottom, (double)X_axis[0], X_axis[mDisplaySize - 1]);
  mpPlotgrid->enableAxis(QwtPlot::xBottom);
  mpPlotgrid->setAxisScale(QwtPlot::yLeft, levelBottomdB, levelTopdB);

  for (uint16_t i = 0; i < mDisplaySize; i++)
  {
    mYValVec[i] = get_db(factor * Y_value[i]);
  }

  mSpectrumCurve.setBaseline(levelBottomdB);
  mSpectrumCurve.setSamples(X_axis, mYValVec.data(), mDisplaySize);
  mpPlotgrid->replot();
}

void SpectrumScope::rightMouseClick(const QPointF & point)
{
  (void)point;

  if (!ColorSelector::show_dialog(mGridColor, ColorSelector::GRIDCOLOR)) return;
  if (!ColorSelector::show_dialog(mCurveColor, ColorSelector::CURVECOLOR)) return;

  mpDabSettings->beginGroup("spectrumViewer");
  mpDabSettings->setValue("gridColor", mGridColor.name());
  mpDabSettings->setValue("curveColor", mCurveColor.name());
  mpDabSettings->endGroup();

  mSpectrumCurve.setPen(QPen(mCurveColor, 2.0));
  mpGrid->setMinorPen(QPen(mGridColor, 0, Qt::DotLine));
  mpGrid->setMajorPen(QPen(mGridColor, 0, Qt::DotLine));
  mpGrid->enableXMin(true);
  mpGrid->enableYMin(true);
}

void SpectrumScope::set_bit_depth(int32_t n)
{
  mNormalizer = n;
}

