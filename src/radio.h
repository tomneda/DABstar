/*
 *    Copyright (C) 2013, 2014, 2015, 2016, 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB (formerly SDR-J, JSDR).
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
#ifndef RADIO_H
#define RADIO_H

#include  "dab-constants.h"
#include  <QMainWindow>
#include  <QStringList>
#include  <QStandardItemModel>
#include  <set>

#ifdef  _SEND_DATAGRAM_
#include	<QUdpSocket>
#endif

#include  <QVector>
#include  <QComboBox>
#include  <QByteArray>
#include  <QLabel>
#include  <QTimer>
#include  <sndfile.h>
#include  "ui_dabradio.h"
#include  "dab-processor.h"
#include  "ringbuffer.h"
#include  "band-handler.h"
#include  "process-params.h"
#include  "dl-cache.h"
#include  "tii-codes.h"
#include  "content-table.h"
#include  <memory>
#include  <mutex>

#ifdef  DATA_STREAMER
#include	"tcp-server.h"
#endif

#include  "preset-handler.h"
#include  "epgdec.h"
#include  "epg-decoder.h"

#include  "spectrum-viewer.h"
//#include  "snr-viewer.h"
#include  "findfilenames.h"
#include  "http-handler.h"

class QSettings;
class deviceHandler;
class AudioBase;
class historyHandler;
class timeTableHandler;
class audioDisplay;

#ifdef  HAVE_PLUTO_RXTX
class	dabStreamer;
#endif

class TechData;

#include  "ui_config-helper.h"

/*
 *	The main gui object. It inherits from
 *	QWidget and the generated form
 */

class dabService
{
public:
  QString channel;
  QString serviceName;
  uint32_t SId;
  int SCIds;
  int subChId;
  bool valid;
  bool is_audio;
  FILE * fd;
  FILE * frameDumper;
};

struct theTime
{
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

class ChannelDescriptor
{
public:
  QString channelName;
  bool realChannel;
  bool etiActive;
  int serviceCount;
  int frequency;
  QString ensembleName;
  uint8_t mainId;
  uint8_t subId;
  std::vector<dabService> backgroundServices;
  dabService currentService;
  dabService nextService;
  uint32_t Eid;
  bool has_ecc;
  uint8_t ecc_byte;
  bool tiiFile;
  QString transmitterName;
  QString countryName;
  int nrTransmitters;
  cmplx localPos;
  cmplx targetPos;
  int snr;
  //QByteArray transmitters;
  union STiiId
  {
    STiiId(int mainId, int subId) : MainId(mainId & 0x7F), SubId(subId & 0xFF) {};
    STiiId(int fullId) : FullId(fullId) {};
    uint16_t FullId; struct { uint8_t MainId, SubId; };
  };
  std::set<uint16_t> transmitters;
  void cleanChannel()
  {
    realChannel = true;
    serviceCount = -1;
    frequency = -1;
    ensembleName = "";
    nrTransmitters = 0;
    countryName = "";
    targetPos = cmplx(0, 0);
    mainId = 0;
    subId = 0;
    Eid = 0;
    has_ecc = false;
    snr = 0;
    transmitters.clear();
    currentService.frameDumper = nullptr;
    nextService.frameDumper = nullptr;
  }
};

class RadioInterface : public QWidget, private Ui_dabradio
{
Q_OBJECT
public:
  RadioInterface(QSettings *, const QString &, const QString &, bool, int32_t dataPort, int32_t clockPort, int, QWidget * parent);
  ~RadioInterface();

protected:
  bool eventFilter(QObject * obj, QEvent * event) override;
  
private:
  FILE * dlTextFile;
  RingBuffer<cmplx> spectrumBuffer;
  RingBuffer<cmplx> iqBuffer;
  RingBuffer<float> carrBuffer;
  RingBuffer<float> responseBuffer;
  RingBuffer<uint8_t> frameBuffer;
  RingBuffer<uint8_t> dataBuffer;
  RingBuffer<int16_t> audioBuffer;
  SpectrumViewer my_spectrumViewer;
  PresetHandler my_presetHandler;
  bandHandler theBand;
  QFrame dataDisplay;
  QFrame configDisplay;
  dlCache the_dlCache;
  TiiHandler tiiHandler;
  FindFileNames filenameFinder;
  RingBuffer<int16_t> theTechData;
  httpHandler * mapHandler;
  ProcessParams globals;
  QString version;
  QString theFont;
  int fontSize;
  int fmFrequency;
  contentTable * my_contentTable;
  contentTable * my_scanTable;
  FILE * logFile;
  ChannelDescriptor channel;
  int maxDistance;
  void LOG(const QString &, const QString &);
  bool error_report;
  TechData * theTechWindow;
  Ui_configWidget configWidget;
  QSettings * dabSettings;
  int16_t tii_delay;
  int32_t dataPort;
  bool stereoSetting;
  std::atomic<bool> running;
  std::atomic<bool> scanning;
  deviceHandler * inputDevice;
#ifdef  HAVE_PLUTO_RXTX
  dabStreamer		*streamerOut;
#endif
  DabProcessor * my_dabProcessor;
  AudioBase * soundOut;
#ifdef  DATA_STREAMER
  tcpServer		*dataStreamer;
#endif
#ifdef  CLOCK_STREAMER
  tcpServer		*clockStreamer;
#endif
  CEPGDecoder epgHandler;
  epgDecoder epgProcessor;
  QString epgPath;
  QTimer epgTimer;
  uint32_t extract_epg(QString, std::vector<serviceId> & serviceList, uint32_t);
  bool saveSlides;
  QString picturesPath;
  QString filePath;
#ifdef  _SEND_DATAGRAM_
  QUdpSocket		dataOut_socket;
  QString			ipAddress;
  int32_t			port;
#endif
  SNDFILE * rawDumper;
  SNDFILE * audioDumper;
  FILE * scanDumpFile;
  void set_channelButton(int);
  QStandardItemModel model;
  std::vector<serviceId> serviceList;
  bool isMember(const std::vector<serviceId> &, serviceId);
  std::vector<serviceId> insert(const std::vector<serviceId> &, serviceId, int);

