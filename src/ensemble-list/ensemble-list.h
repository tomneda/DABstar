/*
 * Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#pragma once

#include "glob_data_types.h"
#include "custom_frame.h"
#include "ensemble-list-db.h"
#include "band-handler.h"
#include "glob_enums.h"

class Ui_ensembleList;
class EnsembleListDbHandler;

// the data structure must be defined outside the EnsembleList class to pre-declare them in DabRadio.h (no nested declaration possible)
struct SScanResultEL : EnsembleListDB::SDbEntryData
{
  EInfoReason infoReason = EInfoReason::NoNullSymbDet;
  i32 curPacketIdx = 0;
  i32 nrPackets = 0;
};

struct SIdentInfoEL
{
  QString fIdOrCh;
  QString absPath;
  u32 sIdToPlay = 0;
  i32 curPacketIdx = 0;
  i32 nrPackets = 0;
  EScanLevel curScanLevel = EScanLevel::SL0_Init; // the current reach level level
  EScanLevel scanLevelToAchieve = EScanLevel::SL0_Init; // the target reach scan level (if same like curScanLevel, do not rescan new data back, except reReadScanLevel is true)
  bool reReadScanLevel = false;
};

class EnsembleList : public QObject
{
  Q_OBJECT

public:
  EnsembleList(const QString & iDbFileName);
  ~EnsembleList() override;

  enum class EListMode { Invalid, PlayFromDevice, PlayFromFiles };
  void set_list_mode(EListMode iListMode);
  EListMode get_list_mode() const { return mListMode; }
  const BandHandler & get_band_handler() { return mBandHandler; }

  bool is_hidden() const { return mFrame.isHidden(); }
  void hide();
  void show();
  void setVisible(bool iVisible) { if (iVisible) show(); else hide(); }
  void init_after_connect();

private:
  static constexpr i64 cMinFileSize = 4 * 1024 * 1024;
  Ui_ensembleList * const ui;
  CustomFrame mFrame;
  QScopedPointer<EnsembleListDbHandler> mpDbHandler;
  EListMode mListMode = EListMode::Invalid;
  i32 mCountScanOk = 0;
  i32 mCountScanFailed = 0;
  i32 mIndentGlobal = 0;
  i32 mCurrScanIdx = 0;
  QVector<SIdentInfoEL> mIdentInfoListForScan;
  std::atomic<bool> mIsScanning = false;
  QString mFIdOrChLast;
  BandHandler mBandHandler;
  EnsembleListDB::SDataFilter mDataFilter{};
  i32 mRowIdxClickOnList = -1;

  enum class ELogType { WARN, ERROR2, INFONEUT, INFOACK, INFONACK }; // ERROR is not working on Windows -> renamed to ERROR2
  void _log_to_result_display(ELogType iLogType, const QString & iMessage, i32 iAddIndent = 0) const;
  void _read_pos_and_size();
  void _write_pos_and_size();
  void _setup_ui_regarding_list_mode() const;
  void _setup_ui_regarding_scan_mode(bool iScanMode) const;
  QString _add_file_to_file_scan_list(const QString & iFileName, i64 iMinFileSize) const;
  void _signal_FId_or_Ch_from_table_index(i32 iRowIdx, u32 iSId = 0);
  void _add_channel_entries_to_db() const;
  QString _get_FId_or_Ch_from_table_index(i32 iRowIdx) const;
  const QString & _list_mode_str(const QString & iTextDeviceMode, const QString & iTextFileMode) const;
  void _set_EL_filter_check_states_active() const;
  [[nodiscard]] i32 _get_nr_rows_in_table() const;
  bool _get_ident_info_from_row_idx(SIdentInfoEL & oIdentInfo, i32 iRowIdx);
  void _signal_ident_info(const SIdentInfoEL & iIdentInfo);
  void _update_remove_invalid_files_button_state() const;
  void _stop_scan_process();

public slots:
  void slot_select_FId_or_Ch(const QString & iFIdOrCh, u32 iSId);   // trigger this will sent signal_file_or_channel_to_play back to DabRadio
  void slot_decoded_data_status(const SScanResultEL & iResult); // this will fill up the database if valid

private slots:
  void _slot_handle_scan_button();
  void _slot_handle_reset_data_base_button();
  void _slot_handle_remove_invalid_entries_button();
  void _slot_handle_path_with_files_to_add_button();
  void _slot_handle_add_files_in_path();
  void _slot_handle_add_single_file();
  void _slot_handle_ensemble_list_filter(int iState = 0);
  void _slot_handle_show_current_FId_or_Ch_only(int iState);
  void _slot_handle_table_click(const QModelIndex &index);

signals:
  void signal_start_stop_scan(bool oIsScanning);
  void signal_file_or_channel_to_play(const SIdentInfoEL & oIdentInfo); // starts playing (also while scan), provide a file or channel name, called with slot_select_FId_or_Ch or by a click in the table
  void signal_delete_unused_FId_or_Ch(QStringList oFIds);
  void signal_show_current_FId_or_Ch_only(bool oShow);
};

 // Q_DECLARE_METATYPE(FilePlayer::SScanResult)
