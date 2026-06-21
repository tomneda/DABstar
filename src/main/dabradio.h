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
#pragma once

#include "dab-constants.h"
#include "dab-processor.h"
#include "ringbuffer.h"
#include "band-handler.h"
#include "process-params.h"
#include "dl-cache.h"
#include "content-table.h"
#include "openfiledialog.h"
#include "device-selector.h"
#include "epg_mot_handler.h"
#include "tii_manager.h"
#include "ensemble-list.h"
#include "dab_channel_desc.h"
#ifdef  DATA_STREAMER
  #include    "tcp-server.h"
#endif
#include <memory>
#include <QStringList>
#include <QVector>
#include <QByteArray>
#include <QLabel>
#include <QTimer>
#include <sndfile.h>

class Ui_DabRadio;
class SpectrumViewer;
class CirViewer;
class Configuration;
class QSettings;
class Qt_Audio;
class TimeTableHandler;
class ServiceListHandler;
class TechData;
class MapHttpServer;
class EnsembleList;
class ItuTables;
class AudioManager;
class MotSlideProgress;
struct SIdentInfoEL;
struct SScanResultEL;

struct SDabService
{
  // Basic data (enough for audio playback)
  u32 SId = 0;
  i32 SubChId = 0;

  // Extended data (needs more data from the FIB which could take a bit longer to be received)
  i32 SCIdS = 0;
  QString serviceLabel;
};

struct STheTime
{
  i32 year;
  i32 month;
  i32 day;
  i32 hour;
  i32 minute;
  i32 second; // == -1: no seconds known
};

class DabRadio : public QWidget
{
Q_OBJECT
public:
  DabRadio(QSettings * ipSettings, const QString & iServiceListDbFileName, const QString & iEnsembleListDbFileName, i32 iDataPort, QWidget * iParent);
  ~DabRadio() override;

