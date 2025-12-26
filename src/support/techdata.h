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


#ifndef  TECH_DATA_H
#define  TECH_DATA_H

#include  "ui_techdata.h"
#include  "dab-constants.h"
#include  "audio-display.h"
#include  "ringbuffer.h"
#include  "custom_frame.h"
#include  <QObject>
#include  <QTimer>

class DabRadio;
class QSettings;

class TechData : public QObject, public Ui_technical_data
{
Q_OBJECT
public:
  TechData(DabRadio *, RingBuffer<i16> * ipAudioBuffer);
  ~TechData() override;

  void show_service_data(const SAudioData * ad) const;
  void show_service_data_addon(const SAudioDataAddOns * adon) const;
  void cleanUp();
  void show();
  void hide();
  bool isHidden() const;

private:
  DabRadio * const mpRadioInterface;
  RingBuffer<i16> * mpAudioBuffer;
  CustomFrame mFrame;
  AudioDisplay * mpAudioDisplay = nullptr;
  QTimer mTimerMotReceived; // avoid fast flickering of the MOT indicator

  void _show_service_label(const QString &) const;
  void _show_SId(u32) const;
  void _show_bitrate(i32) const;
  void _show_subChId(i32) const;
  void _show_CU_start_address(i32) const;
  void _show_CU_size(i32) const;
  void _show_ASCTy(i32) const;
  void _show_uep_eep(i32, i32) const;
  void _show_coderate(i32, i32) const;

public slots:
  void slot_trigger_motHandling();
  void slot_show_frame_error_bar(i32) const;
  void slot_show_aac_error_bar(i32) const;
  void slot_show_rs_error_bar(i32) const;
  void slot_show_rs_corrections(i32, i32) const;
  void slot_show_timetableButton(bool) const;
  void slot_show_language(i32) const;
  void slot_show_fm(i32) const;
  void slot_show_sample_rate_and_audio_flags(i32 iSampleRate, bool iSbrUsed, bool iPsUsed) const;

  void slot_audio_data_available(i32, i32) const;

private slots:
  void _slot_show_motHandling(bool) const;

signals:
  void signal_handle_timeTable();
  void signal_handle_audioDumping();
  void signal_handle_frameDumping();
  void signal_window_closed();
};


#endif
