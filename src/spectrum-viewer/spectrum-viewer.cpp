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
#include  "color-selector.h"


SpectrumViewer::SpectrumViewer(RadioInterface * ipRI, QSettings * ipDabSettings, RingBuffer<cmplx> * ipSpecBuffer,
                               RingBuffer<cmplx> * ipIqBuffer, RingBuffer<float> * ipCarrBuffer, RingBuffer<float> * ipCorrBuffer) :
  Ui_scopeWidget(),
  myFrame(nullptr),
  myRadioInterface(ipRI),
  dabSettings(ipDabSettings),
  spectrumBuffer(ipSpecBuffer),
  iqBuffer(ipIqBuffer),
  carrBuffer(ipCarrBuffer),
  mpCorrelationBuffer(ipCorrBuffer),
  fft(SP_SPECTRUMSIZE, false)
{
  ipDabSettings->beginGroup("spectrumViewer");
  int x = ipDabSettings->value("position-x", 100).toInt();
  int y = ipDabSettings->value("position-y", 100).toInt();
  int w = ipDabSettings->value("width", 150).toInt();
  int h = ipDabSettings->value("height", 120).toInt();
  ipDabSettings->endGroup();

  setupUi(&myFrame);

  myFrame.resize(QSize(w, h));
  myFrame.move(QPoint(x, y));
  myFrame.hide();

  create_blackman_window(Window.data(), SP_SPECTRUMSIZE);

  mpSpectrumScope = new SpectrumScope(dabScope, SP_DISPLAYSIZE, ipDabSettings);
  mpWaterfallScope = new WaterfallScope(dabWaterfall, SP_DISPLAYSIZE, 50);
  mpIQDisplay = new IQDisplay(iqDisplay);
  mpCarrierDisp = new CarrierDisp(phaseCarrPlot);
  mpCorrelationViewer = new CorrelationViewer(impulseGrid, indexDisplay, ipDabSettings, mpCorrelationBuffer);

  setBitDepth(12);

  mShowInLogScale = cbLogIqScope->isChecked();

  cmbCarrier->addItems(CarrierDisp::get_plot_type_names()); // fill combobox with text elements
  cmbIqScope->addItems(IQDisplay::get_plot_type_names()); // fill combobox with text elements

  connect(cmbCarrier, qOverload<int>(&QComboBox::currentIndexChanged), this, &SpectrumViewer::_slot_handle_cmb_carrier);
  connect(cmbIqScope, qOverload<int>(&QComboBox::currentIndexChanged), this, &SpectrumViewer::_slot_handle_cmb_iqscope);
  connect(cbNomChIdx, &QCheckBox::stateChanged, this, &SpectrumViewer::_slot_handle_cb_nom_carrier);
}

SpectrumViewer::~SpectrumViewer()
{
  dabSettings->beginGroup("spectrumViewer");
  dabSettings->setValue("position-x", myFrame.pos().x());
  dabSettings->setValue("position-y", myFrame.pos().y());

  QSize size = myFrame.size();
  dabSettings->setValue("width", size.width());
  dabSettings->setValue("height", size.height());
  dabSettings->endGroup();

  myFrame.hide();

  delete mpCorrelationViewer;
  delete mpCarrierDisp;
  delete mpIQDisplay;
  delete mpSpectrumScope;
  delete mpWaterfallScope;
}

void SpectrumViewer::showSpectrum(int32_t amount, int32_t vfoFrequency)
{
  (void)amount;

  constexpr double temp = (double)INPUT_RATE / 2 / SP_DISPLAYSIZE;
  int16_t averageCount = 5;

  if (spectrumBuffer->GetRingBufferReadAvailable() < SP_SPECTRUMSIZE)
  {
    return;
  }

  spectrumBuffer->getDataFromBuffer(spectrum.data(), SP_SPECTRUMSIZE);
  spectrumBuffer->FlushRingBuffer();

  if (myFrame.isHidden())
  {
    spectrumBuffer->FlushRingBuffer();
    return;
  }

  if (vfoFrequency != lastVcoFreq) // same a bit time
  {
    lastVcoFreq = vfoFrequency;
    for (int i = 0; i < SP_DISPLAYSIZE; i++)
    {
      X_axis[i] = ((double)vfoFrequency - (double)(int)(INPUT_RATE / 2) + (double)((i) * (double)2 * temp)) / 1000.0;
    }
  }


  //	and window it
  //	get the buffer data
  for (int i = 0; i < SP_SPECTRUMSIZE; i++)
  {
    if (std::isnan(abs(spectrum[i])) || std::isinf(abs(spectrum[i])))
    {
      spectrum[i] = cmplx(0, 0);
    }
    else
    {
      spectrum[i] = spectrum[i] * Window[i];
    }
  }

  fft.fft(spectrum.data());

  // map the SP_SPECTRUMSIZE values onto SP_DISPLAYSIZE elements
  for (int i = 0; i < SP_DISPLAYSIZE / 2; i++)
  {
    double f = 0;
    for (int j = 0; j < SP_SPECTRUMSIZE / SP_DISPLAYSIZE; j++)
    {
      f += abs(spectrum[SP_SPECTRUMSIZE / SP_DISPLAYSIZE * i + j]);
    }

    Y_values[SP_DISPLAYSIZE / 2 + i] = f / (double)SP_SPECTRUMOVRSMPFAC; // (int) ->avoid clang-tidy issue
    f = 0;
    for (int j = 0; j < SP_SPECTRUMSIZE / SP_DISPLAYSIZE; j++)
    {
      f += abs(spectrum[SP_SPECTRUMSIZE / 2 + SP_SPECTRUMSIZE / SP_DISPLAYSIZE * i + j]);
    }
    Y_values[i] = f / SP_SPECTRUMOVRSMPFAC;
  }

  // average the image a little.
  for (int i = 0; i < SP_DISPLAYSIZE; i++)
  {
    if (std::isnan(Y_values[i]) || std::isinf(Y_values[i]))
    {
      continue;
    }

    displayBuffer[i] = (double)(averageCount - 1) / averageCount * displayBuffer[i] + 1.0 / averageCount * Y_values[i];
  }

  memcpy(Y_values.data(), displayBuffer.data(), SP_DISPLAYSIZE * sizeof(double));
  memcpy(Y2_values.data(), displayBuffer.data(), SP_DISPLAYSIZE * sizeof(double));
  mpSpectrumScope->showSpectrum(X_axis.data(), Y_values.data(), scopeAmplification->value(), vfoFrequency / 1000);
  mpWaterfallScope->display(X_axis.data(), Y2_values.data(), dabWaterfallAmplitude->value(), vfoFrequency / 1000);
}

