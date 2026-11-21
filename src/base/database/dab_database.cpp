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
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtDebug>
#include <utility>

using namespace DbNames;

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

DabDatabase::EOpenResult DabDatabase::open_db()
{
  mDB = QSqlDatabase::addDatabase("QSQLITE", mConnectionName);
  mDB.setDatabaseName(mDbFileName);
  mDB.setConnectOptions(QSL("QSQLITE_BUSY_TIMEOUT=%1").arg(cBusyTimeoutMs)); // wait if another instance accesses the database

  EResult result = _open_and_migrate();

  if (result == EResult::Corrupt)
  {
    // only a really corrupted database is deleted, the content (e.g. the favorites) would be lost otherwise
    qCritical() << "Error: Database" << mDbFileName << "is corrupted:" << mLastError << "-> delete database and create it anew";
    (void)_delete_db_file();
    result = _open_and_migrate();
  }

  switch (result)
  {
  case EResult::Ok:
    return EOpenResult::Ok;
  case EResult::NewerSchemaVersion:
    qCritical() << "Error: Database" << mDbFileName << "has a newer schema version than supported" << cSchemaVersion;
    mDB.close();
    return EOpenResult::NewerSchemaVersion;
  default:
    qCritical() << "Error: Unable to use database" << mDbFileName << ":" << mLastError;
    mDB.close();
    return EOpenResult::Failed;
  }
}

bool DabDatabase::delete_db_file()
{
  return _delete_db_file();
}

DabDatabase::EResult DabDatabase::_open_and_migrate()
{
  mLastError.clear();
  mLastErrorIsCorrupt = false;

  if (!mDB.open())
  {
    mLastError = mDB.lastError().text();
    return EResult::Failed;
  }

  if (const EResult result = _check_integrity();
      result != EResult::Ok)
  {
    return result;
  }

  QSqlQuery query(mDB);

  if (!_exec(query, QSL("PRAGMA user_version")) || !query.next())
  {
    return (mLastErrorIsCorrupt ? EResult::Corrupt : EResult::Failed);
  }

  const i32 userVersion = query.value(0).toInt();
  query.finish();

  if (userVersion > cSchemaVersion)
  {
    return EResult::NewerSchemaVersion;
  }

  if (!_exec(QSL("PRAGMA foreign_keys = ON"))) // must be set for each connection and outside a transaction
  {
    return EResult::Failed;
  }

  if (userVersion == cSchemaVersion)
  {
    return EResult::Ok; // nothing to do, so nothing is written
  }

  // create or migrate the schema within one transaction, so a failed migration leaves the database unchanged
  if (!mDB.transaction())
  {
    mLastError = mDB.lastError().text();
    return EResult::Failed;
  }

  if (!_migrate(userVersion) || !mDB.commit())
  {
    if (mLastError.isEmpty())
    {
      mLastError = mDB.lastError().text();
    }
    mDB.rollback();
    return EResult::Failed;
  }

  qInfo() << "Database" << mDbFileName << "migrated from schema version" << userVersion << "to" << cSchemaVersion;
  return EResult::Ok;
}

DabDatabase::EResult DabDatabase::_check_integrity()
{
  QSqlQuery query(mDB);

  if (!_exec(query, QSL("PRAGMA quick_check")))
  {
    return (mLastErrorIsCorrupt ? EResult::Corrupt : EResult::Failed); // e.g. SQLITE_BUSY must not delete the database
  }

  if (!query.next() || query.value(0).toString() != QSL("ok"))
  {
    mLastError = QSL("quick_check failed: ") + query.value(0).toString();
    return EResult::Corrupt;
  }

  return EResult::Ok;
}

bool DabDatabase::_migrate(const i32 iFromVersion)
{
  // iFromVersion == 0 is a new (empty) database
  if (iFromVersion < 1 && !_create_schema_v1())
  {
    return false;
  }

  if (iFromVersion < 2 && !_create_schema_v2())
  {
    return false;
  }

  // add further steps here, e.g.:
  // if (iFromVersion < 3 && !_exec("ALTER TABLE ...")) return false;

  return _exec(QSL("PRAGMA user_version = %1").arg(cSchemaVersion));
}

bool DabDatabase::_create_schema_v1()
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
                           + cColMode    + " INTEGER NOT NULL,"
                           + cColFIdOrCh + " TEXT NOT NULL,"
                           + cColSId     + " INTEGER NOT NULL,"
                           "PRIMARY KEY(" + cColMode + "," + cColFIdOrCh + "," + cColSId + ")"
                           ");";

  return _exec(queryEns) &&
         _exec(querySrv) &&
         _exec(queryFav);
}

bool DabDatabase::_create_schema_v2()
{
  // no foreign key here, the user data should survive a rescan or a deletion of the ensemble list
  const QString queryUserData = "CREATE TABLE IF NOT EXISTS " + cTabEnsUserData + " ("
                                + cColMode    + " INTEGER NOT NULL,"
                                + cColFIdOrCh + " TEXT NOT NULL,"
                                + cColIsFav   + " INTEGER NOT NULL DEFAULT 0,"
                                + cColNote    + " TEXT NOT NULL DEFAULT '',"
                                "PRIMARY KEY(" + cColMode + "," + cColFIdOrCh + ")"
                                ");";

  return _exec(queryUserData);
}

bool DabDatabase::_exec(const QString& iQuery)
{
  QSqlQuery query(mDB);
  return _exec(query, iQuery);
}

bool DabDatabase::_exec(QSqlQuery& ioQuery, const QString& iQueryStr)
{
  if (!ioQuery.exec(iQueryStr))
  {
    const QSqlError error = ioQuery.lastError();
    const i32 primaryCode = error.nativeErrorCode().toInt() & 0xFF; // SQLite result code, the lower byte is the primary code
    mLastErrorIsCorrupt = (primaryCode == 11 /*SQLITE_CORRUPT*/ || primaryCode == 26 /*SQLITE_NOTADB*/);
    mLastError = error.text();
    qCritical() << "Error: Failed to execute '" << iQueryStr << "':" << mLastError;
    return false;
  }

  return true;
}

bool DabDatabase::_delete_db_file()
{
  mDB.close();

  // also delete the journal files, they belong to the database
  bool ok = true;
  for (const QString& suffix : { QString(), QSL("-journal"), QSL("-wal"), QSL("-shm") })
  {
    if (const QString fileName = mDbFileName + suffix;
        QFile::exists(fileName) && !QFile::remove(fileName))
    {
      mLastError = QSL("Could not delete file ") + fileName;
      ok = false;
    }
  }
  return ok;
}
