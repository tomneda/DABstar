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
