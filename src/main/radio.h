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
#ifndef RADIO_H
#define RADIO_H

#include  "dab-constants.h"
#include  "ui_radio.h"
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
#include "openfiledialog.h"
#include "http-handler.h"
#include "device-selector.h"
#include "configuration.h"
#include "converter_48000.h"
#include  <set>
#include  <memory>
#include  <mutex>
#include  <QMainWindow>
#include  <QStringList>
#include  <QStandardItemModel>
#include  <QVector>
#include  <QComboBox>
#include  <QByteArray>
#include  <QLabel>
#include  <QTimer>
#include  <sndfile.h>

class QSettings;
class SettingHelper;
class QtAudio;
class timeTableHandler;
class ServiceListHandler;

#ifdef  HAVE_PLUTO_RXTX
class	dabStreamer;
#endif

class TechData;

struct DabService
{
  QString channel;
  QString serviceName;
  uint32_t SId = 0;
  int32_t SCIds = 0;
  int32_t subChId = 0;
  bool valid = false;
  bool is_audio = false;
  FILE * fd = nullptr;
  FILE * frameDumper = nullptr;
};

struct TheTime
{
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

struct ChannelDescriptor
{
  QString channelName;
  bool realChannel;
  bool etiActive;
  int serviceCount;
  int32_t nominalFreqHz;
  QString ensembleName;
  uint8_t mainId;
  uint8_t subId;
  std::vector<DabService> backgroundServices;
  DabService currentService;
  DabService nextService;
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
  std::set<uint16_t> transmitters;

  union STiiId
  {
    STiiId(int mainId, int subId) : MainId(mainId & 0x7F), SubId(subId & 0xFF) {};
    explicit STiiId(int fullId) : FullId(fullId) {};

    uint16_t FullId;
    struct
    {
      uint8_t MainId;
      uint8_t SubId;
    };
  };

