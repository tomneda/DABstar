#include "dab_database.h"
#include "ensemble_list_db.h"
#include "service_db.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QTemporaryDir>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <cstdio>
#include <memory>

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

// creates database files like the former version 04
bool create_former_db_files(const QString& iSrvFileName, const QString& iEnsFileName)
{
  bool ok = true;
  {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "OldEns");
    db.setDatabaseName(iEnsFileName);
    ok &= db.open();
    QSqlQuery q(db);
    for (const char* const tab : { "TabDeviceEnsList", "TabFileEnsList" })
    {
      ok &= q.exec(QSL("CREATE TABLE %1 (Id INTEGER PRIMARY KEY, FIdOrCh TEXT NOT NULL, FilePath TEXT, FileName TEXT, ScanLevel INTEGER DEFAULT 0,"
                       "FileLengthMB REAL DEFAULT 0, EnsembleName TEXT, EnsembleId TEXT, ItuCode TEXT, Date TEXT, LastPlayedSId TEXT,"
                       "DscTyAppTy TEXT, AudioDataRates TEXT, NumDabDabPlus TEXT, ErrorProtection TEXT, NomFreq INTEGER DEFAULT 0,"
                       "SNR REAL DEFAULT 0, MER REAL DEFAULT 0, BaseBandOffset REAL DEFAULT 0, UNIQUE(FIdOrCh) ON CONFLICT REPLACE)").arg(tab));
    }
    ok &= q.exec("INSERT INTO TabDeviceEnsList (FIdOrCh, ScanLevel, EnsembleName, ItuCode) VALUES ('5C', 3, 'DR Deutschland', 'DE(e0/d)')");
    ok &= q.exec("INSERT INTO TabDeviceEnsList (FIdOrCh, ScanLevel) VALUES ('11D', 0)");
    ok &= q.exec("INSERT INTO TabFileEnsList (FIdOrCh, FilePath, FileName, ScanLevel) VALUES ('a1b2c3d4', '/tmp', 'rec.sdr', 2)");
    db.close();
  }
  QSqlDatabase::removeDatabase("OldEns");
  {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "OldSrv");
    db.setDatabaseName(iSrvFileName);
    ok &= db.open();
    QSqlQuery q(db);
    for (const char* const tab : { "TabDeviceList", "TabFilesList" })
    {
      ok &= q.exec(QSL("CREATE TABLE %1 (Id INTEGER PRIMARY KEY, Channel TEXT NOT NULL, SId INTEGER DEFAULT 0, Service TEXT NOT NULL,"
                       "IsFav INTEGER DEFAULT 0, UNIQUE(Channel, SId) ON CONFLICT IGNORE)").arg(tab));
    }
    ok &= q.exec("CREATE TABLE TabFavList (Id INTEGER PRIMARY KEY, Channel TEXT NOT NULL, SId INTEGER DEFAULT 0, UNIQUE(Channel, SId) ON CONFLICT IGNORE)");
    ok &= q.exec("INSERT INTO TabDeviceList (Channel, SId, Service) VALUES ('5C', 4353, 'Radio A'), ('5C', 4354, 'Radio B'), ('9Z', 4355, 'Orphan')");
    ok &= q.exec("INSERT INTO TabFilesList (Channel, SId, Service) VALUES ('a1b2c3d4', 4353, 'Radio A')");
    ok &= q.exec("INSERT INTO TabFavList (Channel, SId) VALUES ('5C', 4354)");
    db.close();
  }
  QSqlDatabase::removeDatabase("OldSrv");
  return ok;
}

