//
//  Simple viewer for correlation of a whole frame
//
#pragma once

#include "ui_cir_widget.h"
#include "ringbuffer.h"
#include "phasetable.h"
#include "plot_widget.h"
#include <fftw3.h>
#include <QFrame>
#include <QLineSeries>
#include <QObject>

constexpr i32 CIR_SPECTRUMSIZE = 2048*97;

class QSettings;
class QLabel;
class DabRadio;

class CirViewer : public QObject, private Ui_cirWidget, private PhaseTable
{
Q_OBJECT
public:
  CirViewer(RingBuffer<cf32> * iCirBuffer);
  ~CirViewer();
  void show_cir();
  void show();
  void hide();
  void setVisible(bool iVisible) { if (iVisible) show(); else hide(); }
  bool is_hidden();
  [[nodiscard]] QWidget * get_widget() { return &mFrame; }

private:
  QFrame mFrame;
  RingBuffer<cf32> * const mpCirBuffer;
  QLineSeries * mpCurve = nullptr;
  alignas(64) TArrayTu mFftInBuffer;
  alignas(64) TArrayTu mFftOutBuffer;
  fftwf_plan mFftPlanFwd;
  fftwf_plan mFftPlanBwd;
  cf32 cirbuffer[CIR_SPECTRUMSIZE];
};