  enum EAudioFlags : u32
  {
    AFL_NONE     = 0x0,
    AFL_SBR_USED = 0x1,
    AFL_PS_USED  = 0x2
  };

private:
  static constexpr i32 cDisplayTimeoutMs     = 1000;
  static constexpr i32 cScanningTimeoutMs    = 6000; // max. waiting time for signal and FIB audio data while scanning
  static constexpr i32 cEpgTimeoutMs         = 3000;
  static constexpr i32 cClockResetTimeoutMs  = 5000;

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
    StatusInfoElem<bool> Stereo;
    StatusInfoElem<bool> MOT;
    StatusInfoElem<bool> EPG;
    StatusInfoElem<bool> SBR;
    StatusInfoElem<bool> PS;
    StatusInfoElem<bool> Announce;
    StatusInfoElem<bool> RsError;
    StatusInfoElem<bool> CrcError;
    StatusInfoElem<i32>  InpBitRate;  // tricky: bit rates must be of type i32
    StatusInfoElem<u32>  OutSampRate; // tricky: sample rates must be of type u32
    StatusInfoElem<const char *> ProtLevel;
  };

  struct SScanResult
  {
    u32 nrChannels = 0;
    u32 nrAudioServices = 0;
    u32 nrNonAudioServices = 0;
    QString lastFIdOrCh;
  };

  struct SDumpStatus
  {
    bool etiDumpActive = false;
    bool rawDumpActive = false;
    SNDFILE * pRawDumper = nullptr;
  };

  Ui_DabRadio * const ui;

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

  const QString mVersionStr{PRJ_VERS};

  QScopedPointer<SpectrumViewer> mpSpectrumViewer;
  QScopedPointer<CirViewer> mpCirViewer;
  QScopedPointer<MapHttpServer> mpHttpHandler;
  QScopedPointer<FibContentTable> mpFibContentTable;
  QScopedPointer<TechData> mpTechDataWidget;
  QScopedPointer<Configuration> mpConfig;
  QScopedPointer<ItuTables> mpItuTables;
  QScopedPointer<DabProcessor> mpDabProcessor;
  QScopedPointer<TimeTableHandler> mpTimeTable;
  QScopedPointer<ServiceListHandler> mpServiceListHandler;
  QScopedPointer<EnsembleList> mpEnsembleList;
  QScopedPointer<AudioManager> mpAudioManager;
  QScopedPointer<EpgMotHandler> mpEpgMotHandler;
  QScopedPointer<TiiManager> mpTiiManager;
  QScopedPointer<MotSlideProgress> mpMotSlideProgress;
  std::unique_ptr<IDeviceHandler> mpInputDevice;

  // Counters and indices
  i32 mMaxDistance = -1;
  u32 mResetRingBufferCnt = 0;
  i32 mEnsListRetriggerCnt = 0;
  usize mPreviousIdleTime = 0;
  usize mPreviousTotalTime = 0;

  // File handles
  FILE * mpFicDumpPointer = nullptr;

  // Dump timers
  uint32_t mRawDumpTimer = 0;
  uint32_t mEtiDumpTimer = 0;

  // QTimer objects
  QTimer mEpgTimer;
  QTimer mDisplayTimer;
  QTimer mScanSecurityTimer;
  QTimer mClockResetTimer;
  QTimer mEnsListRetriggerTimer; // if some data still missing (e.g., DateTime and EnsembleName to give more time)

  // Booleans
  bool mIsFileMode = false;
  bool mUsingFileDevice = false;
  bool mMutingActive = false;
  bool mCurFavoriteState = false;
  bool mClockActiveStyle = true;

  // Atomic booleans
  std::atomic<bool> mIsChannelRunning{false};
  std::atomic<bool> mIsScanning{false};

  // Complex types
  cf32 mLocalPos{};

  // Enums
  enum class EServiceListSrc { DEVICE_PLAYER, FILE_PLAYER };
  EServiceListSrc mEnsListServiceListSrc = EServiceListSrc::DEVICE_PLAYER;
  enum class EDipSyncState { NotSetYet, DipFound, DipNotFound };
  EDipSyncState mDipSyncState = EDipSyncState::NotSetYet;

  // Structures
  StatusInfo mStatusInfo{};
  SChannelDescriptor mChannelDesc{mIsFileMode};
  SDabService mCurPrimaryAudioService{};
  SDabService mCurPrimaryPacketService{};
  STheTime mLocalTime{};
  STheTime mUTC{};
  SDumpStatus mDumpStatus{};
  SScanResult mScanResult{};

  // Vectors
  std::vector<SDabService> mCurSecondaryServiceVec;
  std::vector<SServiceId> mServiceList;
  std::vector<QMetaObject::Connection> mSignalSlotConn;

  // Structures instances
  DynLinkCache mDynLabelCache{10};
  OpenFileDialog mOpenFileDialog;
  ProcessParams mProcessParams;
  DeviceSelector mDeviceSelector;

#ifdef DATA_STREAMER
  tcpServer * dataStreamer = nullptr;
