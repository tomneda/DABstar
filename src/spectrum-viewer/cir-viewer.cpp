#include <QSettings>
#include <QColor>
#include <QPen>
#include "cir-viewer.h"
#ifdef HAVE_SSE_OR_AVX
  #include <volk/volk.h>
#endif

CirViewer::CirViewer(QSettings * s, RingBuffer<cmplx> * iCirBuffer) :
  Ui_cirWidget(),
  PhaseTable(1),
  mpCirSettings(s),
  mpCirBuffer(iCirBuffer)
{
  assert(((uintptr_t)mFftInBuffer.data()  & 0x1F) == 0);  // check for 32-byte alignment
  assert(((uintptr_t)mFftOutBuffer.data() & 0x1F) == 0);
  mFftPlanFwd = fftwf_plan_dft_1d(cTu, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_FORWARD, FFTW_ESTIMATE);
  mFftPlanBwd = fftwf_plan_dft_1d(cTu, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_BACKWARD, FFTW_ESTIMATE);

  mpCirSettings->beginGroup(SETTING_GROUP_NAME);
  int x = mpCirSettings->value("position-x", 100).toInt();
  int y = mpCirSettings->value("position-y", 100).toInt();
  int w = mpCirSettings->value("size-w", 500).toInt();
  int h = mpCirSettings->value("size-h", 250).toInt();
  mpCirSettings->endGroup();

  setupUi(&mFrame);

  mFrame.resize(QSize(w, h));
  mFrame.move(QPoint(x, y));
  mFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  mFrame.hide();

  QColor color = QColor("#5e5c64");
  mGrid.setMajorPen(QPen(color, 0, Qt::DotLine));
  mGrid.setMinorPen(QPen(color, 0, Qt::DotLine));
  mGrid.enableXMin(true);
  mGrid.enableYMin(false);
  mGrid.attach(cirPlot);

  color = QColor("#ffbe6f");
  mCurve.setPen(QPen(color, 2.0));
  mCurve.setOrientation(Qt::Horizontal);
  mCurve.setBaseline(0);
  mCurve.attach(cirPlot);

  cirPlot->setAxisScale(QwtPlot::yLeft, 0, 1.0);
  cirPlot->enableAxis(QwtPlot::yLeft);

  cirPlot->setAxisScale(QwtPlot::xBottom, 0, 96);
  cirPlot->enableAxis(QwtPlot::xBottom);
}

CirViewer::~CirViewer()
{
  mpCirSettings->beginGroup(SETTING_GROUP_NAME);
  mpCirSettings->setValue("position-x", mFrame.pos().x());
  mpCirSettings->setValue("position-y", mFrame.pos().y());
  mpCirSettings->setValue("size-w", mFrame.size().width());
  mpCirSettings->setValue("size-h", mFrame.size().height());
  mpCirSettings->endGroup();
  mFrame.hide();
  fftwf_destroy_plan(mFftPlanBwd);
  fftwf_destroy_plan(mFftPlanFwd);
}

void CirViewer::show_cir()
{
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
    memcpy(mFftInBuffer.data(), &cirbuffer[j*512], sizeof(cmplx)*cTu);
    fftwf_execute(mFftPlanFwd);

    // into the frequency domain, now correlate
#ifdef HAVE_SSE_OR_AVX
    volk_32fc_x2_multiply_conjugate_32fc_a(mFftInBuffer.data(), mFftOutBuffer.data(), mRefTable.data(), cTu);
#else
    for (int i = 0; i < cTu; i++)
      mFftInBuffer[i] = mFftOutBuffer[i] * conj(mRefTable[i]);
#endif
    // and, again, back into the time domain
    fftwf_execute(mFftPlanBwd);

    // find maximum value
#ifdef HAVE_SSE_OR_AVX
    alignas(32) uint32_t index;
    volk_32fc_index_max_32u_a(&index, mFftOutBuffer.data(), cTu);
    Y_value[j] = abs(mFftOutBuffer[index]) / 26000.0f;
#else
    float max = 0;
    for(int i = 0; i < cTu; i++)
    {
      const float x = norm(mFftOutBuffer[i]);
      if(x > max)
        max = x;
      //if(x > 10000) fprintf(stderr, "j=%d, i=%d, x=%.0f\n",j,i,x);
    }
    Y_value[j] = sqrt(max) / 26000.0;
#endif
    X_axis[j] = (float)(j) / 4.0f; // X-axis shows ms
  }

  mpCirBuffer->flush_ring_buffer();
  cirPlot->enableAxis(QwtPlot::yLeft);
  mCurve.setSamples(X_axis.data(), Y_value.data(), sPlotLength);
  cirPlot->replot();
}

void CirViewer::show()
{
  mFrame.show();
}

void CirViewer::hide()
{
  mFrame.hide();
}

bool CirViewer::is_hidden()
{
  return mFrame.isHidden();
}
