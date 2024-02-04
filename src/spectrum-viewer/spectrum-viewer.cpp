/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C)  2014 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include  <QSettings>
#include  <QColor>
#include  <QPen>
#include  "spectrum-viewer.h"
#include  "spectrum-scope.h"
#include  "waterfall-scope.h"
#include  "correlation-viewer.h"
#include  "carrier-display.h"
#include  "iqdisplay.h"

SpectrumViewer::SpectrumViewer(RadioInterface * ipRI, QSettings * ipDabSettings, RingBuffer<cmplx> * ipSpecBuffer,
                               RingBuffer<cmplx> * ipIqBuffer, RingBuffer<float> * ipCarrBuffer, RingBuffer<float> * ipCorrBuffer) :
  Ui_scopeWidget(),
  mpRadioInterface(ipRI),
  mpDabSettings(ipDabSettings),
  mpSpectrumBuffer(ipSpecBuffer),
  mpIqBuffer(ipIqBuffer),
  mpCarrBuffer(ipCarrBuffer),
  mpCorrelationBuffer(ipCorrBuffer)
{
  ipDabSettings->beginGroup(SETTING_GROUP_NAME);
  int32_t x = ipDabSettings->value("position-x", 100).toInt();
  int32_t y = ipDabSettings->value("position-y", 100).toInt();
  int32_t w = ipDabSettings->value("width", 1100).toInt();
  int32_t h = ipDabSettings->value("height", 600).toInt();
  ipDabSettings->endGroup();

  setupUi(&myFrame);

  connect(&myFrame, &CustomFrame::signal_frame_closed, this, &SpectrumViewer::signal_window_closed);

  myFrame.resize(QSize(w, h));
  myFrame.move(QPoint(x, y));
  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  myFrame.hide();

  //create_blackman_window(mWindowVec.data(), SP_SPECTRUMSIZE);
  create_flattop_window(mWindowVec.data(), SP_SPECTRUMSIZE);

  mpSpectrumScope = new SpectrumScope(dabScope, SP_DISPLAYSIZE, ipDabSettings);
  mpWaterfallScope = new WaterfallScope(dabWaterfall, SP_DISPLAYSIZE, 50);
  mpIQDisplay = new IQDisplay(iqDisplay);
  mpCarrierDisp = new CarrierDisp(phaseCarrPlot);
  mpCorrelationViewer = new CorrelationViewer(impulseGrid, indexDisplay, ipDabSettings, mpCorrelationBuffer);
  mShowInLogScale = cbLogIqScope->isChecked();

  cmbCarrier->addItems(CarrierDisp::get_plot_type_names()); // fill combobox with text elements
  cmbIqScope->addItems(IQDisplay::get_plot_type_names()); // fill combobox with text elements

  set_spectrum_averaging_rate(EAvrRate::DEFAULT);
  
  _load_save_combobox_settings(cmbIqScope, "iqPlot", false);
  _load_save_combobox_settings(cmbCarrier, "carrierPlot", false);

  connect(dabWaterfallAmplitude, &QSlider::valueChanged, mpWaterfallScope, &WaterfallScope::slot_scaling_changed);
  connect(scopeAmplification, &QSlider::valueChanged, mpSpectrumScope, &SpectrumScope::slot_scaling_changed);
  connect(cmbCarrier, qOverload<int32_t>(&QComboBox::currentIndexChanged), this, &SpectrumViewer::_slot_handle_cmb_carrier);
  connect(cmbIqScope, qOverload<int32_t>(&QComboBox::currentIndexChanged), this, &SpectrumViewer::_slot_handle_cmb_iqscope);
  connect(cbNomChIdx, &QCheckBox::stateChanged, this, &SpectrumViewer::_slot_handle_cb_nom_carrier);
}

