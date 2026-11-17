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
//   Favorite: favorites, valid globally with the key ECC + SId (ECC = 0 means "not known yet" and matches all ECCs)
namespace DbNames
{
inline const QString cTabEnsemble = QSL("Ensemble");
inline const QString cTabService  = QSL("Service");
inline const QString cTabFavorite = QSL("Favorite");

inline const QString cColId        = QSL("Id");
inline const QString cColMode      = QSL("Mode");
inline const QString cColFIdOrCh   = QSL("FIdOrCh");
inline const QString cColEcc       = QSL("Ecc");
inline const QString cColEnsRef    = QSL("EnsembleRef");
inline const QString cColSId       = QSL("SId");
inline const QString cColService   = QSL("Service");
} // namespace DbNames

class DabDatabase
{
public:
  enum class EMode : i32
  {
    Device = 0,
    File   = 1
  };

  static constexpr i32 cSchemaVersion = 5;

  explicit DabDatabase(QString iDbFileName); // copy is intended
  ~DabDatabase();
  DabDatabase(const DabDatabase&) = delete;
  DabDatabase& operator=(const DabDatabase&) = delete;

  // opens the database and creates the tables if not existing, returns false if the database could not be used
  bool open_db();

  // takes over the content of the former separate database files (version 04) if this database was newly created
  // with open_db(), not existing files are ignored, the former files are not changed or deleted
  bool import_former_db_files(const QString& iServiceListDbFileName, const QString& iEnsembleListDbFileName);

  [[nodiscard]] const QSqlDatabase& get_db() const { return mDB; }
  [[nodiscard]] bool is_newly_created() const { return mIsNewlyCreated; }

  [[nodiscard]] static u8 get_ecc_from_itu_code(const QString& iItuCode); // ItuCode like "DE(e0/d)", returns 0 if not given

private:
  QSqlDatabase mDB;
  QString mDbFileName;
  QString mConnectionName;
  bool mIsNewlyCreated = false;

  bool _open_and_create_schema();
  bool _create_schema();
  bool _exec(const QString& iQuery);
  void _delete_db_file();
  [[nodiscard]] bool _is_table_existing(const QString& iSchema, const QString& iTabName) const;
  bool _import_ensembles(const QString& iOldTab, EMode iMode);
  bool _import_services(const QString& iOldTab, EMode iMode);
  bool _import_favorites();
  bool _set_ecc_from_itu_code();
};
