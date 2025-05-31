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

#include <QSettings>
#include <QColor>
#include <QPen>
#include "spectrum-viewer.h"
#include "spectrum-scope.h"
#include "waterfall-scope.h"
#include "correlation-viewer.h"
#include "carrier-display.h"
#include "iqdisplay.h"
#include "setting-helper.h"

SpectrumViewer::SpectrumViewer(DabRadio * ipRI, QSettings * ipDabSettings, RingBuffer<cf32> * ipSpecBuffer,
                               RingBuffer<cf32> * ipIqBuffer, RingBuffer<f32> * ipCarrBuffer, RingBuffer<f32> * ipCorrBuffer) :
  Ui_scopeWidget(),
  mpRadioInterface(ipRI),
  mpDabSettings(ipDabSettings),
  mpSpectrumBuffer(ipSpecBuffer),
  mpIqBuffer(ipIqBuffer),
  mpCarrBuffer(ipCarrBuffer),
  mpCorrelationBuffer(ipCorrBuffer)
{
  setupUi(&myFrame);

  connect(&myFrame, &CustomFrame::signal_frame_closed, this, &SpectrumViewer::signal_window_closed);

  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  Settings::SpectrumViewer::posAndSize.read_widget_geometry(&myFrame, 1100, 600, false);
  myFrame.hide();

  //create_blackman_window(mWindowVec.data(), SP_SPECTRUMSIZE);
  create_flattop_window(mWindowVec.data(), SP_SPECTRUMSIZE);

  mpSpectrumScope = new SpectrumScope(dabScope, SP_DISPLAYSIZE, ipDabSettings);
  mpWaterfallScope = new WaterfallScope(dabWaterfall, SP_DISPLAYSIZE, 50);
  mpIQDisplay = new IQDisplay(iqDisplay);
  mpCarrierDisp = new CarrierDisp(phaseCarrPlot);
  mpCorrelationViewer = new CorrelationViewer(impulseGrid, indexDisplay, ipDabSettings, mpCorrelationBuffer);

  cmbCarrier->addItems(CarrierDisp::get_plot_type_names()); // fill combobox with text elements
  cmbIqScope->addItems(IQDisplay::get_plot_type_names()); // fill combobox with text elements

  set_spectrum_averaging_rate(EAvrRate::DEFAULT);

  Settings::SpectrumViewer::cmbIqScope.register_widget_and_update_ui_from_setting(cmbIqScope, "default");
  Settings::SpectrumViewer::cmbCarrier.register_widget_and_update_ui_from_setting(cmbCarrier, "default");

  connect(sliderRfScopeZoom, &QSlider::valueChanged, mpWaterfallScope, &WaterfallScope::slot_scaling_changed);
  connect(sliderRfScopeZoom, &QSlider::valueChanged, mpSpectrumScope, &SpectrumScope::slot_scaling_changed);
  connect(cmbCarrier, qOverload<i32>(&QComboBox::currentIndexChanged), this, &SpectrumViewer::_slot_handle_cmb_carrier);
  connect(cmbIqScope, qOverload<i32>(&QComboBox::currentIndexChanged), this, &SpectrumViewer::_slot_handle_cmb_iqscope);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(cbNomChIdx, &QCheckBox::checkStateChanged, this, &SpectrumViewer::_slot_handle_cb_nom_carrier);
#else
  connect(cbNomChIdx, &QCheckBox::stateChanged, this, &SpectrumViewer::_slot_handle_cb_nom_carrier);
#endif

  Settings::SpectrumViewer::sliderRfScopeZoom.register_widget_and_update_ui_from_setting(sliderRfScopeZoom, 0);
  Settings::SpectrumViewer::sliderIqScopeZoomExp.register_widget_and_update_ui_from_setting(sliderIqScopeZoom, 40); // 36.949 -> 0.5 after 10^(2*x-1)
}

SpectrumViewer::~SpectrumViewer()
{
  Settings::SpectrumViewer::posAndSize.write_widget_geometry(&myFrame);

  myFrame.hide();

  delete mpCorrelationViewer;
  delete mpCarrierDisp;
  delete mpIQDisplay;
  delete mpSpectrumScope;
  delete mpWaterfallScope;

  fftwf_destroy_plan(mFftPlan);
}

