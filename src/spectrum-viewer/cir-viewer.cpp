#include <QSettings>
#include <QColor>
#include <QPen>
#include <QChart>
#include "cir-viewer.h"

#include "setting-helper.h"
#ifdef HAVE_SSE_OR_AVX
  #include <volk/volk.h>
#endif

CirViewer::CirViewer(RingBuffer<cf32> * iCirBuffer)
  : Ui_cirWidget()
  , PhaseTable()
  , mpCirBuffer(iCirBuffer)
{
  assert(((uintptr_t)mFftInBuffer.data()  & 0x3F) == 0);
  assert(((uintptr_t)mFftOutBuffer.data() & 0x3F) == 0);
  mFftPlanFwd = fftwf_plan_dft_1d(cTu, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_FORWARD, FFTW_ESTIMATE);
  mFftPlanBwd = fftwf_plan_dft_1d(cTu, (fftwf_complex*)mFftInBuffer.data(), (fftwf_complex*)mFftOutBuffer.data(), FFTW_BACKWARD, FFTW_ESTIMATE);

  setupUi(&mFrame);

  Settings::CirViewer::posAndSize.read_widget_geometry(&mFrame);

  mFrame.setWindowFlag(Qt::Tool, true);
  mFrame.hide();

  // cirPlot is a PlotWidget (promoted in the UI)
  cirPlot->get_x_axis()->setGridLineVisible(true);
  cirPlot->get_x_axis()->setMinorGridLineVisible(true);
  cirPlot->get_y_axis()->setGridLineVisible(true);
  cirPlot->get_y_axis()->setMinorGridLineVisible(true);

  mpCurve = new QLineSeries();
  mpCurve->setPen(QPen(QColor(0xffbe6f), 2.0));
  mpCurve->setUseOpenGL(false);
  cirPlot->chart()->addSeries(mpCurve);
  mpCurve->attachAxis(cirPlot->get_x_axis());
  mpCurve->attachAxis(cirPlot->get_y_axis());

  cirPlot->set_x_range(0, 96);
  cirPlot->set_y_range(0, 1.0);

  connect(&mFrame, &CustomFrame::signal_frame_closed, this, &CirViewer::signal_frame_closed);
}

CirViewer::~CirViewer()
{
  Settings::CirViewer::posAndSize.write_widget_geometry(&mFrame);

  mFrame.hide();
  fftwf_destroy_plan(mFftPlanBwd);
  fftwf_destroy_plan(mFftPlanFwd);
}

void CirViewer::show_cir()
{
  constexpr i32 sPlotLength = 385;
  std::array<f32, sPlotLength> X_axis;
  std::array<f32, sPlotLength> Y_value;

  if (mpCirBuffer->get_ring_buffer_read_available() < CIR_SPECTRUMSIZE)
  {
    return;
  }
  mpCirBuffer->get_data_from_ring_buffer(cirbuffer, CIR_SPECTRUMSIZE);

  for(i32 j = 0; j < sPlotLength; j++)
  {
    memcpy(mFftInBuffer.data(), &cirbuffer[j*512], sizeof(cf32)*cTu);
    fftwf_execute(mFftPlanFwd);

#ifdef HAVE_SSE_OR_AVX
    volk_32fc_x2_multiply_conjugate_32fc_a(mFftInBuffer.data(), mFftOutBuffer.data(), mRefTable.data(), cTu);
#else
    for (i32 i = 0; i < cTu; i++)
    {
      mFftInBuffer[i] = mFftOutBuffer[i] * conj(mRefTable[i]);
    }
#endif
    fftwf_execute(mFftPlanBwd);

#ifdef HAVE_SSE_OR_AVX
    alignas(64) u32 index;
    volk_32fc_index_max_32u_a(&index, mFftOutBuffer.data(), cTu);
    Y_value[j] = abs(mFftOutBuffer[index]) / 26000.0f;
#else
    f32 max = 0;
    for(i32 i = 0; i < cTu; i++)
    {
      const f32 x = norm(mFftOutBuffer[i]);
      if(x > max)
        max = x;
    }
    Y_value[j] = std::sqrt(max) / 26000.0;
#endif
    X_axis[j] = (f32)(j) / 4.0f;
  }

  mpCirBuffer->flush_ring_buffer();

  QList<QPointF> pts;
  pts.reserve(sPlotLength);
  for (i32 i = 0; i < sPlotLength; i++)
  {
    pts.append(QPointF(X_axis[i], Y_value[i]));
  }
  mpCurve->replace(pts);
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
