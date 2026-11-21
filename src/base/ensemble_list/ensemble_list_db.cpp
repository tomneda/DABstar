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

#include "ensemble_list_db.h"
#include "dab_database.h"
#include "glob_enums.h"
#include "qt_compat.h"
#include <QTableView>
#include <QtSql/QSql>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQueryModel>
#include <QVariant>
#include <QtDebug>
#include <QDir>

static const QString& sTabEnsList = DbNames::cTabEnsemble;
static const QString& sTeMode     = DbNames::cColMode;

static const QString sTeFIdOrCh             = QSL("FIdOrCh");
static const QString sTeFilePath            = QSL("FilePath");
static const QString sTeFileName            = QSL("FileName");
static const QString sTeScanLevel           = QSL("ScanLevel");
static const QString sTeEnsembleName        = QSL("EnsembleName");
static const QString sTeEnsembleId          = QSL("EnsembleId");
static const QString sTeItuCode             = QSL("ItuCode");
static const QString sTeNumDabDabPlus       = QSL("NumDabDabPlus");
static const QString sTeDscTyAppTy          = QSL("DscTyAppTy");
static const QString sTeAudioDataRates      = QSL("AudioDataRates");
static const QString sTeErrorProtection     = QSL("ErrorProtection");
static const QString sTeFileLengthMB        = QSL("FileLengthMB");
static const QString sTeSNR                 = QSL("SNR");
static const QString sTeMER                 = QSL("MER");
static const QString sTeBasebandOffset      = QSL("BaseBandOffset");
static const QString sTeNomFreq             = QSL("NomFreq");
static const QString sTeDateUtc             = QSL("Date");
static const QString sTeLastPlayedSId       = QSL("LastPlayedSId");

static const QString& sTabUserData = DbNames::cTabEnsUserData;
static const QString& sTuIsFav     = DbNames::cColIsFav;
static const QString& sTuNote      = DbNames::cColNote;

// selects the given column of the user data of the current ensemble entry (used as correlated subquery)
static QString sel_user_data_col(const QString& iCol)
{
  return "(SELECT u." + iCol + " FROM " + sTabUserData + " u WHERE u." + sTeMode + " = " + sTabEnsList + "." + sTeMode +
         " AND u." + sTeFIdOrCh + " = " + sTabEnsList + "." + sTeFIdOrCh + ")";
}

EnsembleListDB::EnsembleListDB(const DabDatabase& iDabDatabase) :
  mDB(iDabDatabase.get_db())
{
}

void EnsembleListDB::delete_table()
{
  // the services of these entries are deleted automatically (ON DELETE CASCADE)
  QSqlQuery queryDel(mDB);
  queryDel.prepare("DELETE FROM " + sTabEnsList + " WHERE " + sTeMode + " = :mode");
  queryDel.bindValue(":mode", _cur_mode());
  (void)_exec(queryDel, "delete entries");
}

bool EnsembleListDB::is_table_existing(const EDataMode iDataMode) const
{
  assert(iDataMode != EDataMode::Invalid);
  (void)iDataMode; // the table is common for all data modes
  return mDB.tables().contains(sTabEnsList);
}

