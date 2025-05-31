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
#include  "dabradio.h"
#include  "audio-display.h"
#include  "dab-tables.h"
#include  "color-selector.h"
#include "setting-helper.h"

TechData::TechData(DabRadio * mr, RingBuffer<i16> * ipAudioBuffer)
  : Ui_technical_data(),
    mpRadioInterface(mr)
  , mFrame()
{
  mpAudioBuffer = ipAudioBuffer;

  setupUi(&mFrame);

  Settings::TechDataViewer::posAndSize.read_widget_geometry(&mFrame, 310, 670, true);

  formLayout->setLabelAlignment(Qt::AlignLeft);
  mFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  mFrame.hide();
  timeTable_button->setEnabled(false);
  mpAudioDisplay = new AudioDisplay(mr, audio, &Settings::Storage::instance());

  mTimerMotReceived.setInterval(2000);
  mTimerMotReceived.setSingleShot(true);
  connect(&mTimerMotReceived, &QTimer::timeout, this, [this](){ _slot_show_motHandling(false); });

  cleanUp();

  connect(&mFrame, &CustomFrame::signal_frame_closed, this, &TechData::signal_window_closed);
  connect(framedumpButton, &QPushButton::clicked, this, &TechData::signal_handle_frameDumping);
  connect(audiodumpButton, &QPushButton::clicked, this, &TechData::signal_handle_audioDumping);
  connect(timeTable_button, &QPushButton::clicked, this, &TechData::signal_handle_timeTable);
}

TechData::~TechData()
{
  Settings::TechDataViewer::posAndSize.write_widget_geometry(&mFrame);

  mFrame.hide();

  delete mpAudioDisplay;
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
  slot_show_rs_corrections(0, 0); // call via this method to consider color change
  frameError_display->setValue(0);
  rsError_display->setValue(0);
  aacError_display->setValue(0);
  bitrateDisplay->display(0);
  startAddressDisplay->display(0);
  lengthDisplay->display(0);
  subChIdDisplay->display(0);
  _slot_show_motHandling(false);
  timeTable_button->setEnabled(false);
  audioRate->display(0);
}

void TechData::show_serviceData(const Audiodata * ad)
{
  slot_show_serviceName(ad->serviceName);
  slot_show_serviceId(ad->SId);
  slot_show_bitRate(ad->bitRate);
  slot_show_startAddress(ad->startAddr);
  slot_show_length(ad->length);
  slot_show_subChId(ad->subchId);
  slot_show_uep(ad->shortForm, ad->protLevel);
  slot_show_ASCTy(ad->ASCTy);
  slot_show_codeRate(ad->shortForm, ad->protLevel);
  slot_show_language(ad->language);
  slot_show_fm(ad->fmFrequency);
}

void TechData::show()
{
  mFrame.show();
}

void TechData::hide()
{
  mFrame.hide();
}

bool TechData::isHidden()
{
  return mFrame.isHidden();
}

void TechData::slot_show_frame_error_bar(i32 e)
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

void TechData::slot_show_aac_error_bar(i32 e)
{
  QPalette p = aacError_display->palette();
  if (100 - 4 * e < 80)  // e is error out of 25 frames so times 4
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

void TechData::slot_show_rs_error_bar(i32 e)
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

void TechData::slot_show_rs_corrections(i32 c, i32 ec)
{
  // highlight non-zero values with color
  auto set_val_with_col = [](QLCDNumber * ipLCD, i32 iVal)
  {
    QPalette p = ipLCD->palette();
    p.setColor(QPalette::WindowText, (iVal > 0 ? Qt::yellow : Qt::white));
    ipLCD->setPalette(p);
    ipLCD->display(iVal);
  };

  set_val_with_col(rsCorrections, c);
  set_val_with_col(ecCorrections, ec);
}

void TechData::slot_trigger_motHandling()
{
  if (!mTimerMotReceived.isActive())
  {
    _slot_show_motHandling(true);
  }
  mTimerMotReceived.start();
}

void TechData::_slot_show_motHandling(bool b)
{
  motAvailable->setStyleSheet(b ? "QLabel {background-color : green; color: white}" : "QLabel {background-color : #444444; color : white}");
}

void TechData::slot_show_timetableButton(bool b)
{
  if (b)
  {
    timeTable_button->setEnabled(true);
  }
  else
  {
    timeTable_button->setEnabled(false);
  }
}

void TechData::slot_show_frameDumpButton(bool b)
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

void TechData::slot_show_serviceName(const QString & s)
{
  programName->setText(s);
}

void TechData::slot_show_serviceId(i32 SId)
{
  serviceIdDisplay->display(SId);
}

void TechData::slot_show_bitRate(i32 br)
{
  bitrateDisplay->display(br);
}

void TechData::slot_show_startAddress(i32 sa)
{
  startAddressDisplay->display(sa);
}

void TechData::slot_show_length(i32 l)
{
  lengthDisplay->display(l);
}

void TechData::slot_show_subChId(i32 subChId)
{
  subChIdDisplay->display(subChId);
}

void TechData::slot_show_language(i32 l)
{
  language->setText(getLanguage(l));
}

void TechData::slot_show_ASCTy(i32 a)
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

void TechData::slot_show_uep(i32 shortForm, i32 protLevel)
{
  QString protL = getProtectionLevel(shortForm, protLevel);
  uepField->setText(protL);
}

void TechData::slot_show_codeRate(i32 shortForm, i32 protLevel)
{
  codeRate->setText(getCodeRate(shortForm, protLevel));
}

void TechData::slot_show_fm(i32 freq)
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

void TechData::slot_audio_data_available(i32 /*iNumSamples*/, i32 iSampleRate)
{
  constexpr i32 cNumNeededSample = 1024;

  if (mpAudioBuffer->get_ring_buffer_read_available() < cNumNeededSample)
  {
    return;
  }

  if (!mFrame.isHidden())
  {
    auto * const buffer = make_vla(i16, cNumNeededSample);
    mpAudioBuffer->get_data_from_ring_buffer(buffer, cNumNeededSample); // get 512 stereo samples
    mpAudioDisplay->create_spectrum(buffer, cNumNeededSample, iSampleRate);
  }

  mpAudioBuffer->flush_ring_buffer();
}

void TechData::slot_frame_dump_button_text(const QString & text, i32 size)
{
  QFont font = framedumpButton->font();
  font.setPointSize(size);
  framedumpButton->setFont(font);
  framedumpButton->setText(text);
  framedumpButton->update();
}

void TechData::slot_audio_dump_button_text(const QString & text, i32 size)
{
  QFont font = audiodumpButton->font();
  font.setPointSize(size);
  audiodumpButton->setFont(font);
  audiodumpButton->setText(text);
  audiodumpButton->update();
}

void TechData::slot_show_sample_rate_and_audio_flags(i32 iSampleRate, bool iSbrUsed, bool iPsUsed)
{
  audioRate->display(iSampleRate);
  QString afs;
  afs += (iSbrUsed ? "YES / " : "NO / ");
  afs += (iPsUsed ? "YES" : "NO");
  lblSbrUsed->setText(afs);
}

void TechData::slot_show_missed(i32 missed)
{
  missedSamples->display(missed);
}

void TechData::slot_hide_missed()
{
  missedLabel->hide();
  missedSamples->hide();
}
