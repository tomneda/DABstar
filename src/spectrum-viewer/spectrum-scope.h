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
  SpectrumScope(QwtPlot *, int32_t, QSettings *);
  ~SpectrumScope() override;

  void show_spectrum(const double *, const double *, const SpecViewLimits<double> & iSpecViewLimits);

private:
  static constexpr char SETTING_GROUP_NAME[] = "spectrumScope";

  QwtPlotCurve mSpectrumCurve;
  QSettings * const mpDabSettings;
  const int32_t mDisplaySize;
  QwtPlotPicker * mpLmPicker;
  QColor mGridColor;
  QColor mCurveColor;
  double mScale = 0.0;

  QwtPlot * mpPlotgrid = nullptr;
  QwtPlotGrid * mpGrid = nullptr;
  std::vector<double> mYValVec;
  
public slots:
  void slot_scaling_changed(int);

private slots:
  void slot_right_mouse_click(const QPointF &);
};

#endif

