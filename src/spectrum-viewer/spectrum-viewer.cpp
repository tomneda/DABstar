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

#include  "spectrum-viewer.h"
#include  "spectrum-scope.h"
#include  "waterfall-scope.h"
#include  "correlation-viewer.h"
#include  "carrier-display.h"
#include  <QSettings>
#include  "iqdisplay.h"
#include  <QColor>
#include  <QPen>


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
  int32_t w = ipDabSettings->value("width", 150).toInt();
  int32_t h = ipDabSettings->value("height", 120).toInt();
  ipDabSettings->endGroup();

  setupUi(&myFrame);

  myFrame.resize(QSize(w, h));
  myFrame.move(QPoint(x, y));
  myFrame.hide();

  create_blackman_window(mWindowVec.data(), SP_SPECTRUMSIZE);

  mpSpectrumScope = new SpectrumScope(dabScope, SP_DISPLAYSIZE, ipDabSettings);
  mpWaterfallScope = new WaterfallScope(dabWaterfall, SP_DISPLAYSIZE, 50);
  mpIQDisplay = new IQDisplay(iqDisplay);
  mpCarrierDisp = new CarrierDisp(phaseCarrPlot);
  mpCorrelationViewer = new CorrelationViewer(impulseGrid, indexDisplay, ipDabSettings, mpCorrelationBuffer);

  set_bit_depth(12);

  mShowInLogScale = cbLogIqScope->isChecked();

  cmbCarrier->addItems(CarrierDisp::get_plot_type_names()); // fill combobox with text elements
  cmbIqScope->addItems(IQDisplay::get_plot_type_names()); // fill combobox with text elements

  _load_save_combobox_settings(cmbIqScope, "iqPlot", false);
  _load_save_combobox_settings(cmbCarrier, "carrierPlot", false);

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

void SpectrumViewer::show_spectrum(int32_t amount, int32_t vfoFrequency)
{
  (void)amount;

  constexpr int32_t averageCount = 5;

  if (mpSpectrumBuffer->GetRingBufferReadAvailable() < SP_SPECTRUMSIZE)
  {
    return;
  }

  mpSpectrumBuffer->getDataFromBuffer(mSpectrumVec.data(), SP_SPECTRUMSIZE);
  mpSpectrumBuffer->FlushRingBuffer();

  if (myFrame.isHidden())
  {
    mpSpectrumBuffer->FlushRingBuffer();
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
    if (std::isnan(abs(mSpectrumVec[i])) || std::isinf(abs(mSpectrumVec[i])))
    {
      mSpectrumVec[i] = cmplx(0, 0);
    }
    else
    {
      mSpectrumVec[i] = mSpectrumVec[i] * mWindowVec[i];
    }
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

  // average the image a little.
  for (int32_t i = 0; i < SP_DISPLAYSIZE; i++)
  {
    if (std::isnan(mYValVec[i]) || std::isinf(mYValVec[i]))
    {
      continue;
    }
    mean_filter(mDisplayBuffer[i], mYValVec[i], 1.0 / averageCount);
  }

  mpWaterfallScope->show_waterfall(mXAxisVec.data(), mDisplayBuffer.data(), dabWaterfallAmplitude->value());
  mpSpectrumScope->show_spectrum(mXAxisVec.data(), mDisplayBuffer.data(), scopeAmplification->value());
}

void SpectrumViewer::set_bit_depth(int16_t d)
{
  if (d < 0 || d > 32)
  {
    d = 24;
  }

  mNormalizer = 1;
  while (--d > 0)
  {
    mNormalizer <<= 1;
  }

  mpSpectrumScope->set_bit_depth(mNormalizer);
  mpWaterfallScope->set_bit_depth(mNormalizer);
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

  const int32_t numRead = mpIqBuffer->getDataFromBuffer(mIqValuesVec.data(), (int32_t)mIqValuesVec.size());
  /*const int32_t numRead2 =*/ mpCarrBuffer->getDataFromBuffer(mCarrValuesVec.data(), (int32_t)mCarrValuesVec.size());

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
  if (myFrame.isHidden())
  {
    return;
  }
  lcdNomFrequency->display(QString("%1").arg(iFreqMHz, 0, 'f', 3));
}

void SpectrumViewer::show_freq_corr_rf_Hz(int32_t iFreqCorrRF)
{
  if (myFrame.isHidden())
  {
    return;
  }
  lcdFreqCorrRF->display(iFreqCorrRF);
}

void SpectrumViewer::show_freq_corr_bb_Hz(int32_t iFreqCorrBB)
{
  if (myFrame.isHidden())
  {
    return;
  }
  lcdFreqCorrBB->display(iFreqCorrBB);
}

void SpectrumViewer::show_clock_error(float iClockErr)
{
  if (!myFrame.isHidden())
  {
    lcdClockError->display(QString("%1").arg(iClockErr, 0, 'f', 2));
  }
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

  const float valuedB = 20.0f * std::log10(iDigLevel + 0.00001f);

  thermoDigLevel->setValue(valuedB);
}
