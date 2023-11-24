/*
 *    Copyright (C)  2015, 2016, 2017, 2018, 2019, 2020, 2021
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB 
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
#include  "techdata.h"
#include  "radio.h"
#include  "audio-display.h"
#include  "dab-tables.h"
#include  "color-selector.h"

TechData::TechData(RadioInterface * mr, QSettings * s, RingBuffer<int16_t> * audioData) :
  myFrame(nullptr)
{
  myRadioInterface = mr;
  dabSettings = s;
  this->audioData = audioData;

  setupUi(&myFrame);
  dabSettings->beginGroup("techDataSettings");
  int x = dabSettings->value("position-x", 100).toInt();
  int y = dabSettings->value("position-y", 100).toInt();
  int wi = dabSettings->value("width", 100).toInt();
  int he = dabSettings->value("height", 100).toInt();
  dabSettings->endGroup();
  myFrame.move(QPoint(x, y));
  myFrame.resize(QSize(wi, he));

  formLayout->setLabelAlignment(Qt::AlignLeft);
  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  myFrame.hide();
  timeTable_button->hide();
  the_audioDisplay = new AudioDisplay(mr, audio, dabSettings);

  connect(framedumpButton, SIGNAL (clicked()), this, SIGNAL (handle_frameDumping()));
  connect(audiodumpButton, SIGNAL (clicked()), this, SIGNAL (handle_audioDumping()));
  connect(timeTable_button, SIGNAL (clicked()), this, SIGNAL (handle_timeTable()));
}

TechData::~TechData()
{
  dabSettings->beginGroup("techDataSettings");
  dabSettings->setValue("position-x", myFrame.pos().x());
  dabSettings->setValue("position-y", myFrame.pos().y());
  dabSettings->setValue("width", myFrame.width());
  dabSettings->setValue("height", myFrame.height());
  dabSettings->endGroup();
  myFrame.hide();
  delete the_audioDisplay;
}

void TechData::cleanUp()
{
  const QString es("-");
  programName->setText(es);
  uepField->setText(es);
  codeRate->setText(es);
  ASCTy->setText(es);
  language->setText(es);
  lblSbrUsed->setText(es);
  show_rsCorrections(0, 0); // call via this method to consider color change
  frameError_display->setValue(0);
  rsError_display->setValue(0);
  aacError_display->setValue(0);
  bitrateDisplay->display(0);
  startAddressDisplay->display(0);
  lengthDisplay->display(0);
  subChIdDisplay->display(0);
  motAvailable->setStyleSheet("QLabel {background-color : red}");
  timeTable_button->hide();
  audioRate->display(0);
}

void TechData::show_serviceData(Audiodata * ad)
{
  show_serviceName(ad->serviceName);
  show_serviceId(ad->SId);
  show_bitRate(ad->bitRate);
  show_startAddress(ad->startAddr);
  show_length(ad->length);
  show_subChId(ad->subchId);
  show_uep(ad->shortForm, ad->protLevel);
  show_ASCTy(ad->ASCTy);
  show_codeRate(ad->shortForm, ad->protLevel);
  show_language(ad->language);
  show_fm(ad->fmFrequency);
}

void TechData::show()
{
  myFrame.show();
}

void TechData::hide()
{
  myFrame.hide();
}

bool TechData::isHidden()
{
  return myFrame.isHidden();
}


void TechData::show_frameErrors(int e)
{
  QPalette p = frameError_display->palette();
  if (100 - 4 * e < 80)
  {
    p.setColor(QPalette::Highlight, Qt::red);
  }
  else
  {
    p.setColor(QPalette::Highlight, Qt::green);
  }

  frameError_display->setPalette(p);
  frameError_display->setValue(100 - 4 * e);
}

void TechData::show_aacErrors(int e)
{
  QPalette p = aacError_display->palette();
  if (100 - 4 * e < 80)
  {
    p.setColor(QPalette::Highlight, Qt::red);
  }
  else
  {
    p.setColor(QPalette::Highlight, Qt::green);
  }
  aacError_display->setPalette(p);
  aacError_display->setValue(100 - 4 * e);
}

void TechData::show_rsErrors(int e)
{
  QPalette p = rsError_display->palette();
  if (100 - 4 * e < 80)
  {
    p.setColor(QPalette::Highlight, Qt::red);
  }
  else
  {
    p.setColor(QPalette::Highlight, Qt::green);
  }
  rsError_display->setPalette(p);
  rsError_display->setValue(100 - 4 * e);
}

void TechData::show_rsCorrections(int c, int ec)
{
  // highlight non-zero values with color
  auto set_val_with_col = [](QLCDNumber *ipLCD, int iVal)
  {
    QPalette p = ipLCD->palette();
    p.setColor(QPalette::WindowText, (iVal > 0 ? Qt::yellow : Qt::white));
    ipLCD->setPalette(p);
    ipLCD->display(iVal);
  };

  set_val_with_col(rsCorrections, c);
  set_val_with_col(ecCorrections, ec);
}

void TechData::show_motHandling(bool b)
{

  motAvailable->setStyleSheet(b ? "QLabel {background-color : green; color: white}" : "QLabel {background-color : red; color : white}");
}

void TechData::show_timetableButton(bool b)
{
  if (b)
  {
    timeTable_button->show();
  }
  else
  {
    timeTable_button->hide();
  }
}

void TechData::show_frameDumpButton(bool b)
{
  if (b)
  {
    framedumpButton->show();
  }
  else
  {
    framedumpButton->hide();
  }
}

void TechData::show_serviceName(const QString & s)
{
  programName->setText(s);
}

void TechData::show_serviceId(int SId)
{
  serviceIdDisplay->display(SId);
}

void TechData::show_bitRate(int br)
{
  bitrateDisplay->display(br);
}

void TechData::show_startAddress(int sa)
{
  startAddressDisplay->display(sa);
}

void TechData::show_length(int l)
{
  lengthDisplay->display(l);
}

void TechData::show_subChId(int subChId)
{
  subChIdDisplay->display(subChId);
}

void TechData::show_language(int l)
{
  language->setText(getLanguage(l));
}

void TechData::show_ASCTy(int a)
{
  ASCTy->setText(a == 077 ? "DAB+" : "DAB");
  if (a == 077)
  {
    rsError_display->show();
    aacError_display->show();
  }
  else
  {
    rsError_display->hide();
    aacError_display->hide();
  }
}

void TechData::show_uep(int shortForm, int protLevel)
{
  QString protL = getProtectionLevel(shortForm, protLevel);
  uepField->setText(protL);
}

void TechData::show_codeRate(int shortForm, int protLevel)
{
  codeRate->setText(getCodeRate(shortForm, protLevel));
}

void TechData::show_fm(int freq)
{
  if (freq == -1)
  {
    fmFrequency->hide();
    fmLabel->hide();
  }
  else
  {
    fmLabel->show();
    fmFrequency->show();
    QString f = QString::number(freq);
    f.append(" Khz");
    fmFrequency->setText(f);
  }
}

void TechData::audioDataAvailable(int amount, int rate)
{
  int16_t buffer[amount];

  audioData->get_data_from_ring_buffer(buffer, amount);
  if (!myFrame.isHidden())
  {
    the_audioDisplay->create_spectrum(buffer, amount, rate);
  }
}

void TechData::framedumpButton_text(const QString & text, int size)
{
  QFont font = framedumpButton->font();
  font.setPointSize(size);
  framedumpButton->setFont(font);
  framedumpButton->setText(text);
  framedumpButton->update();
}

void TechData::audiodumpButton_text(const QString & text, int size)
{
  QFont font = audiodumpButton->font();
  font.setPointSize(size);
  audiodumpButton->setFont(font);
  audiodumpButton->setText(text);
  audiodumpButton->update();
}

void TechData::show_sample_rate_and_audio_flags(int iSR, int iAudioFlags)
{
  audioRate->display(iSR);
  QString afs;
  afs += ((iAudioFlags & RadioInterface::AFL_SBR_USED) != 0 ? "YES / " : "NO / ");
  afs += ((iAudioFlags & RadioInterface::AFL_PS_USED) != 0 ? "YES" : "NO");
  lblSbrUsed->setText(afs);
}

void TechData::showMissed(int missed)
{
  missedSamples->display(missed);
}

void TechData::hideMissed()
{
  missedLabel->hide();
  missedSamples->hide();
}