  void show_pause_slide();
  QTimer displayTimer;
  QTimer channelTimer;
  QTimer presetTimer;
  bool mutingActive = false;
  int32_t numberofSeconds;
  int16_t ficBlocks;
  int16_t ficSuccess;
  int total_ficError;
  int total_fics;
  void connectGUI();
  void disconnectGUI();

  int serviceCount;
  struct theTime localTime;
  struct theTime UTC;
  QString convertTime(int, int, int, int, int);
  QString footText();
  QString presetText();
  void cleanScreen();
  void hideButtons();
  void showButtons();
  deviceHandler * setDevice(const QString &);
  historyHandler * my_history;
  historyHandler * my_presets;
  timeTableHandler * my_timeTable;

  void start_etiHandler();
  void stop_etiHandler();
  QString checkDir(const QString);
  //
  void startAudioservice(Audiodata *);
  void startPacketservice(const QString &);
  void startScanning();
  void stopScanning(bool);
  void startAudiodumping();
  void stopAudiodumping();
  FILE * ficDumpPointer;

  void startSourcedumping();
  void stopSourcedumping();
  void startFramedumping();
  void stopFramedumping();
  void startChannel(const QString &);
  void stopChannel();
  void stopService(dabService &);
  void startService(dabService &);
  void colorService(QModelIndex ind, QColor c, int pt, bool italic = false);
  void localSelect(const QString & s);
  void localSelect(const QString &, const QString &);
  void showServices();

  bool doStart();
  void save_MOTObject(QByteArray &, QString);

  void save_MOTtext(QByteArray &, int, QString);
  void show_MOTlabel(QByteArray & data, int contentType, const QString & pictureName, int dirs);

  enum direction
  {
    FORWARD, BACKWARDS
  };

  void handle_serviceButton(direction);
  void hide_for_safety();
  void show_for_safety();
  //
  //	short hands
  void new_presetIndex(int);
  void new_channelIndex(int);