bool EnsembleListDB::insert_or_update_entry(const SDbEntryData& iEntryData, const EDbDataType iDataType) const
{
  // add new entry (UNIQUE constraint on FId will cause REPLACE if it already exists due to CREATE TABLE definition)
  QSqlQuery queryAdd(mDB);

  switch (iDataType)
  {
  case EDbDataType::InsertKeyAndS0WsData:
    // an existing entry is reset to the given data (like a REPLACE), but keeps its Id so that its services are kept
    queryAdd.prepare("INSERT INTO " + sTabEnsList + " (" +
                     sTeMode + "," +
                     sTeFIdOrCh + "," +
                     sTeFilePath + "," +
                     sTeFileName + "," +
                     sTeScanLevel + "," +
                     sTeFileLengthMB +
                     ") VALUES ("
                     ":mode, "
                     ":fidorch, "
                     ":filepath, "
                     ":filename, "
                     ":scanlevel, "
                     ":len"
                     ") ON CONFLICT(" + sTeMode + "," + sTeFIdOrCh + ") DO UPDATE SET " +
                     sTeFilePath         + " = excluded." + sTeFilePath + ", " +
                     sTeFileName         + " = excluded." + sTeFileName + ", " +
                     sTeScanLevel        + " = excluded." + sTeScanLevel + ", " +
                     sTeFileLengthMB     + " = excluded." + sTeFileLengthMB + ", " +
                     sTeEnsembleName     + " = NULL, " +
                     sTeEnsembleId       + " = NULL, " +
                     sTeItuCode          + " = NULL, " +
                     sTeDateUtc          + " = NULL, " +
                     sTeLastPlayedSId    + " = NULL, " +
                     sTeDscTyAppTy       + " = NULL, " +
                     sTeAudioDataRates   + " = NULL, " +
                     sTeNumDabDabPlus    + " = NULL, " +
                     sTeErrorProtection  + " = NULL, " +
                     sTeNomFreq          + " = 0, " +
                     sTeSNR              + " = 0, " +
                     sTeMER              + " = 0, " +
                     sTeBasebandOffset   + " = 0");
    
    queryAdd.bindValue(":fidorch", iEntryData.key.fIdOrCh);
    queryAdd.bindValue(":filepath", iEntryData.S0Ws.filePath);
    queryAdd.bindValue(":filename", iEntryData.S0Ws.fileName);
    queryAdd.bindValue(":scanlevel", (int)EScanLevel::SL0_Init);
    queryAdd.bindValue(":len", iEntryData.S0Ws.fileLengthMB);

    break;

  case EDbDataType::UpdateSL1Failed:
    queryAdd.prepare("UPDATE " + sTabEnsList + " SET " +
                     sTeScanLevel + " = :scanlevel " +
                     "WHERE " + sTeMode + " = :mode AND " + sTeFIdOrCh + " = :fidorch");
  
    queryAdd.bindValue(":fidorch", iEntryData.key.fIdOrCh);
    queryAdd.bindValue(":scanlevel", (int)EScanLevel::SL1_ScanFailed);
    break;

  case EDbDataType::UpdateSL2FibData:
    queryAdd.prepare("UPDATE " + sTabEnsList + " SET " +
                     sTeErrorProtection  + " = :errprot, " +
                     sTeNumDabDabPlus    + " = :dabdabplus, " +
                     sTeDscTyAppTy       + " = :dscTyAppTy, " +
                     sTeAudioDataRates   + " = :datarates, " +
                     sTeScanLevel        + " = :scanlevel, " +
                     sTeLastPlayedSId    + " = :lastplayedsid " +
                     "WHERE " + sTeMode + " = :mode AND " + sTeFIdOrCh + " = :fidorch");

    queryAdd.bindValue(":fidorch", iEntryData.key.fIdOrCh);
    queryAdd.bindValue(":errprot", iEntryData.S1Fib.errorProtection);
    queryAdd.bindValue(":dabdabplus", iEntryData.S1Fib.numDabDabPlus);
    queryAdd.bindValue(":dscTyAppTy", iEntryData.S1Fib.dscTyAppTy);
    queryAdd.bindValue(":datarates", iEntryData.S1Fib.audioDataRates);
    queryAdd.bindValue(":scanlevel", (int)EScanLevel::SL2_FibData);
    queryAdd.bindValue(":lastplayedsid", iEntryData.S0Ws.sIdPlayed);
    break;

  case EDbDataType::UpdateSL3MedRunData:
    queryAdd.prepare("UPDATE " + sTabEnsList + " SET " +
                      sTeEnsembleName   + " = :ensname, " +
                      sTeEnsembleId     + " = :ensid, " +
                      sTeItuCode        + " = :itucode, " +
                      sTeDateUtc        + " = :dateutc, " +
                      sTeSNR            + " = :snr, " +
                      sTeMER            + " = :mer, " +
                      sTeBasebandOffset + " = :offset, " +
                      sTeNomFreq        + " = :freq, " +
                      sTeScanLevel      + " = :scanlevel " +
                      "WHERE " + sTeMode + " = :mode AND " + sTeFIdOrCh + " = :fidorch");

    queryAdd.bindValue(":fidorch", iEntryData.key.fIdOrCh);
    queryAdd.bindValue(":scanlevel", (int)EScanLevel::SL3_MedRun);
    queryAdd.bindValue(":ensname", iEntryData.S2MedRun.ensembleName);
    queryAdd.bindValue(":ensid", iEntryData.S2MedRun.ensembleId);
    queryAdd.bindValue(":itucode", iEntryData.S2MedRun.ituCountry);
    queryAdd.bindValue(":dateutc", iEntryData.S2MedRun.dateUtc);
    queryAdd.bindValue(":snr", iEntryData.S2MedRun.snr);
    queryAdd.bindValue(":mer", iEntryData.S2MedRun.mer);
    queryAdd.bindValue(":offset", iEntryData.S2MedRun.basebandOffset);
    queryAdd.bindValue(":freq", iEntryData.S2MedRun.nomFreqkHz);
    break;

  case EDbDataType::UpdateLastPlayedSId:
    queryAdd.prepare("UPDATE " + sTabEnsList + " SET " +
                     sTeLastPlayedSId + " = :lastplayedsid " +
                     "WHERE " + sTeMode + " = :mode AND " + sTeFIdOrCh + " = :fidorch");
  
    queryAdd.bindValue(":fidorch", iEntryData.key.fIdOrCh);
    queryAdd.bindValue(":lastplayedsid", iEntryData.S0Ws.sIdPlayed);
    break;
  }

  queryAdd.bindValue(":mode", _cur_mode());

  return _exec(queryAdd, "insert/update entry");
}