SpectrumViewer::~SpectrumViewer()
{
  mpDabSettings->beginGroup(SETTING_GROUP_NAME);
  mpDabSettings->setValue("position-x", myFrame.pos().x());
  mpDabSettings->setValue("position-y", myFrame.pos().y());

  QSize size = myFrame.size();
  mpDabSettings->setValue("width", size.width());
  mpDabSettings->setValue("height", size.height());
  mpDabSettings->endGroup();

  myFrame.hide();

  delete mpCorrelationViewer;
  delete mpCarrierDisp;
  delete mpIQDisplay;
  delete mpSpectrumScope;
  delete mpWaterfallScope;
}

void SpectrumViewer::show_spectrum(int32_t vfoFrequency)
{
  constexpr int32_t averageCount = 5;

  if (mpSpectrumBuffer->get_ring_buffer_read_available() < SP_SPECTRUMSIZE)
  {
    return;
  }

  mpSpectrumBuffer->get_data_from_ring_buffer(mSpectrumVec.data(), SP_SPECTRUMSIZE);
  mpSpectrumBuffer->flush_ring_buffer();

  if (myFrame.isHidden())
  {
    mpSpectrumBuffer->flush_ring_buffer();
    return;
  }

  if (vfoFrequency != mLastVcoFreq) // same a bit time
  {
    mLastVcoFreq = vfoFrequency;
    constexpr double temp = (double)INPUT_RATE / 2 / SP_DISPLAYSIZE;
    for (int32_t i = 0; i < SP_DISPLAYSIZE; i++)
    {
      mXAxisVec[i] = ((double)vfoFrequency - (double)(INPUT_RATE / 2) + (i * 2.0 * temp)) / 1000.0;
    }
  }

  //	and window it
  //	get the buffer data
  for (int32_t i = 0; i < SP_SPECTRUMSIZE; i++)
  {
    mSpectrumVec[i] = mSpectrumVec[i] * mWindowVec[i];
  }

  fft.fft(mSpectrumVec.data());

  // map the SP_SPECTRUMSIZE values onto SP_DISPLAYSIZE elements
  for (int32_t i = 0; i < SP_DISPLAYSIZE / 2; i++)
  {
    double f = 0;
    for (int32_t j = 0; j < SP_SPEC_OVR_SMP_FAC; j++)
    {
      f += abs(mSpectrumVec[SP_SPEC_OVR_SMP_FAC * i + j]);
    }

    mYValVec[SP_DISPLAYSIZE / 2 + i] = f / (double)SP_SPEC_OVR_SMP_FAC;

    f = 0;
    for (int32_t j = 0; j < SP_SPEC_OVR_SMP_FAC; j++)
    {
      f += abs(mSpectrumVec[SP_SPECTRUMSIZE / 2 + SP_SPEC_OVR_SMP_FAC * i + j]);
    }

    mYValVec[i] = f / SP_SPEC_OVR_SMP_FAC;
  }

  double valdBGlobMin =  1000.0;
  double valdBGlobMax = -1000.0;
  double valdBLocMin =  1000.0;
  double valdBLocMax = -1000.0;

  constexpr int32_t signalBeginIdx = (int32_t)(SP_DISPLAYSIZE * (1.0 - (1536000.0  / INPUT_RATE)) / 2.0);
  constexpr int32_t signalEndIdx   = SP_DISPLAYSIZE - signalBeginIdx - 1;

  // average the image a little.
  for (int32_t i = 0; i < SP_DISPLAYSIZE; i++)
  {
    const double val = 20.0 * std::log10(mYValVec[i] / (double)SP_DISPLAYSIZE + 1.0e-6);
    double & valdBMean = mDisplayBuffer[i];
    mean_filter(valdBMean, val, 1.0 / averageCount);

    if (valdBMean > valdBGlobMax) valdBGlobMax = valdBMean;
    if (valdBMean < valdBGlobMin) valdBGlobMin = valdBMean;
    if (i >= signalBeginIdx && i <= signalEndIdx)
    {
      if (valdBMean > valdBLocMax) valdBLocMax = valdBMean;
      if (valdBMean < valdBLocMin) valdBLocMin = valdBMean;
    }
  }

  mean_filter(mSpecViewLimits.GlobMax, valdBGlobMax, mAvrAlpha);
  mean_filter(mSpecViewLimits.GlobMin, valdBGlobMin, mAvrAlpha);
  mean_filter(mSpecViewLimits.LocMin, valdBLocMin, mAvrAlpha);
  mean_filter(mSpecViewLimits.LocMax, valdBLocMax, mAvrAlpha);

  // avoid scaling jumps if <= -100 occurs in the display (seldom, correct other values accordingly)
  if (mSpecViewLimits.GlobMin < -99.0)
  {
    const double corrOff =  -99.0 - mSpecViewLimits.GlobMin; // is positive value
    mSpecViewLimits.GlobMax += corrOff;
    mSpecViewLimits.LocMin += corrOff;
    mSpecViewLimits.LocMax += corrOff;
  }

  mpWaterfallScope->show_waterfall(mXAxisVec.data(), mDisplayBuffer.data(), mSpecViewLimits);
  mpSpectrumScope->show_spectrum(mXAxisVec.data(), mDisplayBuffer.data(), mSpecViewLimits);
}

