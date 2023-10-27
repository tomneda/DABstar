/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2017 .. 2018
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    Copyright (C) 2019 amplifier, antenna and ppm corectors
 *    Fabio Capozzi
 *
 *    This file is part of the Qt-DAB program
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

#ifndef HACKRF_HANDLER_H
#define HACKRF_HANDLER_H

#include  <QObject>
#include  <QFrame>
#include  <QSettings>
#include  <atomic>
#include  "dab-constants.h"
#include  "ringbuffer.h"
#include  "device-handler.h"
#include  "ui_hackrf-widget.h"
#include  "libhackrf/hackrf.h"
#include  <QLibrary>

using hackrf_sample_block_cb_fn = int (*)(hackrf_transfer * transfer);

#ifdef __MINGW32__
#define GETPROCADDRESS  GetProcAddress
#else
#define GETPROCADDRESS  dlsym
#endif

class xml_fileWriter;

using pfn_hackrf_init = int (*)();
using pfn_hackrf_open = int (*)(hackrf_device ** device);
using pfn_hackrf_close = int (*)(hackrf_device * device);
using pfn_hackrf_exit = int (*)();
using pfn_hackrf_start_rx = int (*)(hackrf_device *, hackrf_sample_block_cb_fn, void *);
using pfn_hackrf_stop_rx = int (*)(hackrf_device *);
using pfn_hackrf_device_list = hackrf_device_list_t * (*)();
using pfn_hackrf_set_baseband_filter_bandwidth = int (*)(hackrf_device *, const uint32_t bandwidth_hz);
using pfn_hackrf_set_lna_gain = int (*)(hackrf_device *, uint32_t);
using pfn_hackrf_set_vga_gain = int (*)(hackrf_device *, uint32_t);
using pfn_hackrf_set_freq = int (*)(hackrf_device *, const uint64_t);
using pfn_hackrf_set_sample_rate = int (*)(hackrf_device *, const double freq_hz);
using pfn_hackrf_is_streaming = int (*)(hackrf_device *);
using pfn_hackrf_error_name = const char * (*)(enum hackrf_error errcode);
using pfn_hackrf_usb_board_id_name = const char * (*)(enum hackrf_usb_board_id);
using pfn_hackrf_set_antenna_enable = int (*)(hackrf_device *, const uint8_t);
using pfn_hackrf_set_amp_enable = int (*)(hackrf_device *, const uint8_t);
using pfn_hackrf_si5351c_read = int (*)(hackrf_device *, const uint16_t, uint16_t *);
using pfn_hackrf_si5351c_write = int (*)(hackrf_device *, const uint16_t, const uint16_t);


///////////////////////////////////////////////////////////////////////////
class HackRfHandler : public QObject, public deviceHandler, public Ui_hackrfWidget
{
Q_OBJECT
public:
  HackRfHandler(QSettings * iSetting, const QString & iRecorderVersion);
  ~HackRfHandler() final;

  void setVFOFrequency(int32_t) final;
  int32_t getVFOFrequency() final;

  bool restartReader(int32_t) final;
  void stopReader() final;
  int32_t getSamples(cmplx *, int32_t) final;
  int32_t Samples() final;
  void resetBuffer() final;
  int16_t bitDepth() final;
  void show() final;
  void hide() final;
  bool isHidden() final;
  QString deviceName() final;

  using TRingBuffer = RingBuffer<std::complex<int8_t> >;
  TRingBuffer mRingBuffer{ 4 * 1024 * 1024 }; // The buffer should be visible by the callback function

private:
  struct SHackRf
  {
    pfn_hackrf_init init;
    pfn_hackrf_open open;
    pfn_hackrf_close close;
    pfn_hackrf_exit exit;
    pfn_hackrf_start_rx start_rx;
    pfn_hackrf_stop_rx stop_rx;
    pfn_hackrf_device_list device_list;
    pfn_hackrf_set_baseband_filter_bandwidth set_baseband_filter_bandwidth;
    pfn_hackrf_set_lna_gain set_lna_gain;
    pfn_hackrf_set_vga_gain set_vga_gain;
    pfn_hackrf_set_freq set_freq;
    pfn_hackrf_set_sample_rate set_sample_rate;
    pfn_hackrf_is_streaming is_streaming;
    pfn_hackrf_error_name error_name;
    pfn_hackrf_usb_board_id_name usb_board_id_name;
    pfn_hackrf_set_antenna_enable set_antenna_enable;
    pfn_hackrf_set_amp_enable set_amp_enable;
    pfn_hackrf_si5351c_read si5351c_read;
    pfn_hackrf_si5351c_write si5351c_write;
  };

  hackrf_device * theDevice;
  QFrame myFrame{ nullptr };
  SHackRf mHackrf;
  QSettings * mpHackrfSettings;
  QString mRecorderVersion;
  int32_t mVfoFrequency;
  std::atomic<bool> mRunning;
  QLibrary * mpHandle;
  FILE * mpXmlDumper;
  xml_fileWriter * mpXmlWriter;
  std::atomic<bool> mDumping;
  bool save_gain_settings;

  bool load_hackrf_functions();
  bool setup_xml_dump();
  void close_xml_dump();
  void record_gain_settings(int);
  void update_gain_settings(int);
  void check_err_throw(int32_t iResult) const;
  bool check_err(int32_t iResult, const char * iFncName, uint32_t iLine) const;

signals:
  void signal_new_ant_enable(bool);
  void signal_new_amp_enable(bool);
  void signal_new_vga_value(int);
  void signal_new_lna_value(int);

private slots:
  void slot_set_lna_gain(int);
  void slot_set_vga_gain(int);
  void slot_enable_antenna(int);
  void slot_enable_amp(int);
  void slot_set_ppm_correction(int);
  void slot_xml_dump();
};

#endif
