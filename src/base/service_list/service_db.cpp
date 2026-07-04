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

#include "service_db.h"
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

static const QString sTabDeviceList = "TabDeviceList";
static const QString sTabFilesList  = "TabFilesList";
// static const QString sTabTempServList   = "TabTempServList";
static const QString sTabFavList     = "TabFavList";

static const QString sTeServiceLabel = "Service";
static const QString sTeFIdOrCh      = "Channel";
static const QString sTeServiceId    = "SId";
static const QString sTeIsFav        = "IsFav";

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
  mDB = QSqlDatabase::addDatabase("QSQLITE", "ServiceList");

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
                            + sTeFIdOrCh      + " TEXT NOT NULL,"
                            + sTeServiceId    + " INTEGER DEFAULT 0,"
                            + sTeServiceLabel + " TEXT NOT NULL,"
                            + sTeIsFav + " INTEGER DEFAULT 0,"
                            "UNIQUE(" + sTeFIdOrCh + "," + sTeServiceId + ") ON CONFLICT IGNORE"
                            ");";

  const QString queryStr2 = "CREATE TABLE IF NOT EXISTS " + sTabFavList + " ("
                            "Id                 INTEGER PRIMARY KEY,"
                            + sTeFIdOrCh    + " TEXT NOT NULL,"
                            + sTeServiceId  + " INTEGER DEFAULT 0,"
                            "UNIQUE(" + sTeFIdOrCh + "," + sTeServiceId + ") ON CONFLICT IGNORE"
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
  QSqlQuery queryAdd(mDB);
  queryAdd.prepare("INSERT OR IGNORE INTO " + _cur_tab_name() + " (" + sTeFIdOrCh + "," + sTeServiceLabel + "," + sTeServiceId + ") VALUES (:channel, :service, :serviceId)");
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
  QSqlQuery queryDel(mDB);
  queryDel.prepare("DELETE FROM " + _cur_tab_name() + " WHERE " + sTeFIdOrCh + " = :channel AND " + sTeServiceId + " = :serviceId");
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

bool ServiceDB::delete_channel(const QString & iChannel)
{
  QSqlQuery queryDel(mDB);
  queryDel.prepare("DELETE FROM " + _cur_tab_name() + " WHERE " + sTeFIdOrCh + " = :channel");
  queryDel.bindValue(":channel", iChannel);

  if (!queryDel.exec())
  {
    const QString dbErr = _error_str(); // next command could destroy this information
    _delete_db_file();
    qCritical() << "Error: Unable delete channel entries: " << dbErr;
    QCoreApplication::exit(1);
  }

  return (queryDel.numRowsAffected() > 0);
}

QList<QString> ServiceDB::get_list_of_channels() const
{
  QList<QString> channelList;
  QSqlQuery query(mDB);

  if (query.exec("SELECT DISTINCT " + sTeFIdOrCh + " FROM " + _cur_tab_name()))
  {
    while (query.next())
    {
      channelList.append(query.value(0).toString());
    }
  }
  else
  {
    const QString dbErr = _error_str(); // next command could destroy this information
    // do not delete database here, it could be a simple empty table or similar...
    qCritical() << "Error: SELECT DISTINCT query: " << dbErr;
  }

  return channelList;
}