#endif

  // DAB Processor Management
  void _connect_dab_processor_signals();
  void _disconnect_dab_processor_signals();
  void _create_and_init_dab_processor();
  void _delete_dab_processor();
  void _clean_up_dab_processor_and_input_device();
  void _create_new_input_device_and_dab_processor(const QString & iDeviceNameOrFileName);

  // Service Management
  bool _create_primary_backend_audio_service(const SAudioData & iAD);
  bool _create_primary_backend_packet_service(const SPacketData & iPD) const;
  bool _create_secondary_backend_packet_service(const SPacketData & iPD) const;
  bool _start_primary_and_secondary_service(u32 iSId, bool iStartPrimaryAudioOnly);
  void _stop_services(bool iStopAlsoGlobServices);

  // Channel and Playback Control
  void _start_channel(const QString & iFIdOrCh, u32 iSId);
  void _stop_channel();
  void _start_playing(const QString & iFIdOrCh, u32 iSId, bool iSameFIdOrCh);

  // Scanning Operations
  void _start_scanning(SChannelDescriptor & ioChannelDesc);
  void _stop_scanning(SChannelDescriptor & ioChannelDesc);
  void _update_scan_statistics(const QString & iFIdOrCh, const SServiceId & sl);
  void _check_for_itu_code();
  QString _get_scan_message(bool iEndMsg) const;

  // Dumping and Recording
  void _start_eti_handler(const SChannelDescriptor & iChannelDesc, SDumpStatus & ioDumpStatus);
  void _stop_eti_handler(SDumpStatus & ioDumpStatus) const;
  void _start_source_dumping(const SChannelDescriptor & iChannelDesc, SDumpStatus & ioDumpStatus);
  void _stop_source_dumping(SDumpStatus & ioDumpStatus) const;

  // Time and Date Handling
  void _get_ymd_from_mod_julian_date(i32 & oYear, i32 & oMonth, i32 & oDay, const i32 iMJD) const;
  i32 _get_local_time(i32 & oLocalHour, i32 & oLocalMinute, i32 iUtcHour, i32 iUtcMinute, i32 iLtoMinutes) const;
  QString _conv_to_time_str(i32 iYear, i32 iMonth, i32 iDay, i32 iHours, i32 iMinutes, i32 iSeconds = -1) const;
  QString _seconds_to_timestring(u32 iTimer) const;

  // UI Display and Status Updates
  template <typename T> void _add_status_label_elem(StatusInfoElem<T> & ioElem, const u32 iColor, const QString & iName, const QString & iToolTip);
  template <typename T> void _set_status_info_status(StatusInfoElem<T> & iElem, const T iValue) const;
  void _clean_screen(StatusInfo & ioStatusInfo) const;
  void _reset_status_info(StatusInfo & ioStatusInfo) const;
  void _display_service_label(const QString & iServiceLabel) const;
  void _write_warning_message(const QString & iMsg) const;

  // UI Controls and Styling
  void _set_favorite_button_style();
  void _show_epg_label(const bool iShowLabel);
  enum class EHttpButtonState { Off, On, Waiting };
  void _set_http_server_button(EHttpButtonState iHttpServerState) const;
  void _set_clock_text(const QString & iText = QString());
  void _enable_ui_elements_for_safety(bool iEnable) const;
  void _adapt_gui_for_device_or_file_play(bool iPlayingDevice) const;
  void _cleanup_ui() const;
  void _set_mot_progress(i32 iMotStart, i32 iMotEnd) const;

  // Settings and Configuration
  void _write_sid_to_settings(u32 iSId) const;
  void _write_fid_or_Ch_to_settings(const QString & iFIdOrCh) const;
  void _get_local_position_from_config(cf32 & oLocalPos) const;
  void _show_or_hide_windows_from_config();

  // Ensemble and Service List
  void _collect_fib_data_and_emit_to_ensemble_list(const SChannelDescriptor & iChannelDesc) const;
  bool _collect_deferred_data_and_emit_to_ensemble_list(const SChannelDescriptor & iChannelDesc, bool iForceSend) const;
  void _inform_ensemble_list(const SIdentInfoEL & iIdentInfoEL, EInfoReason iInfoReason);

  void _update_audio_data_addon(u32 iSId) const;

  // Utility Functions
  QStringList _get_soft_bit_gen_names() const;
  QString _convert_links_to_clickable(const QString & iText) const;
  void _check_on_github_for_update(bool iShowMessageBox);

  void _initialize_signal_slot_connections();
  void _initialize_ensemble_list();
  void _initialize_ui_elements();
  void _initialize_status_info();
  void _initialize_dynamic_label() const;
  void _initialize_audio_output();
  void _initialize_epg_mot_handler();
  void _initialize_tii_manager();
  void _initialize_time_table();
  void _initialize_version_and_copyright_info();
  void _initialize_and_start_timers();
  void _initialize_device_selector(SChannelDescriptor & ioChannelDesc) const;