void SpectrumViewer::show()
{
  myFrame.show();
}

void SpectrumViewer::hide()
{
  myFrame.hide();
}

bool SpectrumViewer::is_hidden()
{
  return myFrame.isHidden();
}

void SpectrumViewer::show_iq(int32_t iAmount, float iAvg)
{
  if (mIqValuesVec.size() != (unsigned)iAmount)
  {
    mIqValuesVec.resize(iAmount);
    mCarrValuesVec.resize(iAmount);
  }

  const int32_t scopeWidth = scopeSlider->value();
  const bool logIqScope = cbLogIqScope->isChecked();

  if (mShowInLogScale != logIqScope)
  {
    mShowInLogScale = logIqScope;
  }

  const int32_t numRead = mpIqBuffer->get_data_from_ring_buffer(mIqValuesVec.data(), (int32_t)mIqValuesVec.size());
  /*const int32_t numRead2 =*/ mpCarrBuffer->get_data_from_ring_buffer(mCarrValuesVec.data(), (int32_t)mCarrValuesVec.size());

  if (myFrame.isHidden())
  {
    return;
  }

  if (logIqScope)
  {
    constexpr float logNorm = std::log10(1.0f + 1.0f);

    for (auto i = 0; i < numRead; i++)
    {
      const float phi = std::arg(mIqValuesVec[i]);
      const float rl = log10f(1.0f + std::abs(mIqValuesVec[i])) / logNorm;
      mIqValuesVec[i] = rl * std::exp(cmplx(0, phi)); // retain phase, only log the vector length
    }
  }

  mpIQDisplay->display_iq(mIqValuesVec, (float)scopeWidth / 100.0f);
  mpCarrierDisp->display_carrier_plot(mCarrValuesVec);
}

void SpectrumViewer::show_quality(int32_t iOfdmSymbNo, float iModQual, float iTimeOffset, float iFreqOffset, float iPhaseCorr, float iSNR)
{
  if (myFrame.isHidden())
  {
    return;
  }

  lcdOfdmSymbNo->display(iOfdmSymbNo);
  lcdModQuality->display(QString("%1").arg(iModQual, 0, 'f', 2));
  lcdTimeOffset->display(QString("%1").arg(iTimeOffset, 0, 'f', 2));
  lcdFreqOffset->display(QString("%1").arg(iFreqOffset, 0, 'f', 2));
  lcdPhaseCorr->display(QString("%1").arg(iPhaseCorr, 0, 'f', 2));
  lcdSnr->display(QString("%1").arg(iSNR, 0, 'f', 2));

  // Very strange thing: the configuration of the ThermoWidget colors has to be done in the calling thread.
  if (!mThermoModQualConfigured)
  {
    thermoModQual->setFillBrush(QBrush(QColor(229, 165, 10)));
    mThermoPeakLevelConfigured = true;
  }

  thermoModQual->setValue(iModQual);
}

