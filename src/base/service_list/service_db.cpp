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
#include "dab_database.h"
#include "qt_compat.h"
#include <QTableView>
#include <QtSql/QSql>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQueryModel>
#include <QVariant>
#include <QtDebug>

using namespace DbNames;

static const QString sTeServiceLabel = QSL("Service");
static const QString sTeFIdOrCh      = QSL("Channel");
static const QString sTeServiceId    = QSL("SId");
static const QString sTeIsFav        = QSL("IsFav");

// ensemble entry of the current data mode with the given channel/FId
static const QString sSelEnsembleId = "SELECT " + cColId + " FROM " + cTabEnsemble + " WHERE " + cColMode + " = :mode AND " + cColFIdOrCh + " = :channel";

// a favorite matches with the same mode, channel/FId and SId, "e" is the ensemble table, "s" the service table
static const QString sIsFavExpr = "EXISTS(SELECT 1 FROM " + cTabFavorite + " f WHERE f." + cColMode + " = e." + cColMode +
                                  " AND f." + cColFIdOrCh + " = e." + cColFIdOrCh + " AND f." + cColSId + " = s." + cColSId + ")";

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

ServiceDB::ServiceDB(const DabDatabase& iDabDatabase) :
  mDB(iDabDatabase.get_db())
{
}

void ServiceDB::delete_table(const bool iDeleteFavorites)
{
  QSqlQuery queryDel(mDB);
  queryDel.prepare("DELETE FROM " + cTabService + " WHERE " + cColEnsRef + " IN (SELECT " + cColId + " FROM " + cTabEnsemble + " WHERE " + cColMode + " = :mode)");
  queryDel.bindValue(":mode", _cur_mode());
  (void)_exec(queryDel, "delete services");

  if (iDeleteFavorites)
  {
    QSqlQuery queryDelFav(mDB);
    queryDelFav.prepare("DELETE FROM " + cTabFavorite + " WHERE " + cColMode + " = :mode");
    queryDelFav.bindValue(":mode", _cur_mode());
    (void)_exec(queryDelFav, "delete favorites");
  }
}

bool ServiceDB::add_entry(const QString& iChannel, const QString& iServiceLabel, const u32 iSId)
{
  // the ensemble entry should already exist via the ensemble list, but get sure that the service can be stored
  QSqlQuery queryEns(mDB);
  queryEns.prepare("INSERT INTO " + cTabEnsemble + " (" + cColMode + ", " + cColFIdOrCh + ") VALUES (:mode, :channel) "
                   "ON CONFLICT(" + cColMode + ", " + cColFIdOrCh + ") DO NOTHING");
  queryEns.bindValue(":mode", _cur_mode());
  queryEns.bindValue(":channel", iChannel);

  if (!_exec(queryEns, "insert ensemble entry"))
  {
    return false;
  }

  // add new entry or update service label on conflict
  QSqlQuery queryAdd(mDB);
  queryAdd.prepare("INSERT INTO " + cTabService + " (" + cColEnsRef + ", " + cColService + ", " + cColSId + ") "
                   "VALUES ((" + sSelEnsembleId + "), :service, :serviceId) "
                   "ON CONFLICT(" + cColEnsRef + ", " + cColSId + ") "
                   "DO UPDATE SET " + cColService + " = excluded." + cColService);
  queryAdd.bindValue(":mode", _cur_mode());
  queryAdd.bindValue(":channel", iChannel);
  queryAdd.bindValue(":service", iServiceLabel);
  queryAdd.bindValue(":serviceId", iSId);

  return _exec(queryAdd, "insert service entry");
}

bool ServiceDB::delete_entry(const QString& iChannel, const u32 iSId)
{
  QSqlQuery queryDel(mDB);
  queryDel.prepare("DELETE FROM " + cTabService + " WHERE " + cColEnsRef + " IN (" + sSelEnsembleId + ") AND " + cColSId + " = :serviceId");
  queryDel.bindValue(":mode", _cur_mode());
  queryDel.bindValue(":channel", iChannel);
  queryDel.bindValue(":serviceId", iSId);

  return _exec(queryDel, "delete service entry") && queryDel.numRowsAffected() > 0;
}

bool ServiceDB::delete_channel(const QString& iChannel)
{
  QSqlQuery queryDel(mDB);
  queryDel.prepare("DELETE FROM " + cTabService + " WHERE " + cColEnsRef + " IN (" + sSelEnsembleId + ")");
  queryDel.bindValue(":mode", _cur_mode());
  queryDel.bindValue(":channel", iChannel);

  return _exec(queryDel, "delete channel entries") && queryDel.numRowsAffected() > 0;
}

