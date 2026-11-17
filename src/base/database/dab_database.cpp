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

#include "dab_database.h"
#include <QFile>
#include <QRegularExpression>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtDebug>
#include <utility>

using namespace DbNames;

// columns of the ensemble table which are taken over 1:1 from the former ensemble list database (version 04)
static const QString sEnsembleDataCols = QSL("FilePath, FileName, ScanLevel, FileLengthMB, EnsembleName, EnsembleId, ItuCode, Date, "
                                             "LastPlayedSId, DscTyAppTy, AudioDataRates, NumDabDabPlus, ErrorProtection, NomFreq, SNR, MER, BaseBandOffset");

static const QString sOldEnsSchema = QSL("OldEns");
static const QString sOldSrvSchema = QSL("OldSrv");

DabDatabase::DabDatabase(QString iDbFileName) :
  mDbFileName(std::move(iDbFileName))
{
  mConnectionName = QSL("DabDatabase:") + mDbFileName;
}

DabDatabase::~DabDatabase()
{
  if (mDB.isValid())
  {
    mDB.close();
    mDB = QSqlDatabase();
    QSqlDatabase::removeDatabase(mConnectionName);
  }
}

bool DabDatabase::open_db()
{
  mDB = QSqlDatabase::addDatabase("QSQLITE", mConnectionName);
  mDB.setDatabaseName(mDbFileName);

  if (!_open_and_create_schema())
  {
    qCritical() << "Error: Unable to use database" << mDbFileName << ":" << mDB.lastError().text() << "-> delete database and try again...";
    _delete_db_file(); // emergency delete, hoping it works with a new database

    if (!_open_and_create_schema())
    {
      qCritical() << "Error: Unable to use database" << mDbFileName << ":" << mDB.lastError().text();
      return false;
    }
  }

  return true;
}

bool DabDatabase::_open_and_create_schema()
{
  if (!mDB.open())
  {
    return false;
  }

  QSqlQuery query(mDB);

  if (!query.exec("PRAGMA user_version") || !query.next())
  {
    return false;
  }

  const i32 userVersion = query.value(0).toInt();

  if (userVersion > cSchemaVersion)
  {
    qCritical() << "Database" << mDbFileName << "has a newer schema version" << userVersion << "than supported" << cSchemaVersion;
    return false;
  }

  mIsNewlyCreated = (userVersion == 0);

  return _exec("PRAGMA foreign_keys = ON") && _create_schema();
}

bool DabDatabase::_create_schema()
{
  const QString queryEns = "CREATE TABLE IF NOT EXISTS " + cTabEnsemble + " ("
                           + cColId     + " INTEGER PRIMARY KEY,"
                           + cColMode   + " INTEGER NOT NULL,"
                           + cColFIdOrCh + " TEXT NOT NULL,"
                           // only for files
                           "FilePath        TEXT,"
                           "FileName        TEXT,"
                           "ScanLevel       INTEGER DEFAULT 0,"
                           "FileLengthMB    REAL DEFAULT 0,"
                           // common for files and device
                           "EnsembleName    TEXT,"
                           "EnsembleId      TEXT,"
                           "ItuCode         TEXT,"
                           "Date            TEXT,"
                           "LastPlayedSId   TEXT,"
                           "DscTyAppTy      TEXT,"
                           "AudioDataRates  TEXT,"
                           "NumDabDabPlus   TEXT,"
                           "ErrorProtection TEXT,"
                           "NomFreq         INTEGER DEFAULT 0,"
                           "SNR             REAL DEFAULT 0,"
                           "MER             REAL DEFAULT 0,"
                           "BaseBandOffset  REAL DEFAULT 0,"
                           + cColEcc    + " INTEGER DEFAULT 0,"
                           "UNIQUE(" + cColMode + "," + cColFIdOrCh + ")"
                           ");";

  const QString querySrv = "CREATE TABLE IF NOT EXISTS " + cTabService + " ("
                           + cColId      + " INTEGER PRIMARY KEY,"
                           + cColEnsRef  + " INTEGER NOT NULL REFERENCES " + cTabEnsemble + "(" + cColId + ") ON DELETE CASCADE,"
                           + cColSId     + " INTEGER NOT NULL,"
                           + cColService + " TEXT NOT NULL,"
                           "UNIQUE(" + cColEnsRef + "," + cColSId + ")"
                           ");";

  // no foreign key here, the favorites should survive a rescan or a deletion of the ensemble list
  const QString queryFav = "CREATE TABLE IF NOT EXISTS " + cTabFavorite + " ("
                           + cColEcc + " INTEGER NOT NULL,"
                           + cColSId + " INTEGER NOT NULL,"
                           "PRIMARY KEY(" + cColEcc + "," + cColSId + ")"
                           ");";

  return _exec(queryEns) &&
         _exec(querySrv) &&
         _exec(queryFav) &&
         _exec(QSL("PRAGMA user_version = %1").arg(cSchemaVersion));
}