EnsembleListDB::SDbEntryData EnsembleListDB::get_entry(const QString& iFIdOrCh) const
{
  QSqlQuery query(mDB);
  query.prepare("SELECT " +
                sTeFIdOrCh          + "," +
                sTeFilePath         + "," +
                sTeFileName         + "," +
                sTeLastPlayedSId    +
                " FROM " + sTabEnsList + " WHERE " + sTeMode + " = :mode AND " + sTeFIdOrCh + " = :fidorch");
  query.bindValue(":mode", _cur_mode());
  query.bindValue(":fidorch", iFIdOrCh);

  SDbEntryData entryData{};

  if (query.exec() && query.next())
  {
    entryData.key.fIdOrCh    = query.value(sTeFIdOrCh).toString();
    entryData.S0Ws.filePath  = query.value(sTeFilePath).toString();
    entryData.S0Ws.fileName  = query.value(sTeFileName).toString();
    entryData.S0Ws.sIdPlayed = query.value(sTeLastPlayedSId).toString();
  }

  return entryData;
}

bool EnsembleListDB::is_entry_existing(const QString& iFIdOrCh) const
{
  QSqlQuery query(mDB);
  query.prepare("SELECT " + sTeFIdOrCh + " FROM " + sTabEnsList + " WHERE " + sTeMode + " = :mode AND " + sTeFIdOrCh + " = :fidorch");
  query.bindValue(":mode", _cur_mode());
  query.bindValue(":fidorch", iFIdOrCh);

  return query.exec() && query.next();
}

bool EnsembleListDB::delete_entry(const QString& iFIdOrCh) const
{
  QSqlQuery queryDel(mDB);
  queryDel.prepare("DELETE FROM " + sTabEnsList + " WHERE " + sTeMode + " = :mode AND " + sTeFIdOrCh + " = :fidorch");
  queryDel.bindValue(":mode", _cur_mode());
  queryDel.bindValue(":fidorch", iFIdOrCh);

  return _exec(queryDel, "delete entry");
}

bool EnsembleListDB::delete_invalid_entries() const
{
  QSqlQuery queryDel(mDB);
  queryDel.prepare("DELETE FROM " + sTabEnsList + " WHERE " + sTeMode + " = :mode AND " + sTeScanLevel + " = " + QString::number((int)EScanLevel::SL1_ScanFailed));
  queryDel.bindValue(":mode", _cur_mode());

  return _exec(queryDel, "delete invalid entries");
}

i32 EnsembleListDB::get_nr_unscanned_entries() const
{
  QSqlQuery query(mDB);
  query.prepare("SELECT COUNT(*) FROM " + sTabEnsList + " WHERE " + sTeMode + " = :mode AND (" + sTeEnsembleId + " IS NULL OR " + sTeEnsembleId + " = '')");
  query.bindValue(":mode", _cur_mode());
  return (query.exec() && query.next() ? query.value(0).toInt() : -1);
}

