#include <QSettings>
#include <QColor>
#include <QPen>
#include "cir-viewer.h"
#include  "time_meas.h"

CirViewer::CirViewer(QSettings * s, RingBuffer<cmplx> * iCirBuffer) :
  Ui_cirWidget(),
  PhaseTable(1),
  mpCirBuffer(iCirBuffer)
{
  mFftPlanFwd = fftwf_plan_dft_1d(2048, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_FORWARD, FFTW_ESTIMATE);
  mFftPlanBwd = fftwf_plan_dft_1d(2048, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_BACKWARD, FFTW_ESTIMATE);

  cirSettings = s;
  cirSettings->beginGroup(SETTING_GROUP_NAME);
  int x = cirSettings->value("position-x", 100).toInt();
  int y = cirSettings->value("position-y", 100).toInt();
  cirSettings->endGroup();

  setupUi(&myFrame);
  myFrame.move(QPoint(x, y));
  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  myFrame.hide();

  mpPlot = cirPlot;
  QColor color = QColor("#5e5c64");
  grid.setMajorPen(QPen(color, 0, Qt::DotLine));
  grid.setMinorPen(QPen(color, 0, Qt::DotLine));
  grid.enableXMin(true);
  grid.enableYMin(false);
  grid.attach(mpPlot);

  color = QColor("#ffbe6f");
  curve.setPen(QPen(color, 2.0));
  curve.setOrientation(Qt::Horizontal);
  curve.setBaseline(0);
  curve.attach(mpPlot);

  mpPlot->setAxisScale(QwtPlot::yLeft, 0, 1.0);
  mpPlot->enableAxis(QwtPlot::yLeft);

  mpPlot->setAxisScale(QwtPlot::xBottom, 0, 96);
  mpPlot->enableAxis(QwtPlot::xBottom);
}

CirViewer::~CirViewer()
{
  cirSettings->beginGroup(SETTING_GROUP_NAME);
  cirSettings->setValue("position-x", myFrame.pos().x());
  cirSettings->setValue("position-y", myFrame.pos().y());
  cirSettings->endGroup();
  myFrame.hide();
  fftwf_destroy_plan(mFftPlanBwd);
  fftwf_destroy_plan(mFftPlanFwd);
  delete mpPlot;
}

void CirViewer::show_cir()
{
  //time_meas_begin(show_cir);
  cmplx cirbuffer[CIR_SPECTRUMSIZE];
  constexpr int32_t sPlotLength = 385;
  std::array<float, sPlotLength> X_axis;
  std::array<float, sPlotLength> Y_value;

  if (mpCirBuffer->get_ring_buffer_read_available() < CIR_SPECTRUMSIZE)
  {
    return;
  }
  mpCirBuffer->get_data_from_ring_buffer(cirbuffer, CIR_SPECTRUMSIZE);

  for(int j = 0; j < sPlotLength; j++)
  {
    int i;
    float max = 0;

    memcpy(mFftInBuffer.data(), &cirbuffer[j*512], sizeof(cmplx)*2048);
    fftwf_execute(mFftPlanFwd);

    // into the frequency domain, now correlate
    for (i = 0; i < 2048; i++)
      mFftInBuffer[i] = mFftOutBuffer[i] * conj(mRefTable[i]);

    // and, again, back into the time domain
    fftwf_execute(mFftPlanBwd);

	// find maximum value
    for(i = 0; i < 2048; i++)
    {
   	  const float x = abs(mFftOutBuffer[i]);
      if(x > max)
        max = x;
      //if(x > 10000) fprintf(stderr, "j=%d, i=%d, x=%.0f\n",j,i,x);
    }
    Y_value[j] = max;
    X_axis[j] = (float)(j) / 4.0; // X-axis shows ms
  }

  mpCirBuffer->flush_ring_buffer();
  for (int i = 0; i < sPlotLength; i++)
  {
    Y_value[i] /= 26000;
  }
  mpPlot->enableAxis(QwtPlot::yLeft);
  curve.setSamples(X_axis.data(), Y_value.data(), sPlotLength);
  mpPlot->replot();
  //time_meas_end_print_us(show_cir);
}

void CirViewer::show()
{
  myFrame.show();
}

void CirViewer::hide()
{
  myFrame.hide();
}

bool CirViewer::is_hidden()
{
  return myFrame.isHidden();
}
