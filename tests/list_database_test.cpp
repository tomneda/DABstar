#include "dab_database.h"
#include "ensemble_list_db.h"
#include "service_db.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <cstdio>
#include <memory>
#include <utility>

namespace
{
bool check(const char* const iName, const bool iCondition)
{
  if (!iCondition)
  {
    std::fprintf(stderr, "FAILED: %s\n", iName);
  }
  return iCondition;
}

i32 count_rows(const QSqlDatabase& iDb, const QString& iQuery)
{
  QSqlQuery query(iDb);
  return (query.exec(iQuery) && query.next() ? query.value(0).toInt() : -1);
}

// returns the favorite state of the given service in the service list model, -1 if the service is not in the list
i32 get_fav_state(ServiceDB& ioServiceDb, const QString& iChannel, const u32 iSId)
{
  const std::unique_ptr<QAbstractItemModel> model(ioServiceDb.create_model(QString()));

  for (i32 rowIdx = 0; rowIdx < model->rowCount(); ++rowIdx)
  {
    if (model->data(model->index(rowIdx, ServiceDB::CI_Channel)).toString() == iChannel &&
        model->data(model->index(rowIdx, ServiceDB::CI_SId)).toUInt() == iSId)
    {
      return model->data(model->index(rowIdx, ServiceDB::CI_Fav)).toInt();
    }
  }
  return -1;
}

EnsembleListDB::SDbEntryData make_entry(const QString& iFIdOrCh)
{
  EnsembleListDB::SDbEntryData ed{};
  ed.key.fIdOrCh = iFIdOrCh;
  return ed;
}

bool test_relations_and_favorites(const QString& iDbFileName)
{
  bool ok = true;
  DabDatabase dabDb(iDbFileName);
  ok &= check("open new database", dabDb.open_db() == DabDatabase::EOpenResult::Ok);

  EnsembleListDB ensDb(dabDb);
  ServiceDB srvDb(dabDb);
  const QSqlDatabase& db = dabDb.get_db();

  // device mode: two channels with services
  ensDb.set_data_mode(EnsembleListDB::EDataMode::Device);
  srvDb.set_data_mode(ServiceDB::EDataMode::DevicePlayer);
  ok &= check("insert 5C", ensDb.insert_or_update_entry(make_entry("5C"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("insert 11D", ensDb.insert_or_update_entry(make_entry("11D"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("add service 5C/1", srvDb.add_entry("5C", "Radio A", 0x1101));
  ok &= check("add service 5C/2", srvDb.add_entry("5C", "Radio B", 0x1102));
  ok &= check("add service 11D/1", srvDb.add_entry("11D", "Radio A", 0x1101)); // same service on another channel
  ok &= check("update service label", srvDb.add_entry("5C", "Radio B new", 0x1102));
  ok &= check("three services", count_rows(db, "SELECT COUNT(*) FROM Service") == 3);
  ok &= check("channel list", srvDb.get_list_of_channels().size() == 2);

  // file mode is separated
  ensDb.set_data_mode(EnsembleListDB::EDataMode::Files);
  srvDb.set_data_mode(ServiceDB::EDataMode::FilePlayer);
  ok &= check("file mode empty", !ensDb.is_entry_existing("5C"));
  EnsembleListDB::SDbEntryData fileEntry = make_entry("a1b2c3d4");
  fileEntry.S0Ws.filePath = "/tmp";
  fileEntry.S0Ws.fileName = "rec.sdr";
  ok &= check("insert file", ensDb.insert_or_update_entry(fileEntry, EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("file full path", ensDb.get_full_path("a1b2c3d4") == "/tmp/rec.sdr");
  ok &= check("add file service", srvDb.add_entry("a1b2c3d4", "Radio A", 0x1101));
  ok &= check("file channel list", srvDb.get_list_of_channels() == QList<QString>{ "a1b2c3d4" });

  // favorites are related to mode, channel/FId and SId
  srvDb.set_data_mode(ServiceDB::EDataMode::DevicePlayer);
  srvDb.set_favorite("5C", 0x1101, true);
  ok &= check("fav at 5C", get_fav_state(srvDb, "5C", 0x1101) == 1);
  ok &= check("no fav at 11D with same SId", get_fav_state(srvDb, "11D", 0x1101) == 0);
  ok &= check("other service no fav", get_fav_state(srvDb, "5C", 0x1102) == 0);
  srvDb.set_favorite("11D", 0x1101, true);
  srvDb.set_favorite("5C", 0x1101, false);
  ok &= check("fav removed at 5C", get_fav_state(srvDb, "5C", 0x1101) == 0);
  ok &= check("fav kept at 11D", get_fav_state(srvDb, "11D", 0x1101) == 1);
  srvDb.set_data_mode(ServiceDB::EDataMode::FilePlayer);
  ok &= check("no fav in file mode with same SId", get_fav_state(srvDb, "a1b2c3d4", 0x1101) == 0);
  srvDb.set_data_mode(ServiceDB::EDataMode::DevicePlayer);

  // the ECC is stored in the ensemble entry
  ok &= check("set ECC 5C", srvDb.set_ecc("5C", 0xE0));
  ok &= check("set ECC 5C again no change", !srvDb.set_ecc("5C", 0xE0));
  ok &= check("ECC stored", count_rows(db, "SELECT Ecc FROM Ensemble WHERE Mode = 0 AND FIdOrCh = '5C'") == 0xE0);
  ensDb.set_data_mode(EnsembleListDB::EDataMode::Device);

  // re-init of an ensemble entry keeps its services
  ok &= check("re-init 5C", ensDb.insert_or_update_entry(make_entry("5C"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("services kept after re-init", count_rows(db, "SELECT COUNT(*) FROM Service s JOIN Ensemble e ON e.Id = s.EnsembleRef WHERE e.FIdOrCh = '5C'") == 2);

  // deleting an ensemble entry deletes its services (cascade), but keeps the favorites
  ok &= check("delete 5C", ensDb.delete_entry("5C"));
  ok &= check("services of 5C deleted", count_rows(db, "SELECT COUNT(*) FROM Service") == 2);
  ensDb.delete_table();
  ok &= check("device services deleted", count_rows(db, "SELECT COUNT(*) FROM Service") == 1);
  ok &= check("file entry kept", count_rows(db, "SELECT COUNT(*) FROM Ensemble WHERE Mode = 1") == 1);
  ok &= check("favorites kept", count_rows(db, "SELECT COUNT(*) FROM Favorite") == 1);

  return ok;
}

// returns the favorite state and note of the given entry in the ensemble list model, isFav is -1 if the entry is not in the list
std::pair<i32, QString> get_ens_user_data(EnsembleListDB& ioEnsDb, const QString& iFIdOrCh)
{
  const std::unique_ptr<QAbstractItemModel> model(ioEnsDb.create_model());
  const bool isDevice = (ioEnsDb.get_data_mode() == EnsembleListDB::EDataMode::Device);
  const i32 keyCol = (isDevice ? EnsembleListDB::CI_CH : EnsembleListDB::CI_FId);
  const i32 noteCol = (isDevice ? EnsembleListDB::CI_CNote : EnsembleListDB::CI_FNote);

  for (i32 rowIdx = 0; rowIdx < model->rowCount(); ++rowIdx)
  {
    if (model->data(model->index(rowIdx, keyCol)).toString() == iFIdOrCh)
    {
      return { model->data(model->index(rowIdx, EnsembleListDB::CI_Fav)).toInt(), model->data(model->index(rowIdx, noteCol)).toString() };
    }
  }
  return { -1, QString() };
}

bool test_ensemble_user_data(const QString& iDbFileName)
{
  bool ok = true;
  DabDatabase dabDb(iDbFileName);
  ok &= check("open user data database", dabDb.open_db() == DabDatabase::EOpenResult::Ok);

  EnsembleListDB ensDb(dabDb);
  const QSqlDatabase& db = dabDb.get_db();

  ensDb.set_data_mode(EnsembleListDB::EDataMode::Device);
  ok &= check("insert 5C", ensDb.insert_or_update_entry(make_entry("5C"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("insert 11D", ensDb.insert_or_update_entry(make_entry("11D"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("no user data initially", get_ens_user_data(ensDb, "5C") == std::make_pair(0, QString()));

  ok &= check("set fav 5C", ensDb.set_favorite("5C", true));
  ok &= check("set note 5C", ensDb.set_note("5C", "Good reception on the balcony"));
  ok &= check("model shows fav and note", get_ens_user_data(ensDb, "5C") == std::make_pair(1, QString("Good reception on the balcony")));
  ok &= check("other entry without user data", get_ens_user_data(ensDb, "11D") == std::make_pair(0, QString()));
  const EnsembleListDB::SUserData ud = ensDb.get_user_data("5C");
  ok &= check("get user data", ud.isFav && ud.note == "Good reception on the balcony");

  // file mode is separated
  ensDb.set_data_mode(EnsembleListDB::EDataMode::Files);
  ok &= check("insert file 5C", ensDb.insert_or_update_entry(make_entry("5C"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("file mode without user data", get_ens_user_data(ensDb, "5C") == std::make_pair(0, QString()));
  ok &= check("file mode get user data empty", !ensDb.get_user_data("5C").isFav);
  ensDb.set_data_mode(EnsembleListDB::EDataMode::Device);

  // the user data survives re-init, deleting and resetting of the ensemble list
  ok &= check("re-init 5C", ensDb.insert_or_update_entry(make_entry("5C"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("user data kept after re-init", get_ens_user_data(ensDb, "5C").first == 1);
  ensDb.delete_table();
  ok &= check("user data kept after delete", count_rows(db, "SELECT COUNT(*) FROM EnsembleUserData") == 1);
  ok &= check("re-add 5C", ensDb.insert_or_update_entry(make_entry("5C"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("user data shown again", get_ens_user_data(ensDb, "5C") == std::make_pair(1, QString("Good reception on the balcony")));

  // clearing fav and note removes the row
  ok &= check("clear fav 5C", ensDb.set_favorite("5C", false));
  ok &= check("row kept with note", count_rows(db, "SELECT COUNT(*) FROM EnsembleUserData") == 1);
  ok &= check("clear note 5C", ensDb.set_note("5C", QString()));
  ok &= check("empty row removed", count_rows(db, "SELECT COUNT(*) FROM EnsembleUserData") == 0);

  return ok;
}

bool is_counts(const EnsembleListDB::SEntryCounts& iCounts, const i32 iTotal, const i32 iNotScanned, const i32 iNoSignal, const i32 iValid)
{
  return iCounts.total == iTotal && iCounts.notScanned == iNotScanned && iCounts.noSignal == iNoSignal && iCounts.valid == iValid;
}

bool test_ensemble_entry_counts(const QString& iDbFileName)
{
  bool ok = true;
  DabDatabase dabDb(iDbFileName);
  ok &= check("open entry counts database", dabDb.open_db() == DabDatabase::EOpenResult::Ok);

  EnsembleListDB ensDb(dabDb);

  ensDb.set_data_mode(EnsembleListDB::EDataMode::Device);
  ok &= check("empty counts", is_counts(ensDb.get_entry_counts(), 0, 0, 0, 0));

  ok &= check("insert 5C", ensDb.insert_or_update_entry(make_entry("5C"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("insert 11D", ensDb.insert_or_update_entry(make_entry("11D"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("insert 12C", ensDb.insert_or_update_entry(make_entry("12C"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("5C failed", ensDb.insert_or_update_entry(make_entry("5C"), EnsembleListDB::EDbDataType::UpdateSL1Failed));
  ok &= check("11D fib data", ensDb.insert_or_update_entry(make_entry("11D"), EnsembleListDB::EDbDataType::UpdateSL2FibData));
  ok &= check("device counts", is_counts(ensDb.get_entry_counts(), 3, 1, 1, 1));

  ok &= check("11D med run data", ensDb.insert_or_update_entry(make_entry("11D"), EnsembleListDB::EDbDataType::UpdateSL3MedRunData));
  ok &= check("med run data counts as valid", is_counts(ensDb.get_entry_counts(), 3, 1, 1, 1));

  // the data filter has no influence on the counts
  EnsembleListDB::SDataFilter filter;
  filter.showNotScannedEntries = false;
  filter.showNoSignalEntries = false;
  ensDb.set_data_filter(filter);
  ok &= check("counts independent of filter", is_counts(ensDb.get_entry_counts(), 3, 1, 1, 1));
  ensDb.set_data_filter(EnsembleListDB::SDataFilter{});

  // file mode is counted separately
  ensDb.set_data_mode(EnsembleListDB::EDataMode::Files);
  ok &= check("file mode empty", is_counts(ensDb.get_entry_counts(), 0, 0, 0, 0));
  ok &= check("insert file", ensDb.insert_or_update_entry(make_entry("a1b2c3d4"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("file mode counts", is_counts(ensDb.get_entry_counts(), 1, 1, 0, 0));

  ensDb.set_data_mode(EnsembleListDB::EDataMode::Device);
  ok &= check("delete invalid", ensDb.delete_invalid_entries());
  ok &= check("counts after delete invalid", is_counts(ensDb.get_entry_counts(), 2, 1, 0, 1));
  ensDb.delete_table();
  ok &= check("counts after delete table", is_counts(ensDb.get_entry_counts(), 0, 0, 0, 0));

  return ok;
}

// executes a query via an own connection (like another program or DABstar instance), returns the first value
QVariant exec_raw(const QString& iDbFileName, const QString& iQuery)
{
  QVariant value;
  {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "Raw");
    db.setDatabaseName(iDbFileName);
    if (db.open())
    {
      QSqlQuery query(db);
      if (query.exec(iQuery) && query.next())
      {
        value = query.value(0);
      }
    }
    db.close();
  }
  QSqlDatabase::removeDatabase("Raw");
  return value;
}

bool test_open_handling(const QString& iDir)
{
  bool ok = true;
  const QString dbFileName = iDir + "/open.db";

  // new database gets the current schema version
  {
    DabDatabase dabDb(dbFileName);
    ok &= check("open fresh", dabDb.open_db() == DabDatabase::EOpenResult::Ok);
    ServiceDB srvDb(dabDb);
    srvDb.set_favorite("5C", 0x1101, true);
  }
  ok &= check("schema version set", exec_raw(dbFileName, "PRAGMA user_version").toInt() == DabDatabase::cSchemaVersion);

  // reopen keeps the content
  {
    DabDatabase dabDb(dbFileName);
    ok &= check("reopen", dabDb.open_db() == DabDatabase::EOpenResult::Ok);
  }
  ok &= check("favorite kept after reopen", exec_raw(dbFileName, "SELECT COUNT(*) FROM Favorite").toInt() == 1);

  // a database of a newer DABstar version must not be touched
  (void)exec_raw(dbFileName, "PRAGMA user_version = 99");
  {
    DabDatabase dabDb(dbFileName);
    ok &= check("newer version detected", dabDb.open_db() == DabDatabase::EOpenResult::NewerSchemaVersion);
  }
  ok &= check("newer version not deleted", exec_raw(dbFileName, "SELECT COUNT(*) FROM Favorite").toInt() == 1);
  ok &= check("newer version unchanged", exec_raw(dbFileName, "PRAGMA user_version").toInt() == 99);
  (void)exec_raw(dbFileName, QSL("PRAGMA user_version = %1").arg(DabDatabase::cSchemaVersion));

  // a database locked by another instance must not be deleted
  {
    QSqlDatabase lockDb = QSqlDatabase::addDatabase("QSQLITE", "Lock");
    lockDb.setDatabaseName(dbFileName);
    ok &= check("lock open", lockDb.open());
    QSqlQuery lockQuery(lockDb);
    ok &= check("lock exclusive", lockQuery.exec("BEGIN EXCLUSIVE"));
    {
      DabDatabase dabDb(dbFileName);
      ok &= check("locked database fails", dabDb.open_db() == DabDatabase::EOpenResult::Failed);
    }
    ok &= check("unlock", lockQuery.exec("ROLLBACK"));
    lockQuery.finish();
    lockDb.close();
  }
  QSqlDatabase::removeDatabase("Lock");
  ok &= check("locked database not deleted", exec_raw(dbFileName, "SELECT COUNT(*) FROM Favorite").toInt() == 1);

  // a corrupted database is created anew
  const QString corruptFileName = iDir + "/corrupt.db";
  {
    QFile file(corruptFileName);
    ok &= check("write corrupt file", file.open(QIODevice::WriteOnly) && file.write(QByteArray(8192, 'x')) == 8192);
  }
  {
    DabDatabase dabDb(corruptFileName);
    ok &= check("corrupt database recreated", dabDb.open_db() == DabDatabase::EOpenResult::Ok);
  }
  ok &= check("recreated database usable", exec_raw(corruptFileName, "SELECT COUNT(*) FROM Favorite").toInt() == 0);
  ok &= check("recreated schema version", exec_raw(corruptFileName, "PRAGMA user_version").toInt() == DabDatabase::cSchemaVersion);

  return ok;
}

bool test_migration_v1(const QString& iDbFileName)
{
  bool ok = true;

  // create a database and reduce it to schema version 1 (without the user data table)
  {
    DabDatabase dabDb(iDbFileName);
    ok &= check("open for migration", dabDb.open_db() == DabDatabase::EOpenResult::Ok);
    ServiceDB srvDb(dabDb);
    srvDb.set_favorite("5C", 0x1101, true);
    QSqlQuery query(dabDb.get_db());
    ok &= check("drop user data table", query.exec("DROP TABLE EnsembleUserData"));
    ok &= check("set version 1", query.exec("PRAGMA user_version = 1"));
  }

  {
    DabDatabase dabDb(iDbFileName);
    ok &= check("open v1 database", dabDb.open_db() == DabDatabase::EOpenResult::Ok);
    ok &= check("user data table created", dabDb.get_db().tables().contains("EnsembleUserData"));
    ok &= check("favorite kept after migration", count_rows(dabDb.get_db(), "SELECT COUNT(*) FROM Favorite") == 1);
  }
  ok &= check("migrated schema version", exec_raw(iDbFileName, "PRAGMA user_version").toInt() == DabDatabase::cSchemaVersion);

  return ok;
}
} // namespace

int main(int argc, char** argv)
{
  QApplication app(argc, argv); // QSqlQueryModel and the database driver need an application object

  const QTemporaryDir tempDir;
  if (!tempDir.isValid())
  {
    std::fprintf(stderr, "list_database_test FAILED: no temporary directory\n");
    return 1;
  }

  bool ok = true;
  ok &= test_relations_and_favorites(tempDir.filePath("relations.db"));
  ok &= test_open_handling(tempDir.path());
  ok &= test_ensemble_user_data(tempDir.filePath("userdata.db"));
  ok &= test_ensemble_entry_counts(tempDir.filePath("entrycounts.db"));
  ok &= test_migration_v1(tempDir.filePath("migration.db"));

  if (!ok)
  {
    std::fprintf(stderr, "list_database_test FAILED\n");
    return 1;
  }

  std::printf("list_database_test passed\n");
  return 0;
}
