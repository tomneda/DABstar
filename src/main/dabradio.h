/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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
#ifndef DABRADIO_H
#define DABRADIO_H

#include  "dab-constants.h"
#include  "dabradio.h"
#include  "dab-processor.h"
#include  "ringbuffer.h"
#include  "band-handler.h"
#include  "process-params.h"
#include  "dl-cache.h"
#include  "tii-codes.h"
#include  "content-table.h"

#ifdef  DATA_STREAMER
#include	"tcp-server.h"
#endif

#include "epgdec.h"
#include "epg-decoder.h"
#include "spectrum-viewer.h"
#include "cir-viewer.h"
#include "openfiledialog.h"
#include "http-handler.h"
#include "device-selector.h"
#include "configuration.h"
#include "wav_writer.h"
#include "audiofifo.h"
#include "tii_list_display.h"
#include <set>
#include <memory>
#include <mutex>
#include <QMainWindow>
#include <QStringList>
#include <QStandardItemModel>
#include <QVector>
#include <QComboBox>
#include <QByteArray>
#include <QLabel>
#include <QTimer>
#include <QAudioDevice>
#include <sndfile.h>

class QSettings;
class Qt_Audio;
class timeTableHandler;
class ServiceListHandler;
class IAudioOutput;


#ifdef  HAVE_PLUTO_RXTX
class	dabStreamer;
#endif

class TechData;

struct SDabService
{
  QString channel;
  QString serviceName;
  u32 SId = 0;
  i32 SCIdS = 0;
  i32 subChId = 0;
  bool valid = false;
  bool is_audio = false;
  // FILE * fd = nullptr;
  FILE * frameDumper = nullptr;
};

struct STheTime
{
  i32 year;
  i32 month;
  i32 day;
  i32 hour;
  i32 minute;
  i32 second;
};

struct SChannelDescriptor
{
  QString channelName;
  bool realChannel = false;
  bool etiActive = false;
  i32 serviceCount = 0;
  i32 nominalFreqHz = 0;
  QString ensembleName;
  u8 mainId = 0;
  u8 subId = 0;
  std::vector<SDabService> backgroundServices;
  SDabService currentService{};
  SDabService nextService{};
  u32 Eid = 0;
  bool ecc_checked = false;
  u8 ecc_byte = 0;
  bool tiiFile = false;
  QString transmitterName;
  QString countryName;
  i32 nrTransmitters = 0;
  cf32 localPos{};
  cf32 targetPos{};
  i32 snr = 0;
  std::set<u16> transmitters;

  union UTiiId
  {
    UTiiId(i32 mainId, i32 subId) : MainId(mainId & 0x7F), SubId(subId & 0xFF) {};
    explicit UTiiId(i32 fullId) : FullId(fullId) {};

    u16 FullId = 0;
    struct
    {
      u8 MainId;
      u8 SubId;
    };
  };

  void clean_channel()
  {
    serviceCount = -1;
    nominalFreqHz = -1;
    ensembleName = "";
    nrTransmitters = 0;
    countryName = "";
    targetPos = cf32(0, 0);
    mainId = 0;
    subId = 0;
    Eid = 0;
    ecc_checked = false;
    snr = 0;
    transmitters.clear();
    currentService.frameDumper = nullptr;
    nextService.frameDumper = nullptr;
  }
};

class Ui_DabRadio;

class DabRadio : public QWidget
{
Q_OBJECT
public:
  DabRadio(QSettings *, const QString &, const QString &, i32 iDataPort, QWidget * iParent);
  ~DabRadio() override;

  enum EAudioFlags : u32
  {
    AFL_NONE     = 0x0,
    AFL_SBR_USED = 0x1,
    AFL_PS_USED  = 0x2
  };

  [[nodiscard]] TechData * get_techdata_widget() const { return mpTechDataWidget.get(); }

private:
  template<typename T>
  struct StatusInfoElem
  {
    T value{};
    QColor colorOn;
    QColor colorOff;
    QLabel * pLbl = nullptr;
  };

  struct StatusInfo
  {
    StatusInfoElem<bool>     Stereo;
    StatusInfoElem<bool>     EPG;
    StatusInfoElem<bool>     SBR;
    StatusInfoElem<bool>     PS;
    StatusInfoElem<bool>     Announce;
    StatusInfoElem<bool>     RsError;
    StatusInfoElem<bool>     CrcError;
    StatusInfoElem<i32>  InpBitRate;  // tricky: bit rates must be of type i32
    StatusInfoElem<u32> OutSampRate; // tricky: sample rates must be of type u32
  };

