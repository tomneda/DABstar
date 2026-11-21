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
#include "qt_compat.h"
#include <QtSql/QSqlDatabase>
#include <QString>

// Common SQLite database for the ensemble list and the service list (and later further data).
// Tables:
//   Ensemble: one entry per channel (device mode) or per file (file mode), the mode is given in column "Mode"
//   Service:  services of an ensemble entry, deleted automatically (ON DELETE CASCADE) with the ensemble entry
//   Favorite: favorites with the key mode + channel/FId + SId, independent of the other tables to survive a rescan
//   EnsembleUserData: favorite state and user note of an ensemble entry with the key mode + channel/FId,
//                     independent of the other tables to survive a rescan or a reset of the ensemble list
namespace DbNames
{
inline const QString cTabEnsemble    = QSL("Ensemble");
inline const QString cTabService     = QSL("Service");
inline const QString cTabFavorite    = QSL("Favorite");
inline const QString cTabEnsUserData = QSL("EnsembleUserData");

inline const QString cColId        = QSL("Id");
inline const QString cColMode      = QSL("Mode");
inline const QString cColFIdOrCh   = QSL("FIdOrCh");
inline const QString cColEcc       = QSL("Ecc");
inline const QString cColEnsRef    = QSL("EnsembleRef");
inline const QString cColSId       = QSL("SId");
inline const QString cColService   = QSL("Service");
inline const QString cColIsFav     = QSL("IsFav");
inline const QString cColNote      = QSL("Note");
} // namespace DbNames

class DabDatabase
{
public:
  enum class EMode : i32
  {
    Device = 0,
    File   = 1
  };

  enum class EOpenResult
  {
    Ok,
    NewerSchemaVersion, // the database was created by a newer DABstar version, it is not touched
    Failed              // the database could not be used (e.g. locked by another instance), it is not touched
  };

  // Schema version stored in "PRAGMA user_version". Increase it with each schema change and add a migration step
  // in _migrate(). The file name of the database is independent of this version.
  static constexpr i32 cSchemaVersion = 2;
  static constexpr i32 cBusyTimeoutMs = 2000;

  explicit DabDatabase(QString iDbFileName); // copy is intended
  ~DabDatabase();
  DabDatabase(const DabDatabase&) = delete;
  DabDatabase& operator=(const DabDatabase&) = delete;

  // opens the database and creates or migrates the tables, only a corrupted database file is deleted and created anew
  EOpenResult open_db();

  // closes the database and deletes the database file (with its journal files), it is created anew with the next open_db()
  bool delete_db_file();

  [[nodiscard]] const QSqlDatabase& get_db() const { return mDB; }
  [[nodiscard]] QString get_last_error() const { return mLastError; }

private:
  enum class EResult
  {
    Ok,
    Corrupt,
    NewerSchemaVersion,
    Failed
  };

  QSqlDatabase mDB;
  QString mDbFileName;
  QString mConnectionName;
  QString mLastError;
  bool mLastErrorIsCorrupt = false;

  EResult _open_and_migrate();
  [[nodiscard]] EResult _check_integrity();
  bool _migrate(i32 iFromVersion);
  bool _create_schema_v1();
  bool _create_schema_v2();
  bool _exec(const QString& iQuery);
  bool _exec(QSqlQuery& ioQuery, const QString& iQueryStr);
  bool _delete_db_file();
};