void SpectrumViewer::show_nominal_frequency_MHz(float iFreqMHz)
{
  lcdNomFrequency->display(QString("%1").arg(iFreqMHz, 0, 'f', 3));
}

void SpectrumViewer::show_freq_corr_rf_Hz(int32_t iFreqCorrRF)
{
  lcdFreqCorrRF->display(iFreqCorrRF);
}

void SpectrumViewer::show_freq_corr_bb_Hz(int32_t iFreqCorrBB)
{
  lcdFreqCorrBB->display(iFreqCorrBB);
}

void SpectrumViewer::show_clock_error(float iClockErr)
{
  lcdClockError->display(QString("%1").arg(iClockErr, 0, 'f', 2));
}

void SpectrumViewer::show_correlation(int32_t dots, int32_t marker, const QVector<int32_t> & v)
{
  mpCorrelationViewer->showCorrelation(dots, marker, v);
}

void SpectrumViewer::_slot_handle_cmb_carrier(int32_t iSel)
{
  auto pt = static_cast<ECarrierPlotType>(iSel);
  mpCarrierDisp->select_plot_type(pt);
  emit signal_cmb_carrier_changed(pt);
  _load_save_combobox_settings(cmbCarrier, "carrierPlot", true);
}

void SpectrumViewer::_slot_handle_cmb_iqscope(int32_t iSel)
{
  auto pt = static_cast<EIqPlotType>(iSel);
  mpIQDisplay->select_plot_type(pt);
  emit signal_cmb_iqscope_changed(pt);
  _load_save_combobox_settings(cmbIqScope, "iqPlot", true);
}

void SpectrumViewer::_slot_handle_cb_nom_carrier(int32_t iSel)
{
  emit signal_cb_nom_carrier_changed(iSel != 0);
}

void SpectrumViewer::_load_save_combobox_settings(QComboBox * ipCmb, const QString & iName, bool iSave)
{
  mpDabSettings->beginGroup(SETTING_GROUP_NAME);
  if (iSave)
  {
    mpDabSettings->setValue(iName, ipCmb->currentText());
  }
  else
  {
    const QString h = mpDabSettings->value(iName, "default").toString();
    const int32_t k = ipCmb->findText(h);
    if (k != -1)
    {
      ipCmb->setCurrentIndex(k);
    }
  }
  mpDabSettings->endGroup();
}

void SpectrumViewer::slot_update_settings()
{
  // This is called when the DabProcessor has been started. Trigger resending UI state to DabProcessor.
  emit _slot_handle_cmb_carrier(cmbCarrier->currentIndex());
  emit _slot_handle_cmb_iqscope(cmbIqScope->currentIndex());
}

void SpectrumViewer::show_digital_peak_level(float iDigLevel)
{
  assert(iDigLevel >= 0.0f); // exact 0.0 is considered below.

  /* Very strange thing: the configuration of the ThermoWidget colors has to be done in the calling thread.
   * As this routine is also called one time in the instance creation thread, the counter must at least count two times.
   */
  if (mThermoPeakLevelConfigured < 2)
  {
    thermoDigLevel->setFillBrush(QBrush(QColor(Qt::darkCyan)));
    thermoDigLevel->setAlarmBrush(QBrush(QColor(Qt::red)));
    ++mThermoPeakLevelConfigured;
  }

  const float valuedB = 20.0f * std::log10(iDigLevel + 1.0e-6f);

  thermoDigLevel->setValue(valuedB);
}

void SpectrumViewer::set_spectrum_averaging_rate(SpectrumViewer::EAvrRate iAvrRate)
{
  switch (iAvrRate)
  {
  case EAvrRate::SLOW:   mAvrAlpha = 0.01; break;
  case EAvrRate::MEDIUM: mAvrAlpha = 0.10; break;
  case EAvrRate::FAST:   mAvrAlpha = 1.00; break;
  }
}