QAbstractItemModel * ServiceDB::create_model(const QString & iFilterToFIdOrCh)
{
  // CustomSqlQueryModel * model = new CustomSqlQueryModel(this);
  auto * const model = new QSqlQueryModel(this);

  const QString sortDir = (mSortDesc ? " DESC" : " ASC");
  const QString sortSym = (mSortDesc ? "↑" : "↓");

  QString sortName;
  QString colNameIsFav   = sTeIsFav + " AS Fav";
  QString colNameService = sTeServiceLabel + " AS Service";
  QString colNameChannel = sTeFIdOrCh + (mDataMode == EDataMode::DevicePlayer ? " AS Ch" : " AS FId");
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
    sortName = sTeFIdOrCh + sortDir + ", UPPER(" + sTeServiceLabel + ")" + sortDir;
    colNameChannel += sortSym;
    break;
  case CI_SId:
    sortName = sTeServiceId + sortDir;
    colNameServiceId += sortSym;
    break;
  default: /*should never happen*/ ;
  }

  QSqlQuery query(mDB);

  QString whereClause;
  if (!iFilterToFIdOrCh.isEmpty())
  {
    whereClause = " WHERE " + sTeFIdOrCh + " = '" + iFilterToFIdOrCh + "'";
  }

  query.prepare("SELECT " + colNameIsFav + "," + colNameService + "," + colNameChannel + "," + colNameServiceId +
                " FROM " + _cur_tab_name() + whereClause + " ORDER BY " + sortName);

  if (query.exec())
  {
    model->setQuery(std::move(query));

    // Force fetching of all rows to ensure rowCount() returns the correct value. This is necessary because QSqlQueryModel
    // (and its SQLITE driver) usually fetches rows incrementally (e.g. only 256 rows at a time).
    while (model->canFetchMore())
    {
      model->fetchMore();
    }
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

void ServiceDB::sort_column(const ServiceDB::EColIdx iColIdx, const ESortDir iSortDir)
{
  switch (iSortDir)
  {
  case ESortDir::ChangeSortDirection: mSortDesc = (mSortColIdx == iColIdx && !mSortDesc); break; // change sorting direction only with choosing the same column again
  case ESortDir::KeepSortDirection:   mSortDesc = (mSortColIdx != iColIdx ? false : mSortDesc);  break; // keep sorting-direction only if same column is chosen again, else it is forced to ascend sorting
  case ESortDir::ForceSortAsc:        mSortDesc = false; break;
  case ESortDir::ForceSortDesc:       mSortDesc = true; break;
  }

  mSortColIdx = iColIdx;
}

// void ServiceDB::show_only_current_FId_or_Ch(const QString & iCurChannel)
// {
//   mCurChannel = iCurChannel;
// }

bool ServiceDB::is_sort_desc() const
{
  return mSortDesc;
}

void ServiceDB::set_favorite(const QString & iChannel, const u32 iSId, const bool iIsFavorite) const
{
  // if (mDataMode == EDataMode::Temporary)
  // {
  //   return;
  // }

  _set_favorite(iChannel, iSId, iIsFavorite, true);
}

void ServiceDB::retrieve_favorites_from_backup_table()
{
  // if (mDataMode == EDataMode::Temporary)
  // {
  //   return;
  // }

  QSqlQuery favQuery(mDB);

  if (favQuery.exec("SELECT " + sTeFIdOrCh + "," + sTeServiceId + " FROM " + sTabFavList))
  {
    while (favQuery.next())
    {
      // found entries are wanted favorites
      const QString channel = favQuery.value(sTeFIdOrCh).toString();
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
  QSqlQuery updateQuery(mDB);
  updateQuery.prepare("UPDATE " + _cur_tab_name() + " SET " + sTeIsFav + " = :isFav WHERE " + sTeFIdOrCh + " = :channel AND " + sTeServiceId + " = :serviceId");
  updateQuery.bindValue(":channel", iChannel);
  updateQuery.bindValue(":serviceId", iSId);
  updateQuery.bindValue(":isFav", (iIsFavorite ? 1 : 0));

  if (!updateQuery.exec())
  {
    qCritical() << "Error: Updating favorite: " << _error_str();
  }

  if (iStoreInFavTable)
  {
    QSqlQuery addQuery(mDB);

    if (iIsFavorite)
    {
      addQuery.prepare("INSERT OR IGNORE INTO " + sTabFavList + " (" + sTeFIdOrCh + "," + sTeServiceId + ") VALUES (:channel, :serviceId)");
    }
    else
    {
      addQuery.prepare("DELETE FROM " + sTabFavList + " WHERE " + sTeFIdOrCh + " = :channel AND " + sTeServiceId + " = :serviceId");
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
  QSqlQuery query(mDB);

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
  QSqlQuery querySearch(mDB);
  querySearch.prepare("SELECT " + sTeFIdOrCh + " FROM " + iTableName + " WHERE " + sTeFIdOrCh + " = :channel AND " + sTeServiceId + " = :serviceId");
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
  return (mDataMode == EDataMode::DevicePlayer ? sTabDeviceList : sTabFilesList);
}

void ServiceDB::set_data_mode(const ServiceDB::EDataMode iDataMode)
{
  mDataMode = iDataMode;
}
