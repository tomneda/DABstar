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

static const QString sTableName = "TabServList";
static const QString sTeService = "Service";
static const QString sTeChannel = "Channel";
static const QString sTeIsFav   = "IsFav";

ServiceDB::ServiceDB(const QString & iDbFileName) :
  mDbFileName(iDbFileName)
{
  mDB = QSqlDatabase::addDatabase("QSQLITE");

  if (!_open_db())
  {
    qCritical("Error: Unable to establish a database connection: %s. Try deleting database and repeat...", _error_str());
    _delete_db_file(); // emergency delete, hoping next start will work with new database

    if (!_open_db())
    {
      qFatal("Error: Unable to establish a database connection: %s", _error_str());
    }
  }
}

ServiceDB::~ServiceDB()
{
  mDB.close();
}

void ServiceDB::create_table()
{
  const QString queryStr = "CREATE TABLE IF NOT EXISTS "+ sTableName + " ("
                           "Id      INTEGER PRIMARY KEY,"
                           + sTeChannel + " TEXT NOT NULL,"
                           + sTeService + " TEXT NOT NULL,"
                           + sTeIsFav   + " INTEGER DEFAULT 0,"
                           "UNIQUE("+ sTeChannel + "," + sTeService + ") ON CONFLICT IGNORE"
                           ");";

  QSqlQuery query;

  if (!query.exec(queryStr))
  {
    _delete_db_file();
    qFatal("Error: Failed to create table: %s", _error_str());
  }

}

void ServiceDB::delete_table()
{
  QSqlQuery query;
  if (!query.exec("DROP TABLE IF EXISTS " + sTableName + ";"))
  {
    _delete_db_file();
    qFatal("Error: Failed to delete table: %s", _error_str());
  }
}


bool ServiceDB::add_entry(const QString & iChannel, const QString & iService)
{
#if 1
  // first check if entry already exists, this avoid new table update afterward
  QSqlQuery querySearch;
  querySearch.prepare("SELECT * FROM " + sTableName + " WHERE " + sTeChannel + " = :channel AND " + sTeService + " = :service");
  querySearch.bindValue(":channel", iChannel);
  querySearch.bindValue(":service", iService);

  if (querySearch.exec())
  {
    if (querySearch.next())
    {
      return false; // no table update needed
    }
  }
  else
  {
    _delete_db_file();
    qFatal("Error: Search query: %s", _error_str());
  }
#endif

  // add new entry
  QSqlQuery queryAdd;
  queryAdd.prepare("INSERT OR IGNORE INTO " + sTableName + " (" + sTeChannel + "," + sTeService + ") VALUES (:channel, :service)");
  queryAdd.bindValue(":channel", iChannel);
  queryAdd.bindValue(":service", iService);

  if (!queryAdd.exec())
  {
    _delete_db_file();
    qFatal("Error: Unable insert entry: %s", _error_str());
  }

  return true;
}

MySqlQueryModel * ServiceDB::create_model()
{
  MySqlQueryModel * model = new MySqlQueryModel;

  const QString sortDir = (mSortDesc ? " DESC" : " ASC");
  const QString sortSym = (mSortDesc ? "↑" : "↓");

  QString sortName;
  QString colNameIsFav   = sTeIsFav   + " AS Fav";
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
  query.prepare("SELECT " + colNameIsFav + "," + colNameService + "," + colNameChannel + "," + colNameId + " FROM " + sTableName + " ORDER BY " + sortName);

  if (query.exec())
  {
    model->setQuery(query);
  }
  else
  {
    _delete_db_file();
    qFatal("Error: Invalid SELECT query: %s", _error_str());
  }

  return model;
}

const char * ServiceDB::_error_str() const
{
  return mDB.lastError().text().toStdString().c_str();
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

void ServiceDB::set_favorite(const QString & iChannel, const QString & iService, const bool iIsFavorite)
{
  QSqlQuery queryAdd;
  queryAdd.prepare("UPDATE " + sTableName + " SET " + sTeIsFav + " = :isFav WHERE " + sTeChannel + " = :channel AND " + sTeService + " = :service");
  queryAdd.bindValue(":channel", iChannel);
  queryAdd.bindValue(":service", iService);
  queryAdd.bindValue(":isFav",   (iIsFavorite ? 1 : 0));

  if (!queryAdd.exec())
  {
    qCritical("Error: updating favorite: %s", _error_str());
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
//    case ServiceDB::CI_Fav:
//      return Qt::AlignHCenter + Qt::AlignVCenter;
    }
  }

  return QSqlQueryModel::data(index, role);
}