  static constexpr i32 cDisplayTimeoutMs     = 1000;
  static constexpr i32 cChannelTimeoutMs     = 5000;
  static constexpr i32 cEpgTimeoutMs         = 3000;
  static constexpr i32 cPresetTimeoutMs      =  500;
  static constexpr i32 cClockResetTimeoutMs  = 5000;
  static constexpr i32 cTiiIndexCntTimeoutMs = 2000;

  Ui_DabRadio * const ui;

  i32 mAudioFrameCnt = 0;
  i32 mMotObjectCnt = 0;
  StatusInfo mStatusInfo{};
  FILE * mDlTextFile = nullptr;
  RingBuffer<cf32> * const mpSpectrumBuffer;
  RingBuffer<cf32> * const mpIqBuffer;
  RingBuffer<f32> * const mpCarrBuffer;
  RingBuffer<f32> * const mpResponseBuffer;
  RingBuffer<u8> * const mpFrameBuffer;
  RingBuffer<u8> * const mpDataBuffer;
  RingBuffer<i16> * const mpAudioBufferFromDecoder;
  RingBuffer<i16> * const mpAudioBufferToOutput;
  RingBuffer<i16> * const mpTechDataBuffer;
  RingBuffer<cf32> * const mpCirBuffer;
  u32 mResetRingBufferCnt = 0;
  std::vector<i16> mAudioTempBuffer;
  SpectrumViewer mSpectrumViewer;
  CirViewer mCirViewer;
  BandHandler mBandHandler;
  DynLinkCache mDynLabelCache{10};
  TiiHandler mTiiHandler{};
  TiiListDisplay mTiiListDisplay{};
  OpenFileDialog mOpenFileDialog;
  HttpHandler * mpHttpHandler = nullptr;
  ProcessParams mProcessParams;
  const QString mVersionStr{PRJ_VERS};
  ContentTable * mpContentTable = nullptr;
  FILE * mpLogFile = nullptr;
  SChannelDescriptor mChannel{};
  i32 mMaxDistance = -1;
  QScopedPointer<TechData> mpTechDataWidget;
  Configuration mConfig;
  std::atomic<bool> mIsRunning{false};
  std::atomic<bool> mIsScanning{false};
  std::unique_ptr<IDeviceHandler> mpInputDevice;
  DeviceSelector mDeviceSelector;
  struct SScanResult
  {
    u32 NrChannels = 0;
    u32 NrAudioServices = 0;
    u32 NrNonAudioServices = 0;
    QString LastChannel;
  };
  SScanResult mScanResult{};
#ifdef  HAVE_PLUTO_RXTX
  dabStreamer * streamerOut = nullptr;
#endif
  QScopedPointer<DabProcessor> mpDabProcessor;
  IAudioOutput * mpAudioOutput; // normal pointer as it is controlled by mAudioOutputThread
  QThread * mAudioOutputThread = nullptr;
  SAudioFifo mAudioFifo{};
  SAudioFifo * mpCurAudioFifo = nullptr;
  enum class EPlaybackState { Stopped, WaitForInit, Running };
  EPlaybackState mPlaybackState = EPlaybackState::Stopped;
  f32 mAudioBufferFillFiltered = 0.0f;
  f32 mPeakLeftDamped = -100.0f;
  f32 mPeakRightDamped = -100.0f;
  bool mProgBarAudioBufferFullColorSet = false;

#ifdef  DATA_STREAMER
  tcpServer * dataStreamer = nullptr;
#endif
  CEPGDecoder mEpgHandler;
  EpgDecoder mEpgProcessor;
  QString mEpgPath;
  QTimer mEpgTimer;
  QString mPicturesPath;
  QString mMotPath;
  SNDFILE * mpRawDumper = nullptr;
  WavWriter mWavWriter;
  enum class EAudioDumpState { Stopped, WaitForInit, Running };
  EAudioDumpState mAudioDumpState = EAudioDumpState::Stopped;
  QString mAudioWavDumpFileName;
  std::vector<SServiceId> mServiceList;

