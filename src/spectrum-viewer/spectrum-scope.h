#ifndef  SPECTRUM_SCOPE_H
#define  SPECTRUM_SCOPE_H

#include  "dab-constants.h"
#include  <QObject>
#include  <qwt.h>
#include  <qwt_plot.h>
#include  <qwt_plot_marker.h>
#include  <qwt_plot_grid.h>
#include  <qwt_plot_curve.h>
#include  <qwt_color_map.h>
#include  <qwt_plot_zoomer.h>
#include  <qwt_plot_textlabel.h>
#include  <qwt_plot_panner.h>
#include  <qwt_plot_layout.h>
#include  <qwt_picker_machine.h>
#include  <qwt_scale_widget.h>
#include  <QBrush>
#include  <QTimer>

class RadioInterface;
class QSettings;

class SpectrumScope : public QObject
{
Q_OBJECT
public:
  SpectrumScope(QwtPlot *, int32_t, QSettings *);
  ~SpectrumScope();

  void show_spectrum(const double *, const double *, int32_t);
  void set_bit_depth(int32_t);

private:
  static constexpr char SETTING_GROUP_NAME[] = "spectrumScope";

  QwtPlotCurve mSpectrumCurve;
  QSettings * const mpDabSettings;
  const int32_t mDisplaySize;
  QwtPlotPicker * mpLmPicker;
  QColor mGridColor;
  QColor mCurveColor;
  int32_t mNormalizer = 2048;

  QwtPlot * mpPlotgrid;
  QwtPlotGrid * mpGrid;
  std::vector<double> mYValVec;

  template<typename T> inline T get_db(T x) { return 20 * std::log10((x + 1) / (T)(mNormalizer)); }

private slots:
  void rightMouseClick(const QPointF &);
};

#endif