EnsembleListDB::SEntryCounts EnsembleListDB::get_entry_counts() const
{
  QSqlQuery query(mDB);
  query.prepare("SELECT COUNT(*), " +
                QSL("COALESCE(SUM(%1 = %2), 0), ").arg(sTeScanLevel).arg((int)EScanLevel::SL0_Init) +
                QSL("COALESCE(SUM(%1 = %2), 0), ").arg(sTeScanLevel).arg((int)EScanLevel::SL1_ScanFailed) +
                QSL("COALESCE(SUM(%1 >= %2), 0)").arg(sTeScanLevel).arg((int)EScanLevel::SL2_FibData) +
                " FROM " + sTabEnsList + " WHERE " + sTeMode + " = :mode");
  query.bindValue(":mode", _cur_mode());

  SEntryCounts counts;

  if (_exec(query, "count entries") && query.next())
  {
    counts.total      = query.value(0).toInt();
    counts.notScanned = query.value(1).toInt();
    counts.noSignal   = query.value(2).toInt();
    counts.valid      = query.value(3).toInt();
  }

  return counts;
}

QAbstractItemModel* EnsembleListDB::create_model()
{
  auto* const model = new QSqlQueryModel(this);

  QStringList cols;
  cols << "COALESCE(" + sel_user_data_col(sTuIsFav) + ", 0) AS 'Fav'"; // see CI_Fav
  if (mDataMode == EDataMode::Files)
  {
    cols << sTeFileName        + " AS 'FileName'";
    cols << sTeFIdOrCh         + " AS 'FId'";  // see CI_FId
    cols << sTeFileLengthMB    + " AS 'Size\n(MB)'";
  }
  else
  {
    cols << sTeFIdOrCh         + " AS 'Ch'"; // see CI_CH
  }
  cols << sTeScanLevel         + " AS 'SL'"; // see CI_CSL and CI_FSL
  cols << sTeEnsembleName      + " AS 'Ensemble\nName'";
  cols << "COALESCE(" + sel_user_data_col(sTuNote) + ", '') AS 'Note'"; // see CI_CNote and CI_FNote
  cols << sTeEnsembleId        + " AS 'EId'";
  cols << sTeItuCode           + " AS 'ITU\nCode'";
  cols << sTeLastPlayedSId     + " AS 'Played\nSId'";
  cols << sTeNumDabDabPlus     + " AS 'Audio\nDAB|DAB+'";
  cols << sTeAudioDataRates    + " AS 'Data Rates\n[kbps]'";
  cols << sTeErrorProtection   + " AS 'Protection\nLevels'";
  cols << sTeDscTyAppTy        + " AS 'DSCTy[AppTy]'";
  cols << sTeSNR               + " AS 'SNR\n(dB)'";
  cols << sTeMER               + " AS 'MER\n(dB)'";
  cols << sTeBasebandOffset    + " AS 'Offset\n(Hz)'";
  cols << sTeNomFreq           + " AS 'Freq\n(kHz)'";
  if (mDataMode == EDataMode::Files)
  {
    cols << sTeDateUtc         + " AS 'Date\n(UTC)'";
    cols << sTeFilePath        + " AS 'FilePath'";
  }

  QStringList scanLevelConditions;
  if (mDataFilter.showNotScannedEntries)
  {
    scanLevelConditions << sTeScanLevel + " = " + QString::number((int)EScanLevel::SL0_Init);
  }
  if (mDataFilter.showNoSignalEntries)
  {
    scanLevelConditions << sTeScanLevel + " = " + QString::number((int)EScanLevel::SL1_ScanFailed);
  }
  if (mDataFilter.showScannedEntries)
  {
    scanLevelConditions << sTeScanLevel + " >= " + QString::number((int)EScanLevel::SL2_FibData);
  }

  const QString scanLevelFilter = scanLevelConditions.isEmpty() ? "1=0" : "(" + scanLevelConditions.join(" OR ") + ")";

  QSqlQuery query(mDB);
  const QString queryStr = "SELECT " + cols.join(",") + " FROM " + sTabEnsList + " WHERE " + sTeMode + " = :mode AND " + scanLevelFilter;
  query.prepare(queryStr);
  query.bindValue(":mode", _cur_mode());

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

QString EnsembleListDB::get_full_path(const QString& iFId) const
{
  assert(mDataMode == EDataMode::Files);
  QSqlQuery query(mDB);
  query.prepare("SELECT " + sTeFilePath + ", " + sTeFileName + " FROM " + sTabEnsList + " WHERE " + sTeMode + " = :mode AND " + sTeFIdOrCh + " = :fidorch");
  query.bindValue(":mode", _cur_mode());
  query.bindValue(":fidorch", iFId);

  if (query.exec() && query.next())
  {
    const QString path = query.value(0).toString();
    QString name = query.value(1).toString(); // no const to get moved below

    if (path.isEmpty())
    {
      return name;
    }

    return QDir(path).filePath(name);
  }

  return {};
}

EnsembleListDB::SUserData EnsembleListDB::get_user_data(const QString& iFIdOrCh) const
{
  QSqlQuery query(mDB);
  query.prepare("SELECT " + sTuIsFav + ", " + sTuNote + " FROM " + sTabUserData + " WHERE " + sTeMode + " = :mode AND " + sTeFIdOrCh + " = :fidorch");
  query.bindValue(":mode", _cur_mode());
  query.bindValue(":fidorch", iFIdOrCh);

  SUserData userData;

  if (query.exec() && query.next())
  {
    userData.isFav = query.value(0).toBool();
    userData.note = query.value(1).toString();
  }

  return userData;
}

bool EnsembleListDB::set_favorite(const QString& iFIdOrCh, const bool iIsFav) const
{
  QSqlQuery query(mDB);
  query.prepare("INSERT INTO " + sTabUserData + " (" + sTeMode + "," + sTeFIdOrCh + "," + sTuIsFav + ") VALUES (:mode, :fidorch, :isfav) "
                "ON CONFLICT(" + sTeMode + "," + sTeFIdOrCh + ") DO UPDATE SET " + sTuIsFav + " = excluded." + sTuIsFav);
  query.bindValue(":mode", _cur_mode());
  query.bindValue(":fidorch", iFIdOrCh);
  query.bindValue(":isfav", iIsFav ? 1 : 0);

  return _exec(query, "update ensemble favorite") && _delete_empty_user_data(iFIdOrCh);
}

bool EnsembleListDB::set_note(const QString& iFIdOrCh, const QString& iNote) const
{
  QSqlQuery query(mDB);
  query.prepare("INSERT INTO " + sTabUserData + " (" + sTeMode + "," + sTeFIdOrCh + "," + sTuNote + ") VALUES (:mode, :fidorch, :note) "
                "ON CONFLICT(" + sTeMode + "," + sTeFIdOrCh + ") DO UPDATE SET " + sTuNote + " = excluded." + sTuNote);
  query.bindValue(":mode", _cur_mode());
  query.bindValue(":fidorch", iFIdOrCh);
  query.bindValue(":note", iNote.isNull() ? QString("") : iNote); // NOT NULL column

  return _exec(query, "update ensemble note") && _delete_empty_user_data(iFIdOrCh);
}

bool EnsembleListDB::_delete_empty_user_data(const QString& iFIdOrCh) const
{
  // do not keep entries without content
  QSqlQuery query(mDB);
  query.prepare("DELETE FROM " + sTabUserData + " WHERE " + sTeMode + " = :mode AND " + sTeFIdOrCh + " = :fidorch AND " +
                sTuIsFav + " = 0 AND " + sTuNote + " = ''");
  query.bindValue(":mode", _cur_mode());
  query.bindValue(":fidorch", iFIdOrCh);

  return _exec(query, "delete empty user data");
}

void EnsembleListDB::set_data_filter(const SDataFilter& iDataFilter)
{
  mDataFilter = iDataFilter;
}

bool EnsembleListDB::_exec(QSqlQuery& ioQuery, const char* const iWhat) const
{
  if (!ioQuery.exec())
  {
    qCritical() << "Error: Unable to" << iWhat << ":" << ioQuery.lastError().text();
    return false;
  }

  return true;
}

i32 EnsembleListDB::_cur_mode() const
{
  assert(mDataMode != EDataMode::Invalid);
  return (i32)(mDataMode == EDataMode::Device ? DabDatabase::EMode::Device : DabDatabase::EMode::File);
}

void EnsembleListDB::set_data_mode(EnsembleListDB::EDataMode iDataMode)
{
  mDataMode = iDataMode;
}