  QTimer mDisplayTimer;
  QTimer mChannelTimer;
  QTimer mPresetTimer;
  QTimer mClockResetTimer;
  QTimer mTiiIndexCntTimer;
  u32 mTiiIndex = 0;
  bool mShowTiiListWindow = false;
  bool mMutingActive = false;
  STheTime mLocalTime{};
  STheTime mUTC{};
  timeTableHandler * mpTimeTable = nullptr;
  FILE * mpFicDumpPointer = nullptr;
  size_t mPreviousIdleTime = 0;
  size_t mPreviousTotalTime = 0;
  QScopedPointer<ServiceListHandler> mpServiceListHandler;
  bool mCurFavoriteState = false;
  bool mClockActiveStyle = true;
  std::mutex mMutex;
  std::vector<STiiResult> mTransmitterIds;
  enum class EAudioFrameType { None, MP2, AAC };
  EAudioFrameType mAudioFrameType = EAudioFrameType::None;
  std::vector<QMetaObject::Connection> mGuiConnections;

  static QStringList get_soft_bit_gen_names();
  std::vector<SServiceId> insert_sorted(const std::vector<SServiceId> &, const SServiceId &);
  void LOG(const QString &, const QString &);
  u32 extract_epg(const QString&, const std::vector<SServiceId> & iServiceList, u32);
  void show_pause_slide();
  void connect_dab_processor();
  void connect_gui();
  void disconnect_gui();
  static QString convertTime(i32, i32, i32, i32, i32, i32 = -1);
  void clean_screen();
  void _show_hide_buttons(const bool iShow);

  void start_etiHandler();
  void stop_etiHandler();
  QString check_and_create_dir(const QString &) const;

  void start_audio_service(const AudioData * const ipAD);
  void start_packet_service(const QString &);
  void start_scanning();
  void stop_scanning();
  void start_audio_dumping();
  void stop_audio_dumping();

  void start_source_dumping();
  void stop_source_dumping();
  void start_frame_dumping();
  void stop_frame_dumping();
  void start_channel(const QString &);
  void stop_channel();
  void clean_up();
  void stop_service(SDabService &);
  void start_service(SDabService &);
  //void colorService(QModelIndex ind, QColor c, i32 pt, bool italic = false);
  void local_select(const QString &, const QString &);
  // void showServices() const;

  bool do_start();

  bool save_MOT_EPG_data(const QByteArray & result, const QString & objectName, i32 contentType);
  void save_MOT_object(const QByteArray &, const QString &);
  void save_MOT_text(const QByteArray &, i32, const QString &);
  void show_MOT_image(const QByteArray & data, i32 contentType, const QString & pictureName, i32 dirs);

  void create_directory(const QString & iDirOrPath, bool iContainsFileName) const;
  QString generate_unique_file_path_from_hash(const QString & iBasePath, const QString & iFileExt, const QByteArray & iData, const bool iStoreAsDir) const;
  QString generate_file_path(const QString & iBasePath, const QString & iFileName, bool iStoreAsDir) const;

  //enum direction { FORWARD, BACKWARDS };
  //void handle_serviceButton(direction);
  void enable_ui_elements_for_safety(bool iEnable);

  //void colorServiceName(const QString & s, QColor color, i32 fS, bool);
  void write_warning_message(const QString & iMsg);
  void write_picture(const QPixmap & iPixMap) const;

  static QString get_bg_style_sheet(const QColor & iBgBaseColor, const char * const ipWidgetType = nullptr);
  void setup_ui_colors();
  void set_favorite_button_style();
  void _update_channel_selector(i32);
  void _show_epg_label(const bool iShowLabel);
  void _set_http_server_button(const bool iActive);
  void _set_clock_text(const QString & iText = QString());
  void _create_status_info();
  template<typename T> void _add_status_label_elem(StatusInfoElem<T> & ioElem, const u32 iColor, const QString & iName, const QString & iToolTip);
  template<typename T> void _set_status_info_status(StatusInfoElem<T> & iElem, const T iValue);
  void _reset_status_info();
  void _update_channel_selector();
  void _set_device_to_file_mode(const bool iDataFromFile);
  void _setup_audio_output(u32 iSampleRate);
  QString _get_scan_message(bool iEndMsg) const;
  QString _convert_links_to_clickable(const QString& iText) const;
  void _check_coordinates() const;
  bool _is_audio_service(const QString & s) const;
  // bool _is_packet_service(const QString & s) const;


signals:
  void signal_set_new_channel(i32);
  void signal_dab_processor_started();
  void signal_test_slider_changed(i32);
  void signal_audio_mute(bool iMuted);

