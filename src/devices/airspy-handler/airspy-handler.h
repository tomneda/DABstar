/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/**
 *  IW0HDV Extio
 *
 *  Copyright 2015 by Andrea Montefusco IW0HDV
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 * @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
 *
 *	recoding and taking parts for the airspyRadio interface
 *	for the Qt-DAB program
 *	jan van Katwijk
 *	Lazy Chair Computing
 */
#ifndef AIRSPY_HANDLER_H
#define AIRSPY_HANDLER_H

#include  <QObject>
#include  <QSettings>
#include  <QFrame>
#include  <QLibrary>
#include  <vector>
#include  <atomic>
#include  "dab-constants.h"
#include  "ringbuffer.h"
#include  "fir-filters.h"
#include  "device-handler.h"
#include  "ui_airspy-widget.h"
#include  "libairspy/airspy.h"

class XmlFileWriter;


extern "C" {
typedef i32 (* pfn_airspy_init)();
typedef i32 (* pfn_airspy_exit)();
typedef i32 (* pfn_airspy_list_devices)(u64 *, i32);
typedef i32 (* pfn_airspy_open)(struct airspy_device **, u64);
// typedef i32 (*pfn_airspy_open) (struct airspy_device**);
typedef i32 (* pfn_airspy_close)(struct airspy_device *);
typedef i32 (* pfn_airspy_get_samplerates)(struct airspy_device * device, u32 * buffer, const u32 len);
typedef i32 (* pfn_airspy_set_samplerate)(struct airspy_device * device, u32 samplerate);
typedef i32 (* pfn_airspy_start_rx)(struct airspy_device * device, airspy_sample_block_cb_fn callback, void * rx_ctx);
typedef i32 (* pfn_airspy_stop_rx)(struct airspy_device * device);
typedef i32 (* pfn_airspy_set_sample_type)(struct airspy_device *, airspy_sample_type);
typedef i32 (* pfn_airspy_set_freq)(struct airspy_device * device, const u32 freq_hz);
typedef i32 (* pfn_airspy_set_lna_gain)(struct airspy_device * device, u8 value);
typedef i32 (* pfn_airspy_set_mixer_gain)(struct airspy_device * device, u8 value);
typedef i32 (* pfn_airspy_set_vga_gain)(struct airspy_device * device, u8 value);
typedef i32 (* pfn_airspy_set_lna_agc)(struct airspy_device * device, u8 value);
typedef i32 (* pfn_airspy_set_mixer_agc)(struct airspy_device * device, u8 value);
typedef i32 (* pfn_airspy_set_rf_bias)(struct airspy_device * dev, u8 value);
typedef const char * (* pfn_airspy_error_name)(enum airspy_error errcode);
typedef i32 (* pfn_airspy_board_id_read)(struct airspy_device *, u8 *);
typedef const char * (* pfn_airspy_board_id_name)(enum airspy_board_id board_id);
typedef i32 (* pfn_airspy_board_partid_serialno_read)(struct airspy_device * device, airspy_read_partid_serialno_t * read_partid_serialno);
typedef i32 (* pfn_airspy_set_linearity_gain)(struct airspy_device * device, u8 value);
typedef i32 (* pfn_airspy_set_sensitivity_gain)(struct airspy_device * device, u8 value);
}

class AirspyHandler final : public QObject, public IDeviceHandler, public Ui_airspyWidget
{
Q_OBJECT
public:
  AirspyHandler(QSettings *, QString);
  ~AirspyHandler() override;

  void setVFOFrequency(i32 nf) override;
  i32 getVFOFrequency() override;
  bool restartReader(i32) override;
  void stopReader() override;
  i32 getSamples(cf32 * v, i32 size) override;
  i32 Samples() override;
  void resetBuffer() override;
  i16 bitDepth() override;
  void show() override;
  void hide() override;
  bool isHidden() override;
  QString deviceName() override;
  bool isFileInput() override;

  i32 defaultFrequency();
  i32 getBufferSpace();

  i16 currentTab;

private:
  QFrame myFrame;
  QLibrary * phandle;
  RingBuffer<cf32> _I_Buffer;
  QString recorderVersion;
  FILE * xmlDumper;
  XmlFileWriter * xmlWriter;
  bool setup_xmlDump();
  void close_xmlDump();
  std::atomic<bool> dumping;
  i32 vfoFrequency;
  void record_gainSettings(i32, i32);
  void restore_gainSliders(i32, i32);
  void restore_gainSettings(i32);
  LowPassFIR * theFilter;
  bool filtering;
  i32 currentDepth;
private slots:
  void set_linearity(i32 value);
  void set_sensitivity(i32 value);
  void set_lna_gain(i32 value);
  void set_mixer_gain(i32 value);
  void set_vga_gain(i32 value);
  void set_lna_agc(i32);
  void set_mixer_agc(i32);
  void set_rf_bias(i32);
  void switch_tab(i32);
  void set_xmlDump();
  void set_filter(i32);
signals:
  void new_tabSetting(i32);
private:
  bool load_airspyFunctions();
  //	The functions to be extracted from the dll/.so file
  pfn_airspy_init my_airspy_init;
  pfn_airspy_exit my_airspy_exit;
  pfn_airspy_error_name my_airspy_error_name;
  pfn_airspy_list_devices my_airspy_list_devices;
  pfn_airspy_open my_airspy_open;
  pfn_airspy_close my_airspy_close;
  pfn_airspy_get_samplerates my_airspy_get_samplerates;
  pfn_airspy_set_samplerate my_airspy_set_samplerate;
  pfn_airspy_start_rx my_airspy_start_rx;
  pfn_airspy_stop_rx my_airspy_stop_rx;
  pfn_airspy_set_sample_type my_airspy_set_sample_type;
  pfn_airspy_set_freq my_airspy_set_freq;
  pfn_airspy_set_lna_gain my_airspy_set_lna_gain;
  pfn_airspy_set_mixer_gain my_airspy_set_mixer_gain;
  pfn_airspy_set_vga_gain my_airspy_set_vga_gain;
  pfn_airspy_set_linearity_gain my_airspy_set_linearity_gain;
  pfn_airspy_set_sensitivity_gain my_airspy_set_sensitivity_gain;
  pfn_airspy_set_lna_agc my_airspy_set_lna_agc;
  pfn_airspy_set_mixer_agc my_airspy_set_mixer_agc;
  pfn_airspy_set_rf_bias my_airspy_set_rf_bias;
  pfn_airspy_board_id_read my_airspy_board_id_read;
  pfn_airspy_board_id_name my_airspy_board_id_name;
  pfn_airspy_board_partid_serialno_read my_airspy_board_partid_serialno_read;

  bool success;
  std::atomic<bool> running;
  bool lna_agc;
  bool mixer_agc;
  bool rf_bias;
  const char * board_id_name();

  i16 vgaGain;
  i16 mixerGain;
  i16 lnaGain;
  i32 selectedRate;
  i16 convBufferSize;
  i16 convIndex;
  std::vector<cf32> convBuffer;
  i16 mapTable_int[4 * 512];
  f32 mapTable_float[4 * 512];
  QSettings * airspySettings;
  i32 inputRate;
  struct airspy_device * device;
  u64 serialNumber;
  char serial[128];
  static i32 callback(airspy_transfer_t *);
  i32 data_available(void * buf, i32 buf_size);
  const char * getSerial();
  i32 open();
};

#endif