  std::mutex locker;
  bool transmitterTags_local;
  void colorServiceName(const QString & s, QColor color, int fS, bool);
  void write_warning_message(const QString & iMsg);
  void write_picture(const QPixmap & iPixMap) const;

signals:
  void signal_set_new_channel(int);
  void signal_set_new_preset_index(int);

public slots:
  void slot_set_corrector_display(int);
  void slot_add_to_ensemble(const QString &, int);
  void slot_name_of_ensemble(int, const QString &);
  void slot_show_frame_errors(int);
  void slot_show_rs_errors(int);
  void slot_show_aac_errors(int);
  void slot_show_fic_success(bool);
  //void slot_show_snr(int);
  void slot_set_synced(bool);
  void slot_show_label(const QString &);
  void slot_handle_mot_object(QByteArray, QString, int, bool);
  void slot_send_datagram(int);
  void slot_handle_tdc_data(int, int);
  void slot_change_in_configuration();
  void slot_new_audio(int, int);
  void slot_set_stereo(bool);
  void slot_set_stream_selector(int);
  void slot_no_signal_found();
  void slot_show_mot_handling(bool);
  void slot_set_sync_lost();
  void slot_show_correlation(int amount, int marker, const QVector<int> & v);
  void slot_show_spectrum(int);
  void slot_show_iq(int, float);
  void slot_show_mod_quality_data(const OfdmDecoder::SQualityData *);
  void slot_show_rs_corrections(int, int);
  void slot_show_tii(int, int);
  //void slot_show_tii_spectrum();
  void slot_clock_time(int, int, int, int, int, int, int, int, int);
  void slot_start_announcement(const QString &, int);
  void slot_stop_announcement(const QString &, int);
  void slot_new_frame(int);
  void slot_show_clock_error(int);
  void slot_set_epg_data(int, int, const QString &, const QString &);
  void slot_epg_timer_timeout();
  void slot_switch_visibility(QWidget *);
  void slot_nr_services(int);
  void slot_handle_preset_selector(const QString &);
  void slot_handle_content_selector(const QString &);
  void slot_http_terminate();

  void closeEvent(QCloseEvent * event) override;

private slots:
  void _slot_handle_time_table();
  void _slot_handle_content_button();
  void _slot_handle_detail_button();
  void _slot_handle_reset_button();
  void _slot_handle_scan_button();
  void _slot_handle_eti_handler();

  //void _slot_handle_tii_button();
  //void _slot_handle_snr_button();
  void _slot_handle_spectrum_button();
  void _slot_handle_device_widget_button();

  void _slot_do_start(const QString &);
  void _slot_new_device(const QString &);

  void _slot_handle_history_button();
  void _slot_handle_source_dump_button();
  void _slot_handle_frame_dump_button();
  void _slot_handle_audio_dump_button();

  void _slot_handle_prev_service_button();
  void _slot_handle_next_service_button();
  void _slot_handle_channel_selector(const QString &);
  void _slot_handle_next_channel_button();
  void _slot_handle_prev_channel_button();

  void _slot_handle_history_select(const QString &);
  void _slot_terminate_process();
  void _slot_update_time_display();
  void _slot_channel_timeout();

  void _slot_select_service(QModelIndex);
  void _slot_set_preset_service();
  void _slot_handle_mute_button();
  void _slot_handle_dl_text_button();

  void _slot_handle_config_button();
  void _slot_handle_http_button();
  void _slot_handle_on_top(int);
  void _slot_handle_auto_browser(int);
  void _slot_handle_transmitter_tags(int);

  //	config handlers
  void _slot_handle_switch_delay_setting(int);
  void _slot_handle_order_alfabetical();
  void _slot_handle_order_service_ids();
  void _slot_handle_order_sub_channel_ids();
  void _slot_handle_scan_mode_selector(int);
  void _slot_handle_save_service_selector(int);
  void _slot_handle_skip_list_button();
  void _slot_handle_skip_file_button();
  void _slot_handle_tii_detector_mode(int);
  void _slot_handle_logger_button(int);
  void _slot_handle_set_coordinates_button();
  void _slot_handle_port_selector();
  void _slot_handle_transm_selector(int);
  void _slot_handle_eti_active_selector(int);
  void _slot_handle_save_slides(int);
  void _slot_load_table();
};

#endif
