//
//	Simple viewer for correlation of a whole frame
//
#ifndef  CIR_VIEWER_H
#define  CIR_VIEWER_H

#include <QFrame>
#include <QObject>
#include <qwt.h>
#include <qwt_plot.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_curve.h>
#include "ui_cir-widget.h"
#include "ringbuffer.h"
#include "phasetable.h"
#include <fftw3.h>

constexpr int32_t CIR_SPECTRUMSIZE = 2048*97;

class QSettings;
class QLabel;
class RadioInterface;

class CirViewer : public QObject, private Ui_cirWidget, private PhaseTable
{
Q_OBJECT
public:
  CirViewer(RingBuffer<cmplx> * iCirBuffer);
  ~CirViewer();
  void show_cir();
  void show();
  void hide();
  bool is_hidden();

private:
  QFrame mFrame;
  RingBuffer<cmplx> * const mpCirBuffer;
  QwtPlotCurve mCurve;
  QwtPlotGrid mGrid;
  alignas(64) TArrayTu mFftInBuffer;
  alignas(64) TArrayTu mFftOutBuffer;
  fftwf_plan mFftPlanFwd;
  fftwf_plan mFftPlanBwd;
};

#endif