QList<QString> ServiceDB::get_list_of_channels() const
{
  QList<QString> channelList;
  QSqlQuery query(mDB);
  query.prepare("SELECT DISTINCT e." + cColFIdOrCh + " FROM " + cTabEnsemble + " e JOIN " + cTabService + " s ON s." + cColEnsRef + " = e." + cColId +
                " WHERE e." + cColMode + " = :mode");
  query.bindValue(":mode", _cur_mode());

  if (_exec(query, "SELECT DISTINCT query"))
  {
    while (query.next())
    {
      channelList.append(query.value(0).toString());
    }
  }

  return channelList;
}

QAbstractItemModel* ServiceDB::create_model(const QString& iFilterToFIdOrCh)
{
  // CustomSqlQueryModel * model = new CustomSqlQueryModel(this);
  auto* const model = new QSqlQueryModel(this);

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

  // the inner query provides the columns IsFav, Service, Channel and SId of the current data mode
  QString innerQuery = "SELECT " + sIsFavExpr + " AS " + sTeIsFav + ", s." + cColService + " AS " + sTeServiceLabel +
                       ", e." + cColFIdOrCh + " AS " + sTeFIdOrCh + ", s." + cColSId + " AS " + sTeServiceId +
                       " FROM " + cTabService + " s JOIN " + cTabEnsemble + " e ON e." + cColId + " = s." + cColEnsRef +
                       " WHERE e." + cColMode + " = :mode";

  if (!iFilterToFIdOrCh.isEmpty())
  {
    innerQuery += " AND e." + cColFIdOrCh + " = :channel";
  }

  QSqlQuery query(mDB);
  query.prepare("SELECT " + colNameIsFav + "," + colNameService + "," + colNameChannel + "," + colNameServiceId +
                " FROM (" + innerQuery + ") ORDER BY " + sortName);
  query.bindValue(":mode", _cur_mode());

  if (!iFilterToFIdOrCh.isEmpty())
  {
    query.bindValue(":channel", iFilterToFIdOrCh);
  }

  if (_exec(query, "SELECT query"))
  {
    model->setQuery(std::move(query));

    // Force fetching of all rows to ensure rowCount() returns the correct value. This is necessary because QSqlQueryModel
    // (and its SQLITE driver) usually fetches rows incrementally (e.g. only 256 rows at a time).
    while (model->canFetchMore())
    {
      model->fetchMore();
    }
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

void ServiceDB::set_favorite(const QString& iChannel, const u32 iSId, const bool iIsFavorite) const
{
  QSqlQuery query(mDB);

  if (iIsFavorite)
  {
    query.prepare("INSERT OR IGNORE INTO " + cTabFavorite + " (" + cColMode + "," + cColFIdOrCh + "," + cColSId + ") VALUES (:mode, :channel, :serviceId)");
  }
  else
  {
    query.prepare("DELETE FROM " + cTabFavorite + " WHERE " + cColMode + " = :mode AND " + cColFIdOrCh + " = :channel AND " + cColSId + " = :serviceId");
  }

  query.bindValue(":mode", _cur_mode());
  query.bindValue(":channel", iChannel);
  query.bindValue(":serviceId", iSId);
  (void)_exec(query, "update favorite");
}

bool ServiceDB::set_ecc(const QString& iChannel, const u8 iEcc)
{
  QSqlQuery queryEcc(mDB);
  queryEcc.prepare("UPDATE " + cTabEnsemble + " SET " + cColEcc + " = :ecc1 WHERE " + cColMode + " = :mode AND " + cColFIdOrCh + " = :channel AND " + cColEcc + " <> :ecc2");
  queryEcc.bindValue(":ecc1", iEcc);
  queryEcc.bindValue(":ecc2", iEcc);
  queryEcc.bindValue(":mode", _cur_mode());
  queryEcc.bindValue(":channel", iChannel);

  return _exec(queryEcc, "update ECC") && queryEcc.numRowsAffected() > 0;
}

bool ServiceDB::_exec(QSqlQuery& ioQuery, const char* const iWhat) const
{
  if (!ioQuery.exec())
  {
    qCritical() << "Error: Unable to" << iWhat << ":" << ioQuery.lastError().text();
    return false;
  }

  return true;
}

i32 ServiceDB::_cur_mode() const
{
  return (i32)(mDataMode == EDataMode::DevicePlayer ? DabDatabase::EMode::Device : DabDatabase::EMode::File);
}

void ServiceDB::set_data_mode(const ServiceDB::EDataMode iDataMode)
{
  mDataMode = iDataMode;
}