float SpectrumViewer::get_db(float x) const
{
  return 20 * log10((x + 1) / (float)(normalizer));
}

void SpectrumViewer::setBitDepth(int16_t d)
{
  if (d < 0 || d > 32)
  {
    d = 24;
  }

  normalizer = 1;
  while (--d > 0)
  {
    normalizer <<= 1;
  }

  mpSpectrumScope->setBitDepth(normalizer);
}

void SpectrumViewer::show()
{
  myFrame.show();
}

void SpectrumViewer::hide()
{
  myFrame.hide();
}

bool SpectrumViewer::isHidden()
{
  return myFrame.isHidden();
}

void SpectrumViewer::showIQ(int iAmount, float iAvg)
{
  if (mIqValuesVec.size() != (unsigned)iAmount)
  {
    mIqValuesVec.resize(iAmount);
    mCarrValuesVec.resize(iAmount);
  }

  const int scopeWidth = scopeSlider->value();
  const bool logIqScope = cbLogIqScope->isChecked();

  if (mShowInLogScale != logIqScope)
  {
    mShowInLogScale = logIqScope;
  }

  const int32_t numRead = iqBuffer->getDataFromBuffer(mIqValuesVec.data(), (int32_t)mIqValuesVec.size());
  /*const int32_t numRead2 =*/ carrBuffer->getDataFromBuffer(mCarrValuesVec.data(), (int32_t)mCarrValuesVec.size());

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

  const float scale = (float)scopeWidth / 100.0f;

  mpIQDisplay->display_iq(mIqValuesVec, scale, scale);
  mpCarrierDisp->display_carrier_plot(mCarrValuesVec);
}

void SpectrumViewer::showQuality(int32_t iOfdmSymbNo, float iStdDev, float iTimeOffset, float iFreqOffset, float iPhaseCorr, float iSNR)
{
  if (myFrame.isHidden())
  {
    return;
  }

  ofdmSymbNo->display(iOfdmSymbNo);
  quality_display->display(QString("%1").arg(iStdDev, 0, 'f', 2));
  timeOffsetDisplay->display(QString("%1").arg(iTimeOffset, 0, 'f', 2));
  frequencyOffsetDisplay->display(QString("%1").arg(iFreqOffset, 0, 'f', 2));
  phaseCorrection->display(QString("%1").arg(iPhaseCorr, 0, 'f', 2));
  snrDisplay->display(QString("%1").arg(iSNR, 0, 'f', 2));
}

void SpectrumViewer::show_snr(float snr)
{
  if (myFrame.isHidden())
  {
    return;
  }
  snrDisplay->display(snr);
}

void SpectrumViewer::show_correction(int c)
{
  if (myFrame.isHidden())
  {
    return;
  }
  correctorDisplay->display(c);
}

void SpectrumViewer::show_clockErr(int e)
{
  if (!myFrame.isHidden())
  {
    clockError->display(e);
  }
}

void SpectrumViewer::show_frequency_MHz(float f)
{
  frequencyDisplay->display(f);
}

void SpectrumViewer::showCorrelation(int32_t dots, int marker, const QVector<int> & v)
{
  mpCorrelationViewer->showCorrelation(dots, marker, v);
}

void SpectrumViewer::_slot_handle_cmb_carrier(int iSel)
{
  auto pt = static_cast<ECarrierPlotType>(iSel);
  mpCarrierDisp->select_plot_type(pt);
  emit signal_cmb_carrier_changed(pt);
}

void SpectrumViewer::_slot_handle_cmb_iqscope(int iSel)
{
  auto pt = static_cast<EIqPlotType>(iSel);
  mpIQDisplay->select_plot_type(pt);
  emit signal_cmb_iqscope_changed(pt);
}

void SpectrumViewer::_slot_handle_cb_nom_carrier(int iSel)
{
  emit signal_cb_nom_carrier_changed(iSel != 0);
}
