#ifndef  SPECTRUM_SCOPE_H
#define  SPECTRUM_SCOPE_H

#include  "dab-constants.h"
#include  <QObject>
#include  <QColor>
#include  <qwt_plot_curve.h>

class QSettings;
class QwtPlot;
class QwtPlotPicker;
class QwtPlotGrid;

class SpectrumScope : public QObject
{
Q_OBJECT
public:
  SpectrumScope(QwtPlot *, i32, QSettings *);
  ~SpectrumScope() override;

  void show_spectrum(const f64 *, const f64 *, const SpecViewLimits<f64> & iSpecViewLimits);

private:
  static constexpr char SETTING_GROUP_NAME[] = "spectrumScope";

  QwtPlotCurve mSpectrumCurve;
  QSettings * const mpDabSettings;
  const i32 mDisplaySize;
  QwtPlotPicker * mpLmPicker;
  QColor mGridColor;
  QColor mCurveColor;
  f64 mScale = 0.0;

  QwtPlot * mpPlotgrid = nullptr;
  QwtPlotGrid * mpGrid = nullptr;
  std::vector<f64> mYValVec;
  
public slots:
  void slot_scaling_changed(int);

private slots:
  void slot_right_mouse_click(const QPointF &);
};

#endif

