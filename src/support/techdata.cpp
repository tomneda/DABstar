/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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
#include  "audio_display.h"
#include  "dab_tables.h"
#include  "color_selector.h"
#include  "gui_helpers.h"
#include "setting_helper.h"

TechData::TechData(DabRadio * mr, RingBuffer<i16> * ipAudioBuffer)
  : Ui_technical_data(),
    mpRadioInterface(mr)
  , mFrame()
{
  mpAudioBuffer = ipAudioBuffer;

  setupUi(&mFrame);

  Settings::TechDataViewer::posAndSize.read_widget_geometry(&mFrame);

  formLayout->setLabelAlignment(Qt::AlignLeft);
  formLayout_2->setLabelAlignment(Qt::AlignLeft);

  framedumpButton->setStyleSheet(get_bg_style_sheet(0x4A7898, Qt::white));
  audiodumpButton->setStyleSheet(get_bg_style_sheet(0x408870, Qt::white));
  timeTable_button->setStyleSheet(get_bg_style_sheet(0xB89028, Qt::white));

  mFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  mFrame.hide();
  timeTable_button->setEnabled(false);
  mpAudioDisplay = new AudioDisplay(mr, plotAudioFft, &Settings::Storage::instance());

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
  lblProgramName->setText(es);
  lblUepField->setText(es);
  lblCodeRate->setText(es);
  lblAscTy->setText(es);
  lblLanguage->setText(es);
  lblSbrUsed->setText(es);
  framedumpButton->setEnabled(false);
  audiodumpButton->setEnabled(false);
  slot_show_rs_corrections(0, 0); // call via this method to consider color change
  frameError_display->setValue(0);
  rsError_display->setValue(0);
  aacError_display->setValue(0);
  lblBitrate->setText("0");
  lblStartAddress->setText("0");
  lblLength->setText("0");
  lblSubChId->setText("0");
  timeTable_button->setEnabled(false);
  lblAudioRate->setText("0");
}

void TechData::show_service_data(const SAudioData * ad) const
{
  _show_service_label(ad->serviceLabel);
  _show_SId(ad->SId);
  _show_bitrate(ad->bitRate);
  _show_CU_start_address(ad->CuStartAddr);
  _show_CU_size(ad->CuSize);
  _show_subChId(ad->SubChId);
  _show_uep_eep(ad->shortForm, ad->protLevel);
  _show_ASCTy(ad->ASCTy);
  _show_coderate(ad->shortForm, ad->protLevel);
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
    p.setColor(QPalette::Highlight, QColor(0xCC3333));
  }
  else
  {
    p.setColor(QPalette::Highlight, QColor(0x52A824));
  }

  frameError_display->setPalette(p);
  frameError_display->setValue(100 - 4 * e);
}

void TechData::slot_show_aac_error_bar(i32 e) const
{
  QPalette p = aacError_display->palette();
  if (100 - 4 * e < 80)  // e is error out of 25 frames so times 4
  {
    p.setColor(QPalette::Highlight, QColor(0xCC3333));
  }
  else
  {
    p.setColor(QPalette::Highlight, QColor(0x52A824));
  }
  aacError_display->setPalette(p);
  aacError_display->setValue(100 - 4 * e);
}

void TechData::slot_show_rs_error_bar(i32 e) const
{
  QPalette p = rsError_display->palette();
  if (100 - 4 * e < 80)
  {
    p.setColor(QPalette::Highlight, QColor(0xCC3333));
  }
  else
  {
    p.setColor(QPalette::Highlight, QColor(0x52A824));
  }
  rsError_display->setPalette(p);
  rsError_display->setValue(100 - 4 * e);
}

void TechData::slot_show_rs_corrections(i32 c, i32 ec) const
{
  // highlight non-zero values with amber color
  auto set_val_with_col = [](QLabel * ipLbl, i32 iVal)
  {
    ipLbl->setStyleSheet(iVal > 0 ? "font-weight: bold; color: #D4A030;" : "font-weight: bold;");
    ipLbl->setText(QString::number(iVal));
  };

  set_val_with_col(lblRsCorrections, c);
  set_val_with_col(lblEcCorrections, ec);
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

void TechData::_show_service_label(const QString & s) const
{
  lblProgramName->setText(s);
}

void TechData::_show_SId(u32 iSId) const
{
  lblServiceId->setText(QString::asprintf("0x%X", iSId));
}

void TechData::_show_bitrate(i32 br) const
{
  lblBitrate->setText(QString::number(br));
}

void TechData::_show_CU_start_address(i32 sa) const
{
  lblStartAddress->setText(QString::number(sa));
}

void TechData::_show_CU_size(i32 l) const
{
  lblLength->setText(QString::number(l));
}

void TechData::_show_subChId(i32 subChId) const
{
  lblSubChId->setText(QString::number(subChId));
}

void TechData::slot_show_language(i32 l) const
{
  lblLanguage->setText(getLanguage(l));
}

void TechData::_show_ASCTy(i32 a) const
{
  const bool isDabPlus = (a == 077);
  lblAscTy->setText(isDabPlus ? "DAB+" : "DAB");
  framedumpButton->setText(isDabPlus ? "Dump AAC" : "Dump MP2");
  aacError_display->setEnabled(isDabPlus);
  aacErrorLabel->setEnabled(isDabPlus);
  rsError_display->setEnabled(isDabPlus);
  rsErrorLabel->setEnabled(isDabPlus);
}

void TechData::_show_uep_eep(i32 shortForm, i32 protLevel) const
{
  QString protL = getProtectionLevel(shortForm, protLevel);
  lblUepField->setText(protL);
}

void TechData::_show_coderate(i32 shortForm, i32 protLevel) const
{
  lblCodeRate->setText(getCodeRate(shortForm, protLevel));
}

void TechData::set_audio_dump_button_emphasized(const bool iEmphasized) const
{
  static const QString sEmphasizedStyle{"background-color: #AE2B05; color: #EEEE00; font-weight: bold;"};
  audiodumpButton->setStyleSheet(iEmphasized ? sEmphasizedStyle : get_bg_style_sheet(0x408870, Qt::white));
}

void TechData::set_frame_dump_button_emphasized(const bool iEmphasized) const
{
  static const QString sEmphasizedStyle{"background-color: #AE2B05; color: #EEEE00; font-weight: bold;"};
  framedumpButton->setStyleSheet(iEmphasized ? sEmphasizedStyle : get_bg_style_sheet(0x4A7898, Qt::white));
}

void TechData::slot_show_fm(i32 freq) const
{
  if (freq == -1)
  {
    lblFmFrequency->hide();
    fmLabel->hide();
  }
  else
  {
    fmLabel->show();
    lblFmFrequency->show();
    QString f = QString::number(freq);
    f.append(" Khz");
    lblFmFrequency->setText(f);
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
  lblAudioRate->setText(QString::number(iSampleRate / 1000));
  QString afs;
  afs += (iSbrUsed ? "YES / " : "NO / ");
  afs += (iPsUsed ? "YES" : "NO");
  lblSbrUsed->setText(afs);
}

