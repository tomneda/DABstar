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
#include "glob_enums.h"
#include <QFile>
#include <QTableView>
#include <QtSql/QSql>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQueryModel>
#include <QVariant>
#include <QtDebug>
#include <QCoreApplication>
#include <QDir>
#include <utility>

static const QString sTabDeviceEnsList = "TabDeviceEnsList";
static const QString sTabFileEnsList   = "TabFileEnsList";

static const QString sTeFIdOrCh             = "FIdOrCh";
static const QString sTeFilePath            = "FilePath";
static const QString sTeFileName            = "FileName";
static const QString sTeScanLevel           = "ScanLevel";
static const QString sTeEnsembleName        = "EnsembleName";
static const QString sTeEnsembleId          = "EnsembleId";
static const QString sTeItuCode             = "ItuCode";
static const QString sTeNumDabDabPlus       = "NumDabDabPlus";
static const QString sTeDscTyAppTy          = "DscTyAppTy";
static const QString sTeAudioDataRates      = "AudioDataRates";
static const QString sTeErrorProtection     = "ErrorProtection";
static const QString sTeFileLengthMB        = "FileLengthMB";
static const QString sTeSNR                 = "SNR";
static const QString sTeMER                 = "MER";
static const QString sTeBasebandOffset      = "BaseBandOffset";
static const QString sTeNomFreq             = "NomFreq";
static const QString sTeDateUtc             = "Date";
static const QString sTeLastPlayedSId       = "LastPlayedSId";

EnsembleListDB::EnsembleListDB(QString iDbFileName) :
  mDbFileName(std::move(iDbFileName)) // clang-tidy wants it so...
{
}

EnsembleListDB::~EnsembleListDB()
{
  mDB.close();
}

