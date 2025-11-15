/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
 *
 *    Many of the ideas as implemented in Qt-DAB are derived from
 *    other work, made available through the GNU general Public License.
 *    All copyrights of the original authors are recognized.
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
 *
 */

#ifndef RTLSDR_HANDLER_H
#define  RTLSDR_HANDLER_H

#include  <QObject>
#include  <QSettings>
#include  <QString>
#include  <cstdio>
#include  <atomic>
#include  <QComboBox>
#include  "dab-constants.h"
#include  "fir-filters.h"
#include  "device-handler.h"
#include  "ringbuffer.h"
#include  "ui_rtlsdr-widget.h"
#include  <QLibrary>

class dll_driver;
class XmlFileWriter;

//	create typedefs for the library functions
typedef struct rtlsdr_dev rtlsdr_dev_t;

extern "C" {
typedef void (* rtlsdr_read_async_cb_t)(u8 * buf, u32 len, void * ctx);
typedef i32 (* pfnrtlsdr_open )(rtlsdr_dev_t **, u32);
typedef i32 (* pfnrtlsdr_close)(rtlsdr_dev_t *);
typedef i32 (* pfnrtlsdr_get_usb_strings)(rtlsdr_dev_t *, char *, char *, char *);
typedef i32 (* pfnrtlsdr_set_center_freq)(rtlsdr_dev_t *, u32);
typedef i32 (* pfnrtlsdr_set_tuner_bandwidth)(rtlsdr_dev_t *, u32);
typedef u32 (* pfnrtlsdr_get_center_freq)(rtlsdr_dev_t *);
typedef i32 (* pfnrtlsdr_get_tuner_gains)(rtlsdr_dev_t *, i32 *);
typedef i32 (* pfnrtlsdr_set_tuner_gain_mode)(rtlsdr_dev_t *, i32);
typedef i32 (* pfnrtlsdr_set_agc_mode)(rtlsdr_dev_t *, i32);
typedef i32 (* pfnrtlsdr_set_sample_rate)(rtlsdr_dev_t *, u32);
typedef i32 (* pfnrtlsdr_get_sample_rate)(rtlsdr_dev_t *);
typedef i32 (* pfnrtlsdr_set_tuner_gain)(rtlsdr_dev_t *, i32);
typedef i32 (* pfnrtlsdr_get_tuner_gain)(rtlsdr_dev_t *);
typedef i32 (* pfnrtlsdr_reset_buffer)(rtlsdr_dev_t *);
typedef i32 (* pfnrtlsdr_read_async)(rtlsdr_dev_t *, rtlsdr_read_async_cb_t, void *, u32, u32);
typedef i32 (* pfnrtlsdr_set_bias_tee)(rtlsdr_dev_t *, i32);
typedef i32 (* pfnrtlsdr_cancel_async)(rtlsdr_dev_t *);
typedef i32 (* pfnrtlsdr_set_direct_sampling)(rtlsdr_dev_t *, i32);
typedef u32 (* pfnrtlsdr_get_device_count)();
typedef i32 (* pfnrtlsdr_set_freq_correction)(rtlsdr_dev_t *, i32);
typedef i32 (* pfnrtlsdr_set_freq_correction_ppb)(rtlsdr_dev_t *, i32);
typedef char * (* pfnrtlsdr_get_device_name)(i32);
typedef i32 (* pfnrtlsdr_get_tuner_i2c_register)(rtlsdr_dev_t *dev, u8* data, i32 *len, i32 *strength);
typedef i32 (* pfnrtlsdr_get_tuner_type)(rtlsdr_dev_t *dev);
}

//	This class is a simple wrapper around the
//	rtlsdr library that is read in  as dll (or .so file in linux)
//	It does not do any processing
class RtlSdrHandler final : public QObject, public IDeviceHandler, public Ui_dabstickWidget
{
Q_OBJECT
public:
  RtlSdrHandler(QSettings * s, const QString & recorderVersion);
  ~RtlSdrHandler() override;
  void setVFOFrequency(i32) override;
  i32 getVFOFrequency() override;
  bool restartReader(i32) override;
  void stopReader() override;
  i32 getSamples(cf32 *, i32) override;
  i32 Samples() override;
  void resetBuffer() override;
  i16 bitDepth() override;
  QString deviceName() override;
  void show() override;
  void hide() override;
  bool isHidden() override;
  bool isFileInput() override;
  i16 maxGain();
  bool detect_overload(u8 *buf, i32 len);