bool SpectrumViewer::_calc_spectrum_display_limits(SpecViewLimits<f64>::SMaxMin & ioMaxMin) const
{
  constexpr f64 cMinShownLevel = -99.0;
  constexpr f64 cMinLevelRange = 20.0;
  bool limitChanged = false;

  // first avoid drifting down lower than -99dB, as the display content will jump then, keep the top level for the first
  if (ioMaxMin.Min < cMinShownLevel)
  {
    limitChanged = true;
    ioMaxMin.Min = cMinShownLevel;
  }

  // now check if the minimum shown range is not < 20dB, set the line in the middle, if possible
  if (ioMaxMin.Max - ioMaxMin.Min < cMinLevelRange)
  {
    limitChanged = true;
    const f64 mean = (ioMaxMin.Max + ioMaxMin.Min) / 2;
    ioMaxMin.Min = mean - cMinLevelRange / 2;
    ioMaxMin.Max = mean + cMinLevelRange / 2;
    if (ioMaxMin.Min < cMinShownLevel) // maybe we shifted now too low? -> correct this
    {
      ioMaxMin.Max += cMinShownLevel - ioMaxMin.Min; // maintain level range
      ioMaxMin.Min = cMinShownLevel;
    }
  }
  return limitChanged;
}

void SpectrumViewer::show_spectrum(i32 vfoFrequency)
{
  constexpr i32 averageCount = 5;

  if (mpSpectrumBuffer->get_ring_buffer_read_available() < SP_SPECTRUMSIZE)
  {
    return;
  }

  mpSpectrumBuffer->get_data_from_ring_buffer(mFftInBuffer.data(), SP_SPECTRUMSIZE);
  mpSpectrumBuffer->flush_ring_buffer();

  if (myFrame.isHidden())
  {
    return;
  }

  if (vfoFrequency != mLastVcoFreq) // same a bit time
  {
    mLastVcoFreq = vfoFrequency;
    constexpr f64 temp = (f64)INPUT_RATE / 2 / SP_DISPLAYSIZE;
    for (i32 i = 0; i < SP_DISPLAYSIZE; i++)
    {
      mXAxisVec[i] = ((f64)vfoFrequency - (f64)(INPUT_RATE / 2) + (i * 2.0 * temp)) / 1000.0;
    }
  }

  //	and window it
  for (i32 i = 0; i < SP_SPECTRUMSIZE; i++)
  {
    mFftInBuffer[i] *= mWindowVec[i];
  }

  fftwf_execute(mFftPlan);

  // map the SP_SPECTRUMSIZE values onto SP_DISPLAYSIZE elements
  for (i32 i = 0; i < SP_DISPLAYSIZE / 2; i++)
  {
    f64 f = 0;
    for (i32 j = 0; j < SP_SPEC_OVR_SMP_FAC; j++)
    {
      f += std::abs(mFftOutBuffer[SP_SPEC_OVR_SMP_FAC * i + j]);
    }

    mYValVec[SP_DISPLAYSIZE / 2 + i] = f / (f64)SP_SPEC_OVR_SMP_FAC;

    f = 0;
    for (i32 j = 0; j < SP_SPEC_OVR_SMP_FAC; j++)
    {
      f += std::abs(mFftOutBuffer[SP_SPECTRUMSIZE / 2 + SP_SPEC_OVR_SMP_FAC * i + j]);
    }

    mYValVec[i] = f / SP_SPEC_OVR_SMP_FAC;
  }

  f64 valdBGlobMin =  1000.0;
  f64 valdBGlobMax = -1000.0;
  f64 valdBLocMin =  1000.0;
  f64 valdBLocMax = -1000.0;

  constexpr i32 signalBeginIdx = (i32)(SP_DISPLAYSIZE * (1.0 - (1536000.0  / INPUT_RATE)) / 2.0);
  constexpr i32 signalEndIdx   = SP_DISPLAYSIZE - signalBeginIdx - 1;

  // average the image a little.
  for (i32 i = 0; i < SP_DISPLAYSIZE; i++)
  {
    const f64 val = 20.0 * std::log10(mYValVec[i] / (f64)SP_DISPLAYSIZE + 1.0e-6);
    f64 & valdBMean = mDisplayBuffer[i];
    mean_filter(valdBMean, val, 1.0 / averageCount);

    if (valdBMean > valdBGlobMax) valdBGlobMax = valdBMean;
    if (valdBMean < valdBGlobMin) valdBGlobMin = valdBMean;
    if (i >= signalBeginIdx && i <= signalEndIdx)
    {
      if (valdBMean > valdBLocMax) valdBLocMax = valdBMean;
      if (valdBMean < valdBLocMin) valdBLocMin = valdBMean;
    }
  }

  mean_filter(mSpecViewLimits.Glob.Max, valdBGlobMax, mAvrAlpha);
  mean_filter(mSpecViewLimits.Glob.Min, valdBGlobMin, mAvrAlpha);
  mean_filter(mSpecViewLimits.Loc.Min, valdBLocMin, mAvrAlpha);
  mean_filter(mSpecViewLimits.Loc.Max, valdBLocMax, mAvrAlpha);

  // avoid scaling jumps if <= -100 occurs in the display (seldom, correct other values accordingly)
  if (_calc_spectrum_display_limits(mSpecViewLimits.Glob))
  {
    // if the global values has change, crosscheck also the local values
    _calc_spectrum_display_limits(mSpecViewLimits.Loc);
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

void SpectrumViewer::show_iq(i32 iAmount, f32 /*iAvg*/)
{
  if (mIqValuesVec.size() != (unsigned)iAmount)
  {
    mIqValuesVec.resize(iAmount);
    mCarrValuesVec.resize(iAmount);
  }

  mpIqBuffer->get_data_from_ring_buffer(mIqValuesVec.data(), (i32)mIqValuesVec.size());
  mpCarrBuffer->get_data_from_ring_buffer(mCarrValuesVec.data(), (i32)mCarrValuesVec.size());

  if (myFrame.isHidden())
  {
    return;
  }

  const i32 scopeWidth = sliderIqScopeZoom->value();
  mpIQDisplay->display_iq(mIqValuesVec, (f32)scopeWidth / 100.0f);
  mpCarrierDisp->display_carrier_plot(mCarrValuesVec);
}

void SpectrumViewer::show_lcd_data(i32 /*iOfdmSymbNo*/, f32 iModQual, f32 iTestData1, f32 iTestData2, f32 iMeanSigmaSqFreqCorr, f32 iSNR)
{
  if (myFrame.isHidden())
  {
    return;
  }

  //lcdOfdmSymbNo->display(iOfdmSymbNo);
  lcdTestData1->display(QString("%1").arg(iTestData1, 0, 'f', 2));
  lcdTestData2->display(QString("%1").arg(iTestData2, 0, 'f', 2));
  lcdSnr->display(QString("%1").arg(iSNR, 0, 'f', 1));
  lcdPhaseCorr->display(QString("%1").arg(iMeanSigmaSqFreqCorr, 0, 'f', 1));
  lcdModQuality->display(QString("%1").arg(iModQual, 0, 'f', 1));

  // Very strange thing: the configuration of the ThermoWidget colors has to be done in the calling thread.
  if (!mThermoModQualConfigured)
  {
    thermoModQual->setFillBrush(QBrush(QColor(229, 165, 10)));
    mThermoPeakLevelConfigured = true;
  }

  thermoModQual->setValue(iModQual);
}

void SpectrumViewer::show_fic_ber(f32 ber)
{
  if (!myFrame.isHidden())
  {
    lcdBER->display(QString("%1").arg(ber, 0, 'e', 1));
  }
}

void SpectrumViewer::show_nominal_frequency_MHz(f32 iFreqMHz)
{
  lcdNomFrequency->display(QString("%1").arg(iFreqMHz, 0, 'f', 3));
}

void SpectrumViewer::show_freq_corr_rf_Hz(i32 iFreqCorrRF)
{
  lcdFreqCorrRF->display(iFreqCorrRF);
}

void SpectrumViewer::show_freq_corr_bb_Hz(i32 iFreqCorrBB)
{
  lcdFreqCorrBB->display(iFreqCorrBB);
}

void SpectrumViewer::show_clock_error(f32 iClockErr)
{
  lcdClockError->display(QString("%1").arg(iClockErr, 0, 'f', 2));
}

void SpectrumViewer::show_correlation(f32 threshold, const QVector<i32> & v, const std::vector<STiiResult> & iTr)
{
  mpCorrelationViewer->showCorrelation(threshold, v, iTr);
}

void SpectrumViewer::_slot_handle_cmb_carrier(i32 iSel)
{
  auto pt = static_cast<ECarrierPlotType>(iSel);
  mpCarrierDisp->select_plot_type(pt);
  emit signal_cmb_carrier_changed(pt);
}

void SpectrumViewer::_slot_handle_cmb_iqscope(i32 iSel)
{
  auto pt = static_cast<EIqPlotType>(iSel);
  mpIQDisplay->select_plot_type(pt);
  emit signal_cmb_iqscope_changed(pt);
}

void SpectrumViewer::_slot_handle_cb_nom_carrier(i32 iSel)
{
  emit signal_cb_nom_carrier_changed(iSel != 0);
}

void SpectrumViewer::slot_update_settings()
{
  // This is called when the DabProcessor has been started. Trigger resending UI state to DabProcessor.
  emit _slot_handle_cmb_carrier(cmbCarrier->currentIndex());
  emit _slot_handle_cmb_iqscope(cmbIqScope->currentIndex());
}

void SpectrumViewer::show_digital_peak_level(f32 iDigLevel)
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

  const f32 valuedB = 20.0f * std::log10(iDigLevel + 1.0e-6f);

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