bool DabDatabase::import_former_db_files(const QString& iServiceListDbFileName, const QString& iEnsembleListDbFileName)
{
  if (!mIsNewlyCreated)
  {
    return true; // only a new database is filled with the former content
  }

  mIsNewlyCreated = false; // do this only once

  const bool hasEnsFile = QFile::exists(iEnsembleListDbFileName);
  const bool hasSrvFile = QFile::exists(iServiceListDbFileName);

  if (!hasEnsFile && !hasSrvFile)
  {
    return true;
  }

  qInfo() << "Import former database files" << iEnsembleListDbFileName << "and" << iServiceListDbFileName << "into" << mDbFileName;

  // ATTACH is not possible within a transaction
  const auto attach = [this](const QString& iFileName, const QString& iSchema)
  {
    QSqlQuery query(mDB);
    query.prepare("ATTACH DATABASE :file AS " + iSchema);
    query.bindValue(":file", iFileName);
    if (!query.exec())
    {
      qCritical() << "Error: Unable to attach" << iFileName << ":" << query.lastError().text();
      return false;
    }
    return true;
  };

  const bool ensAttached = hasEnsFile && attach(iEnsembleListDbFileName, sOldEnsSchema);
  const bool srvAttached = hasSrvFile && attach(iServiceListDbFileName, sOldSrvSchema);

  bool ok = mDB.transaction();

  if (ensAttached)
  {
    ok = ok && _import_ensembles(QSL("TabDeviceEnsList"), EMode::Device);
    ok = ok && _import_ensembles(QSL("TabFileEnsList"), EMode::File);
    ok = ok && _set_ecc_from_itu_code();
  }

  if (srvAttached)
  {
    ok = ok && _import_services(QSL("TabDeviceList"), EMode::Device);
    ok = ok && _import_services(QSL("TabFilesList"), EMode::File);
    ok = ok && _import_favorites();
  }

  if (ok)
  {
    ok = mDB.commit();
  }

  if (!ok)
  {
    qCritical() << "Error: Import of former database files failed:" << mDB.lastError().text();
    mDB.rollback();
  }

  if (ensAttached)
  {
    (void)_exec("DETACH DATABASE " + sOldEnsSchema);
  }

  if (srvAttached)
  {
    (void)_exec("DETACH DATABASE " + sOldSrvSchema);
  }

  return ok;
}

bool DabDatabase::_import_ensembles(const QString& iOldTab, const EMode iMode)
{
  if (!_is_table_existing(sOldEnsSchema, iOldTab))
  {
    return true;
  }

  return _exec("INSERT OR IGNORE INTO " + cTabEnsemble + " (" + cColMode + "," + cColFIdOrCh + "," + sEnsembleDataCols + ") "
               "SELECT " + QString::number((i32)iMode) + "," + cColFIdOrCh + "," + sEnsembleDataCols +
               " FROM " + sOldEnsSchema + "." + iOldTab);
}

bool DabDatabase::_import_services(const QString& iOldTab, const EMode iMode)
{
  if (!_is_table_existing(sOldSrvSchema, iOldTab))
  {
    return true;
  }

  // services without an ensemble entry are not taken over
  return _exec("INSERT OR IGNORE INTO " + cTabService + " (" + cColEnsRef + "," + cColSId + "," + cColService + ") "
               "SELECT e." + cColId + ", s.SId, s.Service FROM " + sOldSrvSchema + "." + iOldTab + " s "
               "JOIN " + cTabEnsemble + " e ON e." + cColMode + " = " + QString::number((i32)iMode) + " AND e." + cColFIdOrCh + " = s.Channel");
}

bool DabDatabase::_import_favorites()
{
  if (!_is_table_existing(sOldSrvSchema, QSL("TabFavList")))
  {
    return true;
  }

  // the former favorites were stored with channel/FId and SId, the ECC is taken from the ensemble entry (0 if not known)
  return _exec("INSERT OR IGNORE INTO " + cTabFavorite + " (" + cColEcc + "," + cColSId + ") "
               "SELECT COALESCE((SELECT MAX(e." + cColEcc + ") FROM " + cTabEnsemble + " e WHERE e." + cColFIdOrCh + " = f.Channel), 0), f.SId "
               "FROM " + sOldSrvSchema + ".TabFavList f");
}

bool DabDatabase::_set_ecc_from_itu_code()
{
  QSqlQuery query(mDB);

  if (!query.exec("SELECT " + cColId + ", ItuCode FROM " + cTabEnsemble + " WHERE ItuCode IS NOT NULL AND ItuCode <> ''"))
  {
    return false;
  }

  QSqlQuery update(mDB);
  update.prepare("UPDATE " + cTabEnsemble + " SET " + cColEcc + " = :ecc WHERE " + cColId + " = :id");

  while (query.next())
  {
    const u8 ecc = get_ecc_from_itu_code(query.value(1).toString());

    if (ecc != 0)
    {
      update.bindValue(":ecc", ecc);
      update.bindValue(":id", query.value(0));

      if (!update.exec())
      {
        return false;
      }
    }
  }

  return true;
}

u8 DabDatabase::get_ecc_from_itu_code(const QString& iItuCode)
{
  static const QRegularExpression re(QSL("\\(([0-9a-fA-F]{2})/"));
  const QRegularExpressionMatch match = re.match(iItuCode);

  if (!match.hasMatch())
  {
    return 0;
  }

  return (u8)match.captured(1).toUInt(nullptr, 16);
}

bool DabDatabase::_is_table_existing(const QString& iSchema, const QString& iTabName) const
{
  QSqlQuery query(mDB);
  query.prepare("SELECT name FROM " + iSchema + ".sqlite_master WHERE type = 'table' AND name = :name");
  query.bindValue(":name", iTabName);
  return query.exec() && query.next();
}

bool DabDatabase::_exec(const QString& iQuery)
{
  QSqlQuery query(mDB);

  if (!query.exec(iQuery))
  {
    qCritical() << "Error: Failed to execute '" << iQuery << "':" << query.lastError().text();
    return false;
  }

  return true;
}

void DabDatabase::_delete_db_file()
{
  mDB.close();

  if (QFile::exists(mDbFileName))
  {
    QFile::remove(mDbFileName);
  }
}