bool test_relations_and_favorites(const QString& iDbFileName)
{
  bool ok = true;
  DabDatabase dabDb(iDbFileName);
  ok &= check("open new database", dabDb.open_db());
  ok &= check("new database detected", dabDb.is_newly_created());

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

  // favorites are global with ECC + SId
  srvDb.set_data_mode(ServiceDB::EDataMode::DevicePlayer);
  ok &= check("set ECC 5C", srvDb.set_ecc("5C", 0xE0));
  ok &= check("set ECC 5C again no change", !srvDb.set_ecc("5C", 0xE0));
  ok &= check("set ECC 11D", srvDb.set_ecc("11D", 0xE0));
  srvDb.set_favorite("5C", 0x1101, true);
  ok &= check("fav at 5C", get_fav_state(srvDb, "5C", 0x1101) == 1);
  ok &= check("fav also at 11D", get_fav_state(srvDb, "11D", 0x1101) == 1);
  ok &= check("other service no fav", get_fav_state(srvDb, "5C", 0x1102) == 0);
  srvDb.set_data_mode(ServiceDB::EDataMode::FilePlayer);
  ok &= check("fav in file mode with unknown ECC", get_fav_state(srvDb, "a1b2c3d4", 0x1101) == 1);
  ok &= check("set file ECC other country", srvDb.set_ecc("a1b2c3d4", 0xE1));
  ok &= check("no fav in file mode with other ECC", get_fav_state(srvDb, "a1b2c3d4", 0x1101) == 0);
  srvDb.set_data_mode(ServiceDB::EDataMode::DevicePlayer);
  srvDb.set_favorite("11D", 0x1101, false);
  ok &= check("fav removed at 5C", get_fav_state(srvDb, "5C", 0x1101) == 0);

  // favorite with unknown ECC gets the ECC later
  ensDb.set_data_mode(EnsembleListDB::EDataMode::Device);
  ok &= check("insert 12A", ensDb.insert_or_update_entry(make_entry("12A"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("add service 12A", srvDb.add_entry("12A", "Radio C", 0x1103));
  srvDb.set_favorite("12A", 0x1103, true);
  ok &= check("fav stored with ECC 0", count_rows(db, "SELECT COUNT(*) FROM Favorite WHERE Ecc = 0 AND SId = 4355") == 1);
  ok &= check("set ECC 12A", srvDb.set_ecc("12A", 0xE0));
  ok &= check("fav upgraded to ECC", count_rows(db, "SELECT COUNT(*) FROM Favorite WHERE Ecc = 224 AND SId = 4355") == 1);
  ok &= check("no fav with ECC 0 left", count_rows(db, "SELECT COUNT(*) FROM Favorite WHERE Ecc = 0") == 0);

  // re-init of an ensemble entry keeps its services
  ok &= check("re-init 5C", ensDb.insert_or_update_entry(make_entry("5C"), EnsembleListDB::EDbDataType::InsertKeyAndS0WsData));
  ok &= check("services kept after re-init", count_rows(db, "SELECT COUNT(*) FROM Service s JOIN Ensemble e ON e.Id = s.EnsembleRef WHERE e.FIdOrCh = '5C'") == 2);

  // deleting an ensemble entry deletes its services (cascade), but keeps the favorites
  ok &= check("delete 5C", ensDb.delete_entry("5C"));
  ok &= check("services of 5C deleted", count_rows(db, "SELECT COUNT(*) FROM Service") == 3);
  ensDb.delete_table();
  ok &= check("device services deleted", count_rows(db, "SELECT COUNT(*) FROM Service") == 1);
  ok &= check("file entry kept", count_rows(db, "SELECT COUNT(*) FROM Ensemble WHERE Mode = 1") == 1);
  ok &= check("favorites kept", count_rows(db, "SELECT COUNT(*) FROM Favorite") == 1);

  return ok;
}

bool test_import(const QString& iDir)
{
  bool ok = true;
  const QString srvFile = iDir + "/servicelist04.db";
  const QString ensFile = iDir + "/ensemblelist04.db";
  ok &= check("create former files", create_former_db_files(srvFile, ensFile));

  DabDatabase dabDb(iDir + "/dabstar05.db");
  ok &= check("open import database", dabDb.open_db());
  ok &= check("import", dabDb.import_former_db_files(srvFile, ensFile));
  const QSqlDatabase& db = dabDb.get_db();

  ok &= check("ensembles imported", count_rows(db, "SELECT COUNT(*) FROM Ensemble") == 3);
  ok &= check("ECC from ITU code", count_rows(db, "SELECT Ecc FROM Ensemble WHERE FIdOrCh = '5C'") == 0xE0);
  ok &= check("file entry imported", count_rows(db, "SELECT COUNT(*) FROM Ensemble WHERE Mode = 1 AND FileName = 'rec.sdr'") == 1);
  ok &= check("services imported without orphan", count_rows(db, "SELECT COUNT(*) FROM Service") == 3);
  ok &= check("favorite imported with ECC", count_rows(db, "SELECT COUNT(*) FROM Favorite WHERE Ecc = 224 AND SId = 4354") == 1);

  ServiceDB srvDb(dabDb);
  srvDb.set_data_mode(ServiceDB::EDataMode::DevicePlayer);
  ok &= check("imported favorite shown", get_fav_state(srvDb, "5C", 4354) == 1);
  ok &= check("ECC parse invalid", DabDatabase::get_ecc_from_itu_code("") == 0);

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
  ok &= test_import(tempDir.path());

  if (!ok)
  {
    std::fprintf(stderr, "list_database_test FAILED\n");
    return 1;
  }

  std::printf("list_database_test passed\n");
  return 0;
}
