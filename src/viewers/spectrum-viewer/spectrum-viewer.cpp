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
#include  "phase_carr_disp.h"
#include  <QSettings>
#include  "iqdisplay.h"
#include  <QColor>
#include  <QPen>
#include  "color-selector.h"


SpectrumViewer::SpectrumViewer(RadioInterface * mr, QSettings * dabSettings, RingBuffer<cmplx> * sbuffer, RingBuffer<cmplx> * ibuffer, RingBuffer<float> * cbuffer)
  : Ui_scopeWidget(),
    myFrame(nullptr),
    fft(SP_SPECTRUMSIZE, false)
{
  int16_t i;

  this->myRadioInterface = mr;
  this->dabSettings = dabSettings;
  this->spectrumBuffer = sbuffer;
  this->iqBuffer = ibuffer;
  this->mpCorrelationBuffer = cbuffer;

  dabSettings->beginGroup("spectrumViewer");
  int x = dabSettings->value("position-x", 100).toInt();
  int y = dabSettings->value("position-y", 100).toInt();
  int w = dabSettings->value("width", 150).toInt();
  int h = dabSettings->value("height", 120).toInt();
  dabSettings->endGroup();

  setupUi(&myFrame);

  myFrame.resize(QSize(w, h));
  myFrame.move(QPoint(x, y));

  //QPoint pos = myFrame.mapToGlobal(QPoint(0, 0));
  //fprintf(stdout, "spectrumViewer  %d %d, staat op %d %d\n", x, y, pos.x(), pos.y());
  myFrame.hide();

  for (i = 0; i < SP_SPECTRUMSIZE; i++)
  {
    Window[i] = static_cast<float>(0.42
                                 - 0.50 * cos((2.0 * M_PI * i) / (SP_SPECTRUMSIZE - 1))
                                 + 0.08 * cos((4.0 * M_PI * i) / (SP_SPECTRUMSIZE - 1)));
  }

  mySpectrumScope = new SpectrumScope(dabScope, SP_DISPLAYSIZE, dabSettings);
  myWaterfallScope = new WaterfallScope(dabWaterfall, SP_DISPLAYSIZE, 50);
  myIQDisplay = new IQDisplay(iqDisplay);
  mpPhaseVsCarrDisp = new PhaseVsCarrDisp(phaseCarrPlot);
  mpCorrelationViewer = new CorrelationViewer(impulseGrid, indexDisplay, dabSettings, mpCorrelationBuffer);
  //myNullScope = new nullScope(nullDisplay, 256, dabSettings);
  setBitDepth(12);
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
  delete mpPhaseVsCarrDisp;
  delete myIQDisplay;
  delete mySpectrumScope;
  delete myWaterfallScope;
  //delete myNullScope;
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

  //
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
  //spectrum = {std::array<std::complex<float>, 2048>}
  //	and map the SP_SPECTRUMSIZE values onto SP_DISPLAYSIZE elements
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
  //
  //	average the image a little.
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
  mySpectrumScope->showSpectrum(X_axis.data(), Y_values.data(), scopeAmplification->value(), vfoFrequency / 1000);
  myWaterfallScope->display(X_axis.data(), Y2_values.data(), dabWaterfallAmplitude->value(), vfoFrequency / 1000);
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

  mySpectrumScope->setBitDepth(normalizer);
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
  if (mValuesVec.size() != (unsigned)iAmount)
  {
    mValuesVec.resize(iAmount);
    mPhaseVec.resize(iAmount);
  }

  const int scopeWidth = scopeSlider->value();
  const bool logIqScope = cbLogIqScope->isChecked();

  const int32_t numRead = iqBuffer->getDataFromBuffer(mValuesVec.data(), (int32_t)mValuesVec.size());

  if (myFrame.isHidden())
  {
    return;
  }

  //float avg = 0;

  for (auto i = 0; i < numRead; i++)
  {
    const float r = std::fabs(mValuesVec[i]);

    if (!std::isnan(r) && !std::isinf(r))
    {
      const float phi = std::arg(mValuesVec[i]);
      mPhaseVec[i] = conv_rad_to_deg(phi);

      if (logIqScope)
      {
        const float rl = log10f(1.0f + r) / log10f(1.0f + 1.0f); // no scaling necessary here due to averaging
        mValuesVec[i] = rl * std::exp(cmplx(0, phi)); // retain phase only log the vector length
        //avg += rl;
      }
      else
      {
        //avg += r;
      }
    }
    else
    {
      mPhaseVec[i] = 0.0f;
      mValuesVec[i] = 0.0f;
    }
  }
  //avg /= (float)numRead;
  const float scale = (float)scopeWidth / 100.0f;
  myIQDisplay->display_iq(mValuesVec, scale, scale);
  mpPhaseVsCarrDisp->disp_phase_carr_plot(mPhaseVec);
}

void SpectrumViewer::showQuality(int32_t iOfdmSymbNo, float iStdDev, float iTimeOffset, float iFreqOffset, float iPhaseCorr)
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

void SpectrumViewer::showFrequency(float f)
{
  frequencyDisplay->display(f);
}

void SpectrumViewer::showCorrelation(int32_t dots, int marker, const QVector<int> & v)
{
  mpCorrelationViewer->showCorrelation(dots, marker, v);
}
