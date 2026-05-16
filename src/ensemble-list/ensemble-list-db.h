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
#include <QtSql/QSqlDatabase>
#include <QDir>

#include "glob_enums.h"

class QTableView;
class QAbstractItemModel;

class EnsembleListDB : public QObject
{
  Q_OBJECT
public:
  explicit EnsembleListDB(const QString iDbFileName); // copy is intended
  ~EnsembleListDB();

  enum class EDataMode
  {
    Invalid,
    Device,
    Files
  };

  enum class EDbDataType
  {
    InsertKeyAndS0WsData, // this is for the primary setup, loads FId or Ch only, deletes all other entries
    UpdateSL1Failed,      // data scanned but contains invalid data
    UpdateSL2FibData,
    UpdateSL3MedRunData,
    UpdateLastPlayedSId
  };

  static constexpr i32 CI_FId = 1; // column index of FId in file mode
  static constexpr i32 CI_CH  = 0; // column index of channel name in device mode
  static constexpr i32 CI_FSL = 3; // column index of scan level in file mode
  static constexpr i32 CI_CSL = 1; // column index of scan level in device mode

  struct SDataFilter
  {
    bool showNotScannedEntries = true;
    bool showScannedEntries = true;
    bool showNoSignalEntries = true;
    bool operator!=(const SDataFilter & data_filter) const
    {
      return showNotScannedEntries != data_filter.showNotScannedEntries ||
             showScannedEntries    != data_filter.showScannedEntries ||
             showNoSignalEntries   != data_filter.showNoSignalEntries;
    }
  };

  struct SDbEntryData
  {
    struct
    {
      QString fIdOrCh; // for file and device based scan
    } key;

    struct
    {
      QString get_path() const { return (fileName.isEmpty() ? "" : QDir(filePath).filePath(fileName)); }
      u32 get_sid_played() const { return sIdPlayed.toUInt(nullptr, 16); } // is null if not given
      void set_sid_played(const u32 iSId) { sIdPlayed = QString("%1").arg(iSId, 4, 16, QChar('0')); } // store as hex string

      // valid for file scan only
      QString filePath;
      QString fileName;
      i32 scanLevel;
      i32 fileLengthMB;
      QString sIdPlayed;
    } S0Ws; // stage 0 working set

    struct
    {
      QString errorProtection;
      QString dscTyAppTy;
      QString audioDataRates;
      QString numDabDabPlus;
    } S1Fib;

    struct
    {
      QString ensembleName;
      QString ensembleId;
      QString ituCountry;
      QString dateUtc;
      f64 snr;
      f64 mer;
      f64 basebandOffset;
      i32 nomFreqkHz;
    } S2MedRun;

    struct
    {
      i32 numTiiEntries;
    } S3LongRun;
  };

  void set_data_mode(EDataMode iDataMode);
  EDataMode get_data_mode() const { return mDataMode; }
  void open_db();
  void create_table();
  void delete_table();
  bool insert_or_update_entry(const SDbEntryData & iEntryData, EDbDataType iDataType) const;
  bool delete_entry(const QString & iFIdOrCh) const;
  bool delete_invalid_entries() const;
  [[nodiscard]] i32 get_nr_unscanned_entries() const; // get number of entries with EId not given yet
  [[nodiscard]] SDbEntryData get_entry(const QString & iFIdOrCh) const;
  bool is_entry_existing(const QString & iFIdOrCh) const;
  void set_data_filter(const SDataFilter & iDataFilter);
  QAbstractItemModel * create_model();
  [[nodiscard]] QString get_full_path(const QString & iFId) const;

private:
  QSqlDatabase mDB;
  QString mDbFileName;
  EDataMode mDataMode = EDataMode::Invalid;
  SDataFilter mDataFilter{};

  [[nodiscard]] QString _error_str() const;
  void _delete_db_file();
  bool _open_db();
  void _exec_simple_query(const QString & iQuery);
  [[nodiscard]] const QString & _cur_tab_name() const;
};

