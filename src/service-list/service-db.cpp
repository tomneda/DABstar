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
#include <QFile>
#include <QTableView>
#include <QtSql/QSql>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQueryModel>
#include <QVariant>
#include <QtDebug>
#include <QCoreApplication>
#include <utility>

static const QString sTabPermServList = "TabPermServList";
static const QString sTabTempServList = "TabTempServList";
static const QString sTabFavList      = "TabFavList";
static const QString sTeServiceLabel  = "Service";
static const QString sTeChannel       = "Channel";
static const QString sTeServiceId     = "SId";
static const QString sTeIsFav         = "IsFav";

#if 0
class CustomSqlQueryModel : public QSqlQueryModel
{
  using QSqlQueryModel::QSqlQueryModel;
public:
  [[nodiscard]] QVariant data(const QModelIndex & index, i32 role) const override
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
};
#endif

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
  const QString queryStr1 = "CREATE TABLE IF NOT EXISTS " + _cur_tab_name() + " ("
                            "Id                   INTEGER PRIMARY KEY,"
                            + sTeChannel      + " TEXT NOT NULL,"
                            + sTeServiceId    + " INTEGER DEFAULT 0,"
                            + sTeServiceLabel + " TEXT NOT NULL,"
                            + sTeIsFav + " INTEGER DEFAULT 0,"
                            "UNIQUE(" + sTeChannel + "," + sTeServiceId + ") ON CONFLICT IGNORE"
                            ");";

  const QString queryStr2 = "CREATE TABLE IF NOT EXISTS " + sTabFavList + " ("
                            "Id                 INTEGER PRIMARY KEY,"
                            + sTeChannel    + " TEXT NOT NULL,"
                            + sTeServiceId  + " INTEGER DEFAULT 0,"
                            "UNIQUE(" + sTeChannel + "," + sTeServiceId + ") ON CONFLICT IGNORE"
                            ");";

  _exec_simple_query(queryStr1);
  _exec_simple_query(queryStr2);
}

void ServiceDB::delete_table(const bool iDeleteFavorites)
{
  _exec_simple_query("DROP TABLE IF EXISTS " + _cur_tab_name() + ";");

  if (iDeleteFavorites)
  {
    _exec_simple_query("DROP TABLE IF EXISTS " + sTabFavList + ";");
  }
}



bool ServiceDB::add_entry(const QString & iChannel, const QString & iServiceLabel, const u32 iSId)
{
  if (_check_if_entry_exists(_cur_tab_name(), iChannel, iSId))
  {
    return false; // entry found, no table update needed
  }

  // add new entry
  QSqlQuery queryAdd;
  queryAdd.prepare("INSERT OR IGNORE INTO " + _cur_tab_name() + " (" + sTeChannel + "," + sTeServiceLabel + "," + sTeServiceId + ") VALUES (:channel, :service, :serviceId)");
  queryAdd.bindValue(":channel", iChannel);
  queryAdd.bindValue(":service", iServiceLabel);
  queryAdd.bindValue(":serviceId", iSId);

  if (!queryAdd.exec())
  {
    const QString dbErr = _error_str(); // next command could destroy this information
    _delete_db_file();
    qCritical() << "Error: Unable insert entry: " << dbErr;
    QCoreApplication::exit(1);
  }

  return true;
}

bool ServiceDB::delete_entry(const QString & iChannel, const u32 iSId)
{
  if (!_check_if_entry_exists(_cur_tab_name(), iChannel, iSId))
  {
    return false; // entry not found, no table update needed
  }

  // add new entry
  QSqlQuery queryDel;
  queryDel.prepare("DELETE FROM " + _cur_tab_name() + " WHERE " + sTeChannel + " = :channel AND " + sTeServiceId + " = :serviceId");
  queryDel.bindValue(":channel", iChannel);
  queryDel.bindValue(":serviceId", iSId);

  if (!queryDel.exec())
  {
    const QString dbErr = _error_str(); // next command could destroy this information
    _delete_db_file();
    qCritical() << "Error: Unable delete entry: " << dbErr;
    QCoreApplication::exit(1);
  }

  return true;
}