void EnsembleListDB::open_db()
{
  mDB = QSqlDatabase::addDatabase("QSQLITE", "EnsembleList");

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

void EnsembleListDB::create_table()
{
  const QString queryStr = "CREATE TABLE IF NOT EXISTS " + _cur_tab_name() + " ("
                           "Id                        INTEGER PRIMARY KEY,"
                           + sTeFIdOrCh           + " TEXT NOT NULL,"
                           // only for files
                           + sTeFilePath          + " TEXT,"
                           + sTeFileName          + " TEXT,"
                           + sTeScanLevel         + " INTEGER DEFAULT 0,"
                           + sTeFileLengthMB      + " REAL DEFAULT 0,"
                           // common for files and device
                           + sTeEnsembleName      + " TEXT,"
                           + sTeEnsembleId        + " TEXT,"
                           + sTeItuCode           + " TEXT,"
                           + sTeDateUtc           + " TEXT,"
                           + sTeLastPlayedSId     + " TEXT,"
                           + sTeDscTyAppTy        + " TEXT,"
                           + sTeAudioDataRates    + " TEXT,"
                           + sTeNumDabDabPlus     + " TEXT,"
                           + sTeErrorProtection   + " TEXT,"
                           + sTeNomFreq           + " INTEGER DEFAULT 0,"
                           + sTeSNR               + " REAL DEFAULT 0,"
                           + sTeMER               + " REAL DEFAULT 0,"
                           + sTeBasebandOffset    + " REAL DEFAULT 0,"
                           "UNIQUE(" + sTeFIdOrCh + ") ON CONFLICT REPLACE"
                           ");";

  _exec_simple_query(queryStr);
}

void EnsembleListDB::delete_table()
{
  _exec_simple_query("DROP TABLE IF EXISTS " + _cur_tab_name() + ";");
}

bool EnsembleListDB::is_table_existing(const EDataMode iDataMode) const
{
  assert(iDataMode != EDataMode::Invalid);
  const QString & tabName = (iDataMode == EDataMode::Device ? sTabDeviceEnsList : sTabFileEnsList);
  return mDB.tables().contains(tabName);
}

bool EnsembleListDB::insert_or_update_entry(const SDbEntryData & iEntryData, const EDbDataType iDataType) const
{
  // add new entry (UNIQUE constraint on FId will cause REPLACE if it already exists due to CREATE TABLE definition)
  QSqlQuery queryAdd(mDB);

  switch (iDataType)
  {
  case EDbDataType::InsertKeyAndS0WsData:
    queryAdd.prepare("INSERT INTO " + _cur_tab_name() + " (" +
                     sTeFIdOrCh + "," +
                     sTeFilePath + "," +
                     sTeFileName + "," +
                     sTeScanLevel + "," +
                     sTeFileLengthMB +
                     ") VALUES ("
                     ":fidorch, "
                     ":filepath, "
                     ":filename, "
                     ":scanlevel, "
                     ":len"
                     ")");
    
    queryAdd.bindValue(":fidorch", iEntryData.key.fIdOrCh);
    queryAdd.bindValue(":filepath", iEntryData.S0Ws.filePath);
    queryAdd.bindValue(":filename", iEntryData.S0Ws.fileName);
    queryAdd.bindValue(":scanlevel", (int)EScanLevel::SL0_Init);
    queryAdd.bindValue(":len", iEntryData.S0Ws.fileLengthMB);

    break;

  case EDbDataType::UpdateSL1Failed:
    queryAdd.prepare("UPDATE " + _cur_tab_name() + " SET " +
                     sTeScanLevel + " = :scanlevel " +
                     "WHERE " + sTeFIdOrCh + " = :fidorch");
  
    queryAdd.bindValue(":fidorch", iEntryData.key.fIdOrCh);
    queryAdd.bindValue(":scanlevel", (int)EScanLevel::SL1_ScanFailed);
    break;

  case EDbDataType::UpdateSL2FibData:
    queryAdd.prepare("UPDATE " + _cur_tab_name() + " SET " +
                     sTeErrorProtection  + " = :errprot, " +
                     sTeNumDabDabPlus    + " = :dabdabplus, " +
                     sTeDscTyAppTy       + " = :dscTyAppTy, " +
                     sTeAudioDataRates   + " = :datarates, " +
                     sTeScanLevel        + " = :scanlevel, " +
                     sTeLastPlayedSId    + " = :lastplayedsid " +
                     "WHERE " + sTeFIdOrCh + " = :fidorch");

    queryAdd.bindValue(":fidorch", iEntryData.key.fIdOrCh);
    queryAdd.bindValue(":errprot", iEntryData.S1Fib.errorProtection);
    queryAdd.bindValue(":dabdabplus", iEntryData.S1Fib.numDabDabPlus);
    queryAdd.bindValue(":dscTyAppTy", iEntryData.S1Fib.dscTyAppTy);
    queryAdd.bindValue(":datarates", iEntryData.S1Fib.audioDataRates);
    queryAdd.bindValue(":scanlevel", (int)EScanLevel::SL2_FibData);
    queryAdd.bindValue(":lastplayedsid", iEntryData.S0Ws.sIdPlayed);
    break;

  case EDbDataType::UpdateSL3MedRunData:
    queryAdd.prepare("UPDATE " + _cur_tab_name() + " SET " +
                      sTeEnsembleName   + " = :ensname, " +
                      sTeEnsembleId     + " = :ensid, " +
                      sTeItuCode        + " = :itucode, " +
                      sTeDateUtc        + " = :dateutc, " +
                      sTeSNR            + " = :snr, " +
                      sTeMER            + " = :mer, " +
                      sTeBasebandOffset + " = :offset, " +
                      sTeNomFreq        + " = :freq, " +
                      sTeScanLevel      + " = :scanlevel " +
                      "WHERE " + sTeFIdOrCh + " = :fidorch");

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
    queryAdd.prepare("UPDATE " + _cur_tab_name() + " SET " +
                     sTeLastPlayedSId + " = :lastplayedsid " +
                     "WHERE " + sTeFIdOrCh + " = :fidorch");
  
    queryAdd.bindValue(":fidorch", iEntryData.key.fIdOrCh);
    queryAdd.bindValue(":lastplayedsid", iEntryData.S0Ws.sIdPlayed);
    break;
  }

  if (!queryAdd.exec())
  {
    const QString dbErr = _error_str();
    qCritical() << "Error: Unable insert/update entry: " << dbErr;
    return false;
  }

  return true;
}

EnsembleListDB::SDbEntryData EnsembleListDB::get_entry(const QString & iFIdOrCh) const
{
  QSqlQuery query(mDB);
  query.prepare("SELECT " +
                sTeFIdOrCh          + "," +
                sTeFilePath         + "," +
                sTeFileName         + "," +
                sTeLastPlayedSId    +
                " FROM " + _cur_tab_name() + " WHERE " + sTeFIdOrCh + " = :fidorch");
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

bool EnsembleListDB::is_entry_existing(const QString & iFIdOrCh) const
{
  QSqlQuery query(mDB);
  query.prepare("SELECT " + sTeFIdOrCh + " FROM " + _cur_tab_name() + " WHERE " + sTeFIdOrCh + " = :fidorch");
  query.bindValue(":fidorch", iFIdOrCh);

  return query.exec() && query.next();
}

bool EnsembleListDB::delete_entry(const QString & iFIdOrCh) const
{
  QSqlQuery queryDel(mDB);
  queryDel.prepare("DELETE FROM " + _cur_tab_name() + " WHERE " + sTeFIdOrCh + " = :fidorch");
  queryDel.bindValue(":fidorch", iFIdOrCh);

  if (!queryDel.exec())
  {
    const QString dbErr = _error_str();
    qCritical() << "Error: Unable delete entry: " << dbErr;
    return false;
  }

  return true;
}

bool EnsembleListDB::delete_invalid_entries() const
{
  QSqlQuery queryDel(mDB);
  queryDel.prepare("DELETE FROM " + _cur_tab_name() + " WHERE " + sTeScanLevel + " = " + QString::number((int)EScanLevel::SL1_ScanFailed));

  if (!queryDel.exec())
  {
    const QString dbErr = _error_str();
    qCritical() << "Error: Unable delete entry: " << dbErr;
    return false;
  }

  return true;
}

i32 EnsembleListDB::get_nr_unscanned_entries() const
{
  QSqlQuery query(mDB);
  query.prepare("SELECT COUNT(*) FROM " + _cur_tab_name() + " WHERE " + sTeEnsembleId + " IS NULL OR " + sTeEnsembleId + " = ''");
  return (query.exec() && query.next() ? query.value(0).toInt() : -1);
}

QAbstractItemModel * EnsembleListDB::create_model()
{
  auto * const model = new QSqlQueryModel(this);

  QStringList cols;
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
  QString queryStr = "SELECT " + cols.join(",") + " FROM " + _cur_tab_name() + " WHERE " + scanLevelFilter;
  query.prepare(queryStr);

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
    const QString dbErr = _error_str();
    qCritical() << "Error: Invalid SELECT query: " << dbErr;
  }

  return model;
}

QString EnsembleListDB::get_full_path(const QString & iFId) const
{
  assert(mDataMode == EDataMode::Files);
  QSqlQuery query(mDB);
  query.prepare("SELECT " + sTeFilePath + ", " + sTeFileName + " FROM " + _cur_tab_name() + " WHERE " + sTeFIdOrCh + " = :fidorch");
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

void EnsembleListDB::set_data_filter(const SDataFilter & iDataFilter)
{
  mDataFilter = iDataFilter;
}

bool EnsembleListDB::_open_db()
{
  mDB.setDatabaseName(mDbFileName);
  return mDB.open();
}

void EnsembleListDB::_delete_db_file()
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

QString EnsembleListDB::_error_str() const
{
  return mDB.lastError().text(); // make a copy of the string as the DB object could be invalid when using it
}

void EnsembleListDB::_exec_simple_query(const QString & iQuery)
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

const QString & EnsembleListDB::_cur_tab_name() const
{
  assert(mDataMode != EDataMode::Invalid);
  return (mDataMode == EDataMode::Device ? sTabDeviceEnsList : sTabFileEnsList);
}

void EnsembleListDB::set_data_mode(EnsembleListDB::EDataMode iDataMode)
{
  mDataMode = iDataMode;
}
