/*
 * Copyright (c) 2023 by Thomas Neder (https://github.com/tomneda)
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

#include "service-db.h"

#include <filesystem>
#include <QTableView>
#include <QtSql/QSql>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QVariant>
#include <QtDebug>
#include <QCoreApplication>
#include <utility>

static const QString sTabServList = "TabServList";
static const QString sTabFavList  = "TabFavList";
static const QString sTeService   = "Service";
static const QString sTeChannel   = "Channel";
static const QString sTeIsFav     = "IsFav";

ServiceDB::ServiceDB(QString iDbFileName) :
  mDbFileName(std::move(iDbFileName)) // clang-tidy wants it so...
{
}

ServiceDB::~ServiceDB()
{
  mDB.close();
}

void ServiceDB::open_db()
{
  mDB = QSqlDatabase::addDatabase("QSQLITE");

  if (!_open_db())
  {
    qCritical() << "Error: Unable to establish a database connection: %s. Try deleting database and repeat..." << _error_str();
    _delete_db_file(); // emergency delete, hoping next start will work with new database

    if (!_open_db())
    {
      qCritical() << "Error: Unable to establish a database connection: " << _error_str();
      QCoreApplication::exit(1);
    }
  }
  // the code will exit if table could not be opened (qFatal does this)
}

void ServiceDB::create_table()
{
  const QString queryStr1 = "CREATE TABLE IF NOT EXISTS " + sTabServList + " ("
                            "Id      INTEGER PRIMARY KEY,"
                            + sTeChannel + " TEXT NOT NULL,"
                            + sTeService + " TEXT NOT NULL,"
                            + sTeIsFav + " INTEGER DEFAULT 0,"
                            "UNIQUE(" + sTeChannel + "," + sTeService + ") ON CONFLICT IGNORE"
                            ");";

  const QString queryStr2 = "CREATE TABLE IF NOT EXISTS " + sTabFavList + " ("
                            "Id      INTEGER PRIMARY KEY,"
                            + sTeChannel + " TEXT NOT NULL,"
                            + sTeService + " TEXT NOT NULL,"
                            "UNIQUE(" + sTeChannel + "," + sTeService + ") ON CONFLICT IGNORE"
                            ");";

  _exec_simple_query(queryStr1);
  _exec_simple_query(queryStr2);
}

void ServiceDB::delete_table(const bool iDeleteFavorites)
{
  _exec_simple_query("DROP TABLE IF EXISTS " + sTabServList + ";");

  if (iDeleteFavorites)
  {
    _exec_simple_query("DROP TABLE IF EXISTS " + sTabFavList + ";");
  }
}



bool ServiceDB::add_entry(const QString & iChannel, const QString & iService)
{
#if 1
  if (_check_if_entry_exists(sTabServList, iChannel, iService))
  {
    return false; // entry found, no table update needed
  }
#endif

  // add new entry
  QSqlQuery queryAdd;
  queryAdd.prepare("INSERT OR IGNORE INTO " + sTabServList + " (" + sTeChannel + "," + sTeService + ") VALUES (:channel, :service)");
  queryAdd.bindValue(":channel", iChannel);
  queryAdd.bindValue(":service", iService);

  if (!queryAdd.exec())
  {
    const QString dbErr = _error_str(); // next command could destroy this information
    _delete_db_file();
    qCritical() << "Error: Unable insert entry: " << dbErr;
    QCoreApplication::exit(1);
  }

  return true;
}

MySqlQueryModel * ServiceDB::create_model()
{
  MySqlQueryModel * model = new MySqlQueryModel;

  const QString sortDir = (mSortDesc ? " DESC" : " ASC");
  const QString sortSym = (mSortDesc ? "↑" : "↓");

  QString sortName;
  QString colNameIsFav   = sTeIsFav + " AS Fav";
  QString colNameService = sTeService + " AS Service";
  QString colNameChannel = sTeChannel + " AS Ch";
  QString colNameId      = "Id AS Id";

  switch (mSortColIdx)
  {
  case CI_Fav:
    sortName = sTeIsFav + " DESC, UPPER(" + sTeService + ")" + sortDir;
    colNameIsFav += sortSym;
    break;
  case CI_Service:
    sortName = "UPPER(" + sTeService + ")" + sortDir;
    colNameService += sortSym;
    break;
  case CI_Channel:
    sortName = sTeChannel + sortDir + ", UPPER(" + sTeService + ")" + sortDir;
    colNameChannel += sortSym;
    break;
  case CI_Id:
    sortName = "Id" + sortDir;
    colNameId      += sortSym;
    break;
  }

  QSqlQuery query;
  query.prepare("SELECT " + colNameIsFav + "," + colNameService + "," + colNameChannel + "," + colNameId + " FROM " + sTabServList + " ORDER BY " + sortName);

  if (query.exec())
  {
    model->setQuery(query);
  }
  else
  {
    const QString dbErr = _error_str(); // next command could destroy this information
    _delete_db_file();
    qCritical() << "Error: Invalid SELECT query: " << dbErr;
    QCoreApplication::exit(1);
  }

  return model;
}

bool ServiceDB::_open_db()
{
  mDB.setDatabaseName(mDbFileName);
  return mDB.open();
}

void ServiceDB::_delete_db_file()
{
  qCritical("Failure in database -> delete database -> try to restart and scan services again!");
  
  if (_open_db())
  {
    mDB.close();
  }

  if (std::filesystem::exists(mDbFileName.toStdString()))
  {
    std::filesystem::remove(mDbFileName.toStdString());
  }
}

void ServiceDB::sort_column(const ServiceDB::EColIdx iColIdx)
{
  mSortDesc = (mSortColIdx == iColIdx && !mSortDesc); // change sorting direction with choosing the same column again or reset to asc sorting
  mSortColIdx = iColIdx;
}

void ServiceDB::set_favorite(const QString & iChannel, const QString & iService, const bool iIsFavorite, const bool iStoreInFavTable /*= true*/)
{
  QSqlQuery updateQuery;
  updateQuery.prepare("UPDATE " + sTabServList + " SET " + sTeIsFav + " = :isFav WHERE " + sTeChannel + " = :channel AND " + sTeService + " = :service");
  updateQuery.bindValue(":channel", iChannel);
  updateQuery.bindValue(":service", iService);
  updateQuery.bindValue(":isFav", (iIsFavorite ? 1 : 0));

  if (!updateQuery.exec())
  {
    qCritical() << "Error: Updating favorite: " << _error_str();
  }

  if (iStoreInFavTable)
  {
    QSqlQuery addQuery;
    if (iIsFavorite)
    {
      addQuery.prepare("INSERT OR IGNORE INTO " + sTabFavList + " (" + sTeChannel + "," + sTeService + ") VALUES (:channel, :service)");
    }
    else
    {
      addQuery.prepare("DELETE FROM " + sTabFavList + " WHERE " + sTeChannel + " = :channel AND " + sTeService + " = :service");
    }
    addQuery.bindValue(":channel", iChannel);
    addQuery.bindValue(":service", iService);

    if (!addQuery.exec())
    {
      qCritical() << "Error: Unable insert favorite table entry: " << _error_str();
    }
  }
}

