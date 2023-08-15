#include  "spectrum-scope.h"
#include  <QSettings>
#include  <QColor>
#include  <QPen>
#include  "color-selector.h"

SpectrumScope::SpectrumScope(QwtPlot * dabScope, int displaySize, QSettings * dabSettings) :
  spectrumCurve("")
{
  QString colorString = "black";
  bool brush;

  this->dabSettings = dabSettings;
  this->displaySize = displaySize;
  dabSettings->beginGroup("spectrumViewer");
//  colorString = dabSettings->value("displayColor", "black").toString();
//  displayColor = QColor(colorString);
  colorString = dabSettings->value("gridColor", "white").toString();
  mGridColor = QColor(colorString);
  colorString = dabSettings->value("curveColor", "white").toString();
  mCurveColor = QColor(colorString);
  brush = dabSettings->value("brush", 0).toInt() == 1;
  dabSettings->endGroup();
  plotgrid = dabScope;
  //plotgrid->setCanvasBackground(displayColor);
  grid = new QwtPlotGrid;
#if defined QWT_VERSION && ((QWT_VERSION >> 8) < 0x0601)
  grid	-> setMajPen (QPen(gridColor, 0, Qt::DotLine));
#else
  grid->setMajorPen(QPen(mGridColor, 0, Qt::DotLine));
#endif
  grid->enableXMin(true);
  grid->enableYMin(true);
#if defined QWT_VERSION && ((QWT_VERSION >> 8) < 0x0601)
  grid	-> setMinPen (QPen(gridColor, 0, Qt::DotLine));
#else
  grid->setMinorPen(QPen(mGridColor, 0, Qt::DotLine));
#endif
  grid->attach(plotgrid);

  lm_picker = new QwtPlotPicker(dabScope->canvas());
  QwtPickerMachine * lpickerMachine = new QwtPickerClickPointMachine();

  lm_picker->setStateMachine(lpickerMachine);
  lm_picker->setMousePattern(QwtPlotPicker::MouseSelect1, Qt::RightButton);
  connect(lm_picker, SIGNAL (selected(const QPointF&)), this, SLOT (rightMouseClick(const QPointF &)));

  spectrumCurve.setPen(QPen(mCurveColor, 2.0));
  spectrumCurve.setOrientation(Qt::Horizontal);
  spectrumCurve.setBaseline(get_db(0));

  if (brush)
  {
    QBrush ourBrush(mCurveColor);
    ourBrush.setStyle(Qt::Dense3Pattern);
    spectrumCurve.setBrush(ourBrush);
  }
  spectrumCurve.attach(plotgrid);

  Marker = new QwtPlotMarker();
  Marker->setLineStyle(QwtPlotMarker::VLine);
  Marker->setLinePen(QPen(Qt::red));
  Marker->attach(plotgrid);
  plotgrid->enableAxis(QwtPlot::yLeft);

  normalizer = 2048;
}

SpectrumScope::~SpectrumScope()
{
  delete Marker;
  delete grid;
}

void SpectrumScope::showSpectrum(const double * X_axis, double * Y_value, int amplification, int frequency)
{
  const float factor = (float)amplification / 100.0f; // amplification is between [1..100], so factor ]0..100]
  const float levelBottomdB = get_db(0); // eg. about -42 (dB)
  const float levelTopdB = (1.0f - factor) * levelBottomdB;

  plotgrid->setAxisScale(QwtPlot::xBottom, (double)X_axis[0], X_axis[displaySize - 1]);
  plotgrid->enableAxis(QwtPlot::xBottom);
  plotgrid->setAxisScale(QwtPlot::yLeft, levelBottomdB, levelTopdB);

  for (uint16_t i = 0; i < displaySize; i++)
  {
    Y_value[i] = get_db(factor * Y_value[i]);
  }

  spectrumCurve.setBaseline(levelBottomdB);
  spectrumCurve.setSamples(X_axis, Y_value, displaySize);
  Marker->setXValue(0);
  plotgrid->replot();
}

void SpectrumScope::rightMouseClick(const QPointF & point)
{
  (void)point;

  if (!ColorSelector::show_dialog(mGridColor, ColorSelector::GRIDCOLOR)) return;
  if (!ColorSelector::show_dialog(mCurveColor, ColorSelector::CURVECOLOR)) return;

  dabSettings->beginGroup("spectrumViewer");
  //dabSettings->setValue("displayColor", mDisplayColor.name());
  dabSettings->setValue("gridColor", mGridColor.name());
  dabSettings->setValue("curveColor", mCurveColor.name());
  dabSettings->endGroup();

  //this->displayColor = QColor(displayColor);
  //this->gridColor = QColor(gridColor);
  //this->curveColor = QColor(curveColor);
  spectrumCurve.setPen(QPen(mCurveColor, 2.0));
#if defined QWT_VERSION && ((QWT_VERSION >> 8) < 0x0601)
  grid->setMajPen(QPen(this->gridColor, 0, Qt::DotLine));
#else
  grid->setMajorPen(QPen(mGridColor, 0, Qt::DotLine));
#endif
  grid->enableXMin(true);
  grid->enableYMin(true);
#if defined QWT_VERSION && ((QWT_VERSION >> 8) < 0x0601)
  grid->setMinPen(QPen(this->gridColor, 0, Qt::DotLine));
#else
  grid->setMinorPen(QPen(mGridColor, 0, Qt::DotLine));
#endif
  //plotgrid->setCanvasBackground(this->displayColor);
}

float SpectrumScope::get_db(float x)
{
  return 20 * log10((x + 1) / (float)(normalizer));
}

void SpectrumScope::setBitDepth(int n)
{
  normalizer = n;
}

