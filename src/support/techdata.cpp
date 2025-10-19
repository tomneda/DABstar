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

  Settings::TechDataViewer::posAndSize.read_widget_geometry(&mFrame, 310, 640, true);

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
  framedumpButton->setEnabled(false);
  audiodumpButton->setEnabled(false);
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

void TechData::show_service_data(const SAudioData * ad) const
{
  slot_show_service_label(ad->serviceLabel);
  slot_show_SId(ad->SId);
  slot_show_bitrate(ad->bitRate);
  slot_show_CU_start_address(ad->CuStartAddr);
  slot_show_CU_size(ad->CuSize);
  slot_show_subChId(ad->SubChId);
  slot_show_uep(ad->shortForm, ad->protLevel);
  slot_show_ASCTy(ad->ASCTy);
  slot_show_coderate(ad->shortForm, ad->protLevel);
  framedumpButton->setEnabled(true);
  audiodumpButton->setEnabled(true);
}

void TechData::show_service_data_addon(const SAudioDataAddOns * adon) const
{
  slot_show_language(adon->language.value_or(0));
  slot_show_fm(-1);  // TODO: switched off
  // slot_show_fm(ad->fmFrequency);
}

void TechData::show()
{
  mFrame.show();
}

void TechData::hide()
{
  mFrame.hide();
}

bool TechData::isHidden() const
{
  return mFrame.isHidden();
}

void TechData::slot_show_frame_error_bar(i32 e) const
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

void TechData::slot_show_aac_error_bar(i32 e) const
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

void TechData::slot_show_rs_error_bar(i32 e) const
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

void TechData::slot_show_rs_corrections(i32 c, i32 ec) const
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

void TechData::_slot_show_motHandling(bool b) const
{
  motAvailable->setStyleSheet(b ? "QLabel {background-color : green; color: white}" : "QLabel {background-color : #444444; color : white}");
}

void TechData::slot_show_timetableButton(bool b) const
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

void TechData::slot_show_service_label(const QString & s) const
{
  programName->setText(s);
}

void TechData::slot_show_SId(u32 iSId) const
{
  serviceIdDisplay->display((i32)iSId);
  if ((iSId & 0x8000'0000) != 0) // display only knows negative numbers but not u32, should this ever happen?
  {
    qWarning() << "Service ID can not displayed correctly";
  }
}

void TechData::slot_show_bitrate(i32 br) const
{
  bitrateDisplay->display(br);
}

void TechData::slot_show_CU_start_address(i32 sa) const
{
  startAddressDisplay->display(sa);
}

void TechData::slot_show_CU_size(i32 l) const
{
  lengthDisplay->display(l);
}

void TechData::slot_show_subChId(i32 subChId) const
{
  subChIdDisplay->display(subChId);
}

void TechData::slot_show_language(i32 l) const
{
  language->setText(getLanguage(l));
}

void TechData::slot_show_ASCTy(i32 a) const
{
  const bool isDabPlus = (a == 077);
  ASCTy->setText(isDabPlus ? "DAB+" : "DAB");
  framedumpButton->setText(isDabPlus ? "Dump AAC" : "Dump MP2");
  rsError_display->setEnabled(isDabPlus);
  aacError_display->setEnabled(isDabPlus);
  aacErrorLabel->setEnabled(isDabPlus);
  rsErrorLabel->setEnabled(isDabPlus);
}

void TechData::slot_show_uep(i32 shortForm, i32 protLevel) const
{
  QString protL = getProtectionLevel(shortForm, protLevel);
  uepField->setText(protL);
}

void TechData::slot_show_coderate(i32 shortForm, i32 protLevel) const
{
  codeRate->setText(getCodeRate(shortForm, protLevel));
}

void TechData::slot_show_fm(i32 freq) const
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

void TechData::slot_audio_data_available(i32 /*iNumSamples*/, i32 iSampleRate) const
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

void TechData::slot_show_sample_rate_and_audio_flags(i32 iSampleRate, bool iSbrUsed, bool iPsUsed) const
{
  audioRate->display(iSampleRate / 1000);
  QString afs;
  afs += (iSbrUsed ? "YES / " : "NO / ");
  afs += (iPsUsed ? "YES" : "NO");
  lblSbrUsed->setText(afs);
}