QAbstractItemModel * ServiceDB::create_model()
{
  //CustomSqlQueryModel * model = new CustomSqlQueryModel;
  auto * const model = new QSqlQueryModel;

  const QString sortDir = (mSortDesc ? " DESC" : " ASC");
  const QString sortSym = (mSortDesc ? "↑" : "↓");

  QString sortName;
  QString colNameIsFav   = sTeIsFav + " AS Fav";
  QString colNameService = sTeServiceLabel + " AS Service";
  QString colNameChannel = sTeChannel + " AS Ch";
  QString colNameServiceId = sTeServiceId + " AS SId";

  switch (mSortColIdx)
  {
  case CI_Fav:
    sortName = sTeIsFav + " DESC, UPPER(" + sTeServiceLabel + ")" + sortDir;
    colNameIsFav += sortSym;
    break;
  case CI_Service:
    sortName = "UPPER(" + sTeServiceLabel + ")" + sortDir;
    colNameService += sortSym;
    break;
  case CI_Channel:
    sortName = sTeChannel + sortDir + ", UPPER(" + sTeServiceLabel + ")" + sortDir;
    colNameChannel += sortSym;
    break;
  case CI_SId:
    sortName = sTeServiceId + sortDir;
    colNameServiceId += sortSym;
    break;
  default: /*should never happen*/ ;
  }

  QSqlQuery query;
  query.prepare("SELECT " + colNameIsFav + "," + colNameService + "," + colNameChannel + "," + colNameServiceId + " FROM " + _cur_tab_name() + " ORDER BY " + sortName);

  if (query.exec())
  {
    model->setQuery(std::move(query));
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

void ServiceDB::sort_column(const ServiceDB::EColIdx iColIdx, const bool iForceSortAsc)
{
  if (iForceSortAsc)
  {
    mSortDesc = false; // force to ascending sorting
  }
  else
  {
    mSortDesc = (mSortColIdx == iColIdx && !mSortDesc); // change sorting direction with choosing the same column again or reset to asc sorting
  }
  mSortColIdx = iColIdx;
}

bool ServiceDB::is_sort_desc() const
{
  return mSortDesc;
}

void ServiceDB::set_favorite(const QString & iChannel, const u32 iSId, const bool iIsFavorite) const
{
  if (mDataMode != EDataMode::Permanent)
  {
    return;
  }

  _set_favorite(iChannel, iSId, iIsFavorite, true);
}

void ServiceDB::retrieve_favorites_from_backup_table()
{
  if (mDataMode != EDataMode::Permanent)
  {
    return;
  }

  QSqlQuery favQuery;

  if (favQuery.exec("SELECT * FROM " + sTabFavList))
  {
    while (favQuery.next())
    {
      // found entries are wanted favorites
      const QString channel = favQuery.value(sTeChannel).toString();
      const u32 SId = favQuery.value(sTeServiceId).toUInt();
      _set_favorite(channel, SId, true, false);
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

void ServiceDB::_set_favorite(const QString & iChannel, const u32 iSId, const bool iIsFavorite, const bool iStoreInFavTable /*= true*/) const
{
  QSqlQuery updateQuery;
  updateQuery.prepare("UPDATE " + _cur_tab_name() + " SET " + sTeIsFav + " = :isFav WHERE " + sTeChannel + " = :channel AND " + sTeServiceId + " = :serviceId");
  updateQuery.bindValue(":channel", iChannel);
  updateQuery.bindValue(":serviceId", iSId);
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
      addQuery.prepare("INSERT OR IGNORE INTO " + sTabFavList + " (" + sTeChannel + "," + sTeServiceId + ") VALUES (:channel, :serviceId)");
    }
    else
    {
      addQuery.prepare("DELETE FROM " + sTabFavList + " WHERE " + sTeChannel + " = :channel AND " + sTeServiceId + " = :serviceId");
    }
    addQuery.bindValue(":channel", iChannel);
    addQuery.bindValue(":serviceId", iSId);

    if (!addQuery.exec())
    {
      qCritical() << "Error: Unable insert favorite table entry: " << _error_str();
    }
  }
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

  if (QFile::exists(mDbFileName))
  {
    QFile::remove(mDbFileName);
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

bool ServiceDB::_check_if_entry_exists(const QString & iTableName, const QString & iChannel, const u32 iSId)
{
  // first check if entry already exists, this avoid new table update afterward
  QSqlQuery querySearch;
  querySearch.prepare("SELECT * FROM " + iTableName + " WHERE " + sTeChannel + " = :channel AND " + sTeServiceId + " = :serviceId");
  querySearch.bindValue(":channel", iChannel);
  querySearch.bindValue(":serviceId", iSId);

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

const QString & ServiceDB::_cur_tab_name() const
{
  return (mDataMode == EDataMode::Permanent ? sTabPermServList : sTabTempServList);
}

void ServiceDB::set_data_mode(ServiceDB::EDataMode iDataMode)
{
  mDataMode = iDataMode;
}