QString ServiceDB::_error_str() const
{
  return mDB.lastError().text(); // make a copy of the string as the DB object could be invalid when using it
}

void ServiceDB::_exec_simple_query(const QString & iQuery)
{
  QSqlQuery query;

  if (!query.exec(iQuery))
  {
    const QString dbErr = _error_str(); // next command could destroy this information
    _delete_db_file();
    qCritical() << "Error: Failed to execute '" << iQuery << "': " << dbErr;
    QCoreApplication::exit(1);
  }
}

bool ServiceDB::_check_if_entry_exists(const QString & iTableName, const QString & iChannel, const QString & iService)
{
  // first check if entry already exists, this avoid new table update afterward
  QSqlQuery querySearch;
  querySearch.prepare("SELECT * FROM " + iTableName + " WHERE " + sTeChannel + " = :channel AND " + sTeService + " = :service");
  querySearch.bindValue(":channel", iChannel);
  querySearch.bindValue(":service", iService);

  if (querySearch.exec())
  {
    if (querySearch.next())
    {
      return true; // entry found
    }
  }
  else
  {
    const QString dbErr = _error_str(); // next command could destroy this information
    _delete_db_file();
    qCritical() << "Error: Search query with table '" << iTableName << "': " << dbErr;
    QCoreApplication::exit(1);
  }
  return false; // no entry found
}

void ServiceDB::retrieve_favorites_from_backup_table()
{
  QSqlQuery favQuery;

  if (favQuery.exec("SELECT * FROM " + sTabFavList))
  {
    while (favQuery.next())
    {
      // found entries are wanted favorites
      const QString channel = favQuery.value(sTeChannel).toString();
      const QString service = favQuery.value(sTeService).toString();
      set_favorite(channel, service, true, false);
    }
  }
  else
  {
    const QString dbErr = _error_str(); // next command could destroy this information
    _delete_db_file();
    qCritical() << "Error: Retrieve query with table '" << sTabFavList << "': " << dbErr;
    QCoreApplication::exit(1);
  }
}

QVariant MySqlQueryModel::data(const QModelIndex & index, int role) const
{
  // certain columns should be right aligned
  if (role == Qt::TextAlignmentRole)
  {
    switch (index.column())
    {
    case ServiceDB::CI_Channel:
    case ServiceDB::CI_Id:
      return Qt::AlignRight + Qt::AlignVCenter;
    }
  }

  return QSqlQueryModel::data(index, role);
}