public slots:
  // Ensemble and Service Information
  void slot_name_of_ensemble(i32 iEId, const QString & iEnsName, const QString & iEnsNameShort);
  void slot_show_label(const QString & iLabel);
  void slot_change_in_configuration();
  void slot_handle_fib_content_selector(u32 iSId);
  void slot_load_table();

  // Error and Status Display
  void slot_show_frame_errors(i32 iFrameErrors);
  void slot_show_rs_errors(i32 iRsErrors);
  void slot_show_aac_errors(i32 iAacErrors);
  void slot_show_fic_status(i32 iSuccessPercent, f32 iBER);
  void slot_show_rs_corrections(i32 iC, i32 iEc);

  // Signal Quality and Measurements
  void slot_show_correlation(f32 iThreshold, const QVector<i32> & iV);
  void slot_show_spectrum(i32 iAmount) const;
  void slot_show_cir() const;
  void slot_show_iq(i32, f32) const;
  void slot_show_lcd_data(const OfdmDecoder::SLcdData & iQD);
  void slot_show_tii(const std::vector<STiiResult> & iTr);
  void slot_show_clock_error(f32 e) const;
  void slot_set_and_show_freq_corr_rf_Hz(i32 iFreqCorrRF);
  void slot_show_freq_corr_bb_Hz(i32 iFreqCorrBB);

  // Audio
  void slot_new_audio(i32 iAmount, u32 iAudioSampleRate, u32 iAudioFlags);
  void slot_set_stereo(bool iStereo);
  void slot_set_stream_selector(i32 iIndex);
  void slot_new_aac_mp2_frame();
  void slot_show_digital_peak_and_rms_level(f32 iLevelPeak, f32 iLevelRms) const;

  // MOT (Multimedia Object Transfer) — thin wrappers delegating to mpEpgMotHandler
  void slot_pad_mot_progress(i32 iPercent) const;
  void slot_handle_mot_object(const QByteArray & iResult, const QString & iObjectName, i32 iContentType, bool iDirElement) const;
  void slot_trigger_mot_indicator() const;

  // Data Services
  void slot_send_datagram(i32 iLength);
  void slot_handle_tdc_data(i32 iFrameType, i32 iLength);

  // Time and Announcements
  void slot_fib_time(const IFibDecoder::SUtcTimeSet & fibTimeInfo);
  void slot_start_announcement(const QString & iName, i32 iSubChId);
  void slot_stop_announcement(const QString & iName, i32 iSubChId);
  void slot_epg_timer_timeout();

  // Configuration and Settings
  void slot_handle_dc_avoidance_algorithm(bool);
  void slot_handle_dc_and_iq_corr(bool iDcCorr, bool iIqCorr);

  // UI Actions
  void slot_handle_journaline_viewer_closed(i32 iSubChannel);

  // Network and Updates
  void slot_http_terminate();
  void slot_check_for_update();

protected slots:
  void closeEvent(QCloseEvent * event) override;

private slots:
  // Button Handlers - UI Navigation
  void _slot_handle_time_table();
  void _slot_handle_content_button();
  void _slot_handle_tech_detail_button();
  void _slot_handle_reset_button();
  void _slot_handle_spectrum_button();
  void _slot_handle_cir_button();
  void _slot_handle_open_pic_folder_button() const;
  void _slot_handle_device_widget_button() const;
  void _slot_handle_config_button();
  void _slot_handle_http_button();
  void _slot_handle_ensemble_list_button() const;

  // Button Handlers - Dumping and Recording
  void _slot_handle_eti_button();
  void _slot_handle_source_dump_button();

  // Button Handlers - Service Navigation
  void _slot_handle_prev_service_button();
  void _slot_handle_next_service_button();
  void _slot_handle_target_service_button();
  void _slot_handle_file_looped();

  // Button Handlers - Audio Control
  void _slot_handle_mute_button();

  // Button Handlers - Favorites
  void _slot_handle_favorite_button(bool iClicked);

  // Service and Channel Management
  void _slot_device_selected(const QString &);
  void _slot_service_changed(const QString & iFIdOrCh, const QString & iService, u32 iSId);
  void _slot_file_or_channel_to_play(const SIdentInfoEL & iIdentInfo);
  void _slot_start_stop_scan(bool iIsScanning);
  void _slot_service_list_src_change(int iIdClicked);
  void _slot_show_current_fid_or_ch_only(bool iShowOnlyCurrentFIdOrCh) const;

  // State Updates and Timers
  void _slot_favorite_changed(bool iIsFav);
  void _slot_update_mute_state(bool iMute);
  void _slot_fib_loaded_state(IFibDecoder::EFibLoadingState iFibLoadingState);
  void _slot_update_time_display();
  void _slot_scanning_security_timeout();
  void _slot_no_dip_sync_found();
  void _slot_dip_sync_found();
  void _slot_ensemble_list_retrigger_timeout();

  // UI Styling and Display
  void _slot_set_static_button_style();

  // Application Control
  void _slot_terminate_process();
  void _slot_check_for_update();

signals:
  void signal_dab_processor_started();
  void signal_fib_data_status(const SScanResultEL &) const;
  void signal_fid_or_ch_selected(const QString & oFIdOrCh, u32 oSId);
};