  //	These need to be visible for the separate usb handling thread
  RingBuffer<std::complex<u8>> _I_Buffer;
  pfnrtlsdr_read_async rtlsdr_read_async;
  struct rtlsdr_dev * theDevice;
  std::atomic<bool> isActive;

private:
  QFrame myFrame;
  QSettings * rtlsdrSettings;
  i32 inputRate;
  i32 deviceCount;
  QLibrary * phandle;
  dll_driver * workerHandle;
  i32 lastFrequency;
  i16 gainsCount;
  QString deviceModel;
  QString recorderVersion;
  FILE * xmlDumper;
  XmlFileWriter * xmlWriter;
  bool setup_xmlDump();
  void close_xmlDump();
  std::atomic<bool> xml_dumping;
  FILE * iqDumper;
  bool setup_iqDump();
  void close_iqDump();
  std::atomic<bool> iq_dumping;
  f32 mapTable[256];
  bool filtering;
  LowPassFIR theFilter;
  i32 currentDepth;
  i32 agcControl;
  void set_autogain(i32);
  void enable_gainControl(i32);
  i32 old_overload = 2;
  i32 old_gain = 0;

  //	here we need to load functions from the dll
  bool load_rtlFunctions(bool & oHasNewInterface);
  pfnrtlsdr_open rtlsdr_open;
  pfnrtlsdr_close rtlsdr_close;
  pfnrtlsdr_get_usb_strings rtlsdr_get_usb_strings;
  pfnrtlsdr_set_center_freq rtlsdr_set_center_freq;
  pfnrtlsdr_set_tuner_bandwidth rtlsdr_set_tuner_bandwidth;
  pfnrtlsdr_get_center_freq rtlsdr_get_center_freq;
  pfnrtlsdr_get_tuner_gains rtlsdr_get_tuner_gains;
  pfnrtlsdr_set_tuner_gain_mode rtlsdr_set_tuner_gain_mode;
  pfnrtlsdr_set_agc_mode rtlsdr_set_agc_mode;
  pfnrtlsdr_set_sample_rate rtlsdr_set_sample_rate;
  pfnrtlsdr_get_sample_rate rtlsdr_get_sample_rate;
  pfnrtlsdr_set_tuner_gain rtlsdr_set_tuner_gain;
  pfnrtlsdr_get_tuner_gain rtlsdr_get_tuner_gain;
  pfnrtlsdr_reset_buffer rtlsdr_reset_buffer;
  pfnrtlsdr_cancel_async rtlsdr_cancel_async;
  pfnrtlsdr_set_bias_tee rtlsdr_set_bias_tee;
  pfnrtlsdr_set_direct_sampling rtlsdr_set_direct_sampling;
  pfnrtlsdr_get_device_count rtlsdr_get_device_count;
  pfnrtlsdr_set_freq_correction rtlsdr_set_freq_correction;
  pfnrtlsdr_set_freq_correction_ppb rtlsdr_set_freq_correction_ppb;
  pfnrtlsdr_get_device_name rtlsdr_get_device_name;
  pfnrtlsdr_get_tuner_i2c_register rtlsdr_get_tuner_i2c_register;
  pfnrtlsdr_get_tuner_type rtlsdr_get_tuner_type;

private slots:
  void set_ExternalGain(i32);
  void set_ppmCorrection(f64);
  void set_bandwidth(i32);
  void set_xmlDump();
  void set_iqDump();
  void set_filter(i32);
  void set_biasControl(i32);
  void handle_hw_agc();
  void handle_sw_agc();
  void handle_manual();
  void slot_timer(i32);

signals:
  void signal_timer(i32);
};

#endif