  void signal_start_audio(SAudioFifo *buffer);
  void signal_switch_audio(SAudioFifo *buffer);
  void signal_stop_audio();
  void signal_set_audio_device(const QByteArray & deviceId);
  void signal_audio_buffer_filled_state(i32);

public slots:
  void slot_add_to_ensemble(const QString &, u32);
  void slot_name_of_ensemble(i32, const QString &);
  void slot_show_frame_errors(i32);
  void slot_show_rs_errors(i32);
  void slot_show_aac_errors(i32);
  void slot_show_fic_status(i32 iSuccessPercent, f32 iBER);
  void slot_show_label(const QString &);
  void slot_handle_mot_object(const QByteArray &, const QString &, i32, bool);
  void slot_send_datagram(i32);
  void slot_handle_tdc_data(i32, i32);
  void slot_change_in_configuration();
  void slot_new_audio(i32, u32, u32);
  void slot_set_stereo(bool);
  void slot_set_stream_selector(i32);
  void slot_no_signal_found();
  void slot_show_mot_handling();
  void slot_show_correlation(f32, const QVector<i32> & v);
  void slot_show_spectrum(i32);
  void slot_show_cir();
  void slot_show_iq(i32, f32);
  void slot_show_lcd_data(const OfdmDecoder::SLcdData *);
  void slot_show_digital_peak_level(f32 iPeakLevel);
  void slot_show_rs_corrections(i32, i32);
  void slot_show_tii(const std::vector<STiiResult> & iTr);
  void slot_clock_time(i32, i32, i32, i32, i32, i32, i32, i32, i32);
  void slot_start_announcement(const QString &, i32);
  void slot_stop_announcement(const QString &, i32);
  void slot_new_frame();
  void slot_show_clock_error(f32 e);
  void slot_set_epg_data(i32, i32, const QString &, const QString &);
  void slot_epg_timer_timeout();
  void slot_nr_services(i32);
  void slot_handle_content_selector(const QString &);
  void slot_set_and_show_freq_corr_rf_Hz(i32 iFreqCorrRF);
  void slot_show_freq_corr_bb_Hz(i32 iFreqCorrBB);
  void slot_test_slider(i32);
  void slot_load_table();
  //void slot_handle_transmitter_tags(i32);
  void slot_handle_dl_text_button();
  void slot_handle_logger_button(i32);
  void slot_handle_port_selector();
  void slot_handle_set_coordinates_button();
  void slot_use_strongest_peak(bool);
  void slot_handle_dc_avoidance_algorithm(bool);
  void slot_handle_dc_removal(bool);
  void slot_show_audio_peak_level(const f32 iPeakLeft, const f32 iPeakRight);
  void slot_handle_tii_collisions(bool);
  void slot_handle_tii_threshold(i32);
  void slot_handle_tii_subid(i32);

  void closeEvent(QCloseEvent * event) override;

private slots:
  void _slot_handle_time_table();
  void _slot_handle_content_button();
  void _slot_handle_tech_detail_button();
  void _slot_handle_reset_button();
  void _slot_handle_scan_button();
  void _slot_handle_eti_button();
  void _slot_handle_spectrum_button();
  void _slot_handle_cir_button();
  void _slot_handle_device_widget_button();
  void _slot_do_start(const QString &);
  void _slot_new_device(const QString &);
  void _slot_handle_source_dump_button();
  void _slot_handle_frame_dump_button();
  void _slot_handle_audio_dump_button();
  void _slot_handle_tii_button();
  void _slot_handle_prev_service_button();
  void _slot_handle_next_service_button();
  void _slot_handle_target_service_button();
  void _slot_handle_channel_selector(const QString &);
  void _slot_terminate_process();
  void _slot_update_time_display();
  void _slot_channel_timeout();
  void _slot_select_service(QModelIndex);
  void _slot_service_changed(const QString & iChannel, const QString & iService);
  void _slot_favorite_changed(const bool iIsFav);
  void _slot_handle_favorite_button(bool iClicked);
  void _slot_set_static_button_style();
  void _slot_preset_timeout();
  void _slot_handle_mute_button();
  void _slot_update_mute_state(const bool iMute);
  void _slot_handle_config_button();
  void _slot_handle_http_button();
  void _slot_handle_skip_list_button();
  void _slot_handle_skip_file_button();
  void _slot_load_audio_device_list(const QList<QAudioDevice> & iDeviceList) const;
  void _slot_handle_volume_slider(i32);
};

#endif
