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

ServiceDB::ServiceDB(const QString & iDbFileName) :
  mDbFileName(iDbFileName)
{
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

void ServiceDB::_delete_db_file()
{
  if (std::filesystem::exists(mDbFileName.toStdString()))
  {
    std::filesystem::remove(mDbFileName.toStdString());
  }
}

void ServiceDB::create_table()
{
  const QString queryStr = "CREATE TABLE IF NOT EXISTS TabServList ("
                           "Id      INTEGER PRIMARY KEY,"
                           "Channel TEXT NOT NULL,"
                           "Name    TEXT NOT NULL,"
                           "UNIQUE(Channel, Name) ON CONFLICT IGNORE"
                           ");";

  QSqlQuery query;

  if (!query.exec(queryStr))
  {
    qFatal("Error: Failed to create table: %s", _error_str());
  }

}

void ServiceDB::delete_table()
{
  QSqlQuery query;
  if (!query.exec("DROP TABLE IF EXISTS TabServList;"))
  {
    qFatal("Error: Failed to delete table: %s", _error_str());
  }
}


void ServiceDB::add_entry(const QString & iChannel, const QString & iServiceName)
{
  QSqlQuery query;
  query.prepare("INSERT OR IGNORE INTO TabServList (Channel, Name) VALUES (:channel, :name)");
  query.bindValue(":channel", iChannel);
  query.bindValue(":name", iServiceName);

  if (!query.exec())
  {
    qFatal("Error: Unable insert entry: %s", _error_str());
  }
}

QSqlQueryModel * ServiceDB::create_model()
{
  QSqlQueryModel * model = new QSqlQueryModel;

  QSqlQuery query;
  //query.prepare("SELECT * FROM TabServList ORDER BY Name ASC");
  query.prepare("SELECT Name, Channel AS Ch, Id FROM TabServList ORDER BY Name ASC");

  if (query.exec())
  {
    model->setQuery(query);
  }
  else
  {
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
  mDB = QSqlDatabase::addDatabase("QSQLITE");
  mDB.setDatabaseName(mDbFileName);

  return mDB.open();
}