  void clean_channel()
  {
    realChannel = true;
    serviceCount = -1;
    nominalFreqHz = -1;
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

class RadioInterface : public QWidget, private Ui_DabRadio
{
Q_OBJECT
public:
  RadioInterface(QSettings *, const QString &, const QString &, bool, int32_t dataPort, int32_t clockPort, int, QWidget * parent);
  ~RadioInterface() override;

  enum EAudioFlags : uint32_t
  {
    AFL_NONE     = 0x0,
    AFL_SBR_USED = 0x1,
    AFL_PS_USED  = 0x2
  };

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
    StatusInfoElem<bool>    Stereo;
    StatusInfoElem<bool>    EPG;
    StatusInfoElem<bool>    SBR;
    StatusInfoElem<bool>    PS;
    StatusInfoElem<bool>    Announce;
    StatusInfoElem<int32_t> BitRate;
  };

  int32_t mAudioFrameCnt = 0;
  int32_t mMotObjectCnt = 0;
  StatusInfo mStatusInfo{};
  FILE * mDlTextFile = nullptr;
  RingBuffer<cmplx> mSpectrumBuffer{2048};
  RingBuffer<cmplx> mIqBuffer{2 * 1536};
  RingBuffer<TQwtData> mCarrBuffer{2 * 1536};
  RingBuffer<float> mResponseBuffer{32768};
  RingBuffer<uint8_t> mFrameBuffer{2 * 32768};
  RingBuffer<uint8_t> mDataBuffer{32768};
  RingBuffer<int16_t> mAudioBuffer{8 * 32768};
  std::vector<float> mAudioOutBuffer;
  SpectrumViewer mSpectrumViewer;
  BandHandler mBandHandler;
  dlCache mDlCache{10};
  TiiHandler mTiiHandler{};
  OpenFileDialog mOpenFileDialog;
  RingBuffer<int16_t> mTechDataBuffer{16 * 32768};
  httpHandler * mpHttpHandler = nullptr;
  ProcessParams mProcessParams;
  const QString mVersionStr{PRJ_VERS};
  int32_t mFmFrequency = 0;
  ContentTable * mpContentTable = nullptr;
  FILE * mpLogFile = nullptr;
  ChannelDescriptor mChannel;
  int32_t mMaxDistance = -1;
  bool mDoReportError = false;
  TechData * mpTechDataWidget = nullptr;
  Configuration mConfig;
  SettingHelper * const mpSH;
  std::atomic<bool> mIsRunning;
  std::atomic<bool> mIsScanning;
  std::unique_ptr<IDeviceHandler> mpInputDevice;
  DeviceSelector mDeviceSelector;
#ifdef  HAVE_PLUTO_RXTX
  dabStreamer * streamerOut = nullptr;
#endif
  DabProcessor * mpDabProcessor = nullptr;
  //AudioBase * mpSoundOut = nullptr;
  converter_48000 mAudioSampRateConv{};
  QScopedPointer<QtAudio> mpSoundOut;

#ifdef  DATA_STREAMER
  tcpServer * dataStreamer = nullptr;
#endif
#ifdef  CLOCK_STREAMER
  tcpServer * clockStreamer = nullptr;
#endif
  CEPGDecoder mEpgHandler;
  EpgDecoder mEpgProcessor;
  QString mEpgPath;
  QTimer mEpgTimer;
  QString mPicturesPath;
  QString mFilePath;
  SNDFILE * mpRawDumper = nullptr;
  bool mAudioDumping = false;
  std::vector<serviceId> mServiceList;

  QTimer mDisplayTimer;
  QTimer mChannelTimer;
  QTimer mPresetTimer;
  QTimer mClockResetTimer;
  bool mMutingActive = true;
  int32_t mNumberofSeconds = 0;
  int16_t mFicBlocks = 0;
  int16_t mFicSuccess = 0;
  int32_t mTotalFicError = 0;
  int32_t mTotalFics = 0;
  TheTime mLocalTime;
  TheTime mUTC;
  timeTableHandler * mpTimeTable = nullptr;
  FILE * mpFicDumpPointer = nullptr;
  bool mShowOnlyCurrTrans = false;
  size_t mPreviousIdleTime = 0;
  size_t mPreviousTotalTime = 0;
  std::unique_ptr<ServiceListHandler> mpServiceListHandler;
  bool mCurFavoriteState = false;
  bool mClockActiveStyle = true;
  std::mutex mMutex;

  static QStringList get_soft_bit_gen_names();
  std::vector<serviceId> insert_sorted(const std::vector<serviceId> &, const serviceId &);
  void LOG(const QString &, const QString &);
  uint32_t extract_epg(const QString&, const std::vector<serviceId> & iServiceList, uint32_t);
  void show_pause_slide();
  void connectGUI();
  void disconnectGUI();
  static QString convertTime(int, int, int, int, int, int = -1);
  QString get_copyright_text() const;
  void cleanScreen();
  void _show_hide_buttons(const bool iShow);

  void start_etiHandler();
  void stop_etiHandler();
  static QString check_and_create_dir(const QString &);

  void startAudioservice(Audiodata *);
  void startPacketservice(const QString &);
  void startScanning();
  void stopScanning(bool);
  void startAudiodumping();
  void stopAudiodumping();

  void startSourcedumping();
  void stopSourcedumping();
  void startFramedumping();
  void stopFramedumping();
  void startChannel(const QString &);
  void stopChannel();
  void stopService(DabService &);
  void startService(DabService &);
  //void colorService(QModelIndex ind, QColor c, int pt, bool italic = false);
  void localSelect(const QString & s);
  void localSelect(const QString &, const QString &);
  //void showServices();

  bool doStart();
  void save_MOTObject(QByteArray &, QString);

  void save_MOTtext(QByteArray &, int, QString);
  void show_MOTlabel(QByteArray & data, int contentType, const QString & pictureName, int dirs);

  //enum direction { FORWARD, BACKWARDS };
  //void handle_serviceButton(direction);
  void enable_ui_elements_for_safety(bool iEnable);

  //void colorServiceName(const QString & s, QColor color, int fS, bool);
  void write_warning_message(const QString & iMsg);
  void write_picture(const QPixmap & iPixMap) const;

  static QString get_bg_style_sheet(const QColor & iBgBaseColor, const char * const ipWidgetType = nullptr);
  void setup_ui_colors();
  void set_favorite_button_style();
  void _update_channel_selector(int);
  void _show_epg_label(const bool iShowLabel);
  void _set_http_server_button(const bool iActive);
  void _set_clock_text(const QString & iText = QString());
  void _create_status_info();
  template<typename T> void _add_status_label_elem(StatusInfoElem<T> & ioElem, const uint32_t iColor, const QString & iName, const QString & iToolTip);
  template<typename T> void _set_status_info_status(StatusInfoElem<T> & iElem, const T iValue);
  void _reset_status_info();
  void _update_channel_selector();
  void _set_device_to_file_mode(const bool iDataFromFile);

signals:
  void signal_set_new_channel(int);
  void signal_dab_processor_started();
  void signal_test_slider_changed(int);

public slots:
  void slot_add_to_ensemble(const QString &, int);
  void slot_name_of_ensemble(int, const QString &);
  void slot_show_frame_errors(int);
  void slot_show_rs_errors(int);
  void slot_show_aac_errors(int);
  void slot_show_fic_success(bool);
  void slot_set_synced(bool);
  void slot_show_label(const QString &);
  void slot_handle_mot_object(QByteArray, QString, int, bool);
  void slot_send_datagram(int);
  void slot_handle_tdc_data(int, int);
  void slot_change_in_configuration();
  void slot_new_audio(int, int, int);
  void slot_set_stereo(bool);
  void slot_set_stream_selector(int);
  void slot_no_signal_found();
  void slot_show_mot_handling(bool);
  void slot_set_sync_lost();
  void slot_show_correlation(int amount, int marker, float, const QVector<int> & v);
  void slot_show_spectrum(int);
  void slot_show_iq(int, float);
  void slot_show_mod_quality_data(const OfdmDecoder::SQualityData *);
  void slot_show_peak_level(float iPeakLevel);
  void slot_show_rs_corrections(int, int);
  void slot_show_tii(int, int);
  void slot_clock_time(int, int, int, int, int, int, int, int, int);
  void slot_start_announcement(const QString &, int);
  void slot_stop_announcement(const QString &, int);
  void slot_new_frame(int);
  void slot_show_clock_error(float e);
  void slot_set_epg_data(int, int, const QString &, const QString &);
  void slot_epg_timer_timeout();
  void slot_nr_services(int);
  void slot_handle_content_selector(const QString &);
  //void slot_http_terminate();
  void slot_set_and_show_freq_corr_rf_Hz(int iFreqCorrRF);
  void slot_show_freq_corr_bb_Hz(int iFreqCorrBB);
  void slot_test_slider(int);
  void slot_load_table();
  void slot_handle_transmitter_tags(int);
  void slot_handle_dl_text_button();
  void slot_handle_logger_button(int);
  void slot_handle_port_selector();
  void slot_handle_set_coordinates_button();
  void slot_handle_eti_active_selector(int);
  void slot_handle_tii_detector_mode(bool);
  void slot_handle_dc_avoidance_algorithm(bool);
  void slot_handle_dc_removal(bool);

  void closeEvent(QCloseEvent * event) override;

private slots:
  void _slot_handle_time_table();
  void _slot_handle_content_button();
  void _slot_handle_tech_detail_button();
  void _slot_handle_reset_button();
  void _slot_handle_scan_button();
  void _slot_handle_eti_handler();

  void _slot_handle_spectrum_button();
  void _slot_handle_device_widget_button();

  void _slot_do_start(const QString &);
  void _slot_new_device(const QString &);

  void _slot_handle_source_dump_button();
  void _slot_handle_frame_dump_button();
  void _slot_handle_audio_dump_button();

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
  void _slot_set_preset_service();
  void _slot_handle_mute_button();

  void _slot_handle_config_button();
  void _slot_handle_http_button();

  //	config handlers
  void _slot_handle_skip_list_button();
  void _slot_handle_skip_file_button();
};

#endif
