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

#include  "ui_technical_data.h"
#include  "dab-constants.h"
#include  "audio-display.h"
#include  "ringbuffer.h"
#include  "custom_frame.h"
#include  <QObject>
#include  <QFrame>
#include  <QTimer>

class DabRadio;
class QSettings;

class TechData : public QObject, public Ui_technical_data
{
Q_OBJECT
public:
  TechData(DabRadio *, RingBuffer<int16_t> * ipAudioBuffer);
  ~TechData() override;

  void show_serviceData(const Audiodata * ad);
  void cleanUp();
  void show();
  void hide();
  bool isHidden();

private:
  DabRadio * const mpRadioInterface;
  RingBuffer<int16_t> * mpAudioBuffer;
  CustomFrame mFrame;
  AudioDisplay * mpAudioDisplay = nullptr;
  QTimer mTimerMotReceived; // avoid fast flickering of the MOT indicator

public slots:
  void slot_trigger_motHandling();
  void slot_show_frame_error_bar(int);
  void slot_show_aac_error_bar(int);
  void slot_show_rs_error_bar(int);
  void slot_show_rs_corrections(int, int);
  void slot_show_timetableButton(bool);
  void slot_show_frameDumpButton(bool);
  void slot_show_serviceName(const QString &);
  void slot_show_serviceId(int);
  void slot_show_bitRate(int);
  void slot_show_subChId(int);
  void slot_show_startAddress(int);
  void slot_show_length(int);
  void slot_show_language(int);
  void slot_show_ASCTy(int);
  void slot_show_uep(int, int);
  void slot_show_codeRate(int, int);
  void slot_show_fm(int);
  void slot_show_sample_rate_and_audio_flags(int32_t iSampleRate, bool iSbrUsed, bool iPsUsed);

  void slot_hide_missed();
  void slot_show_missed(int);
  void slot_audio_data_available(int, int);

  void slot_frame_dump_button_text(const QString & s, int);
  void slot_audio_dump_button_text(const QString & s, int);

private slots:
  void _slot_show_motHandling(bool);

signals:
  void signal_handle_timeTable();
  void signal_handle_audioDumping();
  void signal_handle_frameDumping();
  void signal_window_closed();
};


#endif
