//
// Created by work on 09/12/23.
//

#include "service-db.h"

#include <QTableView>
#include <QtSql/QSql>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QVariant>

ServiceDB::ServiceDB(const QString & iFilePathName)
{
  mDB = QSqlDatabase::addDatabase("QSQLITE");
  mDB.setDatabaseName(iFilePathName);

  if(!mDB.open())
  {
    QSqlError err = mDB.lastError();
    qFatal("Error: Unable to establish a database connection: %s", err.text().toStdString().c_str());
  }
}

ServiceDB::~ServiceDB()
{
  mDB.close();
}

void ServiceDB::create_table()
{
  const QString queryStr = "CREATE TABLE IF NOT EXISTS TabServList ("
                           "Id      INTEGER PRIMARY KEY,"
                           "Channel TEXT NOT NULL,"
                           "Name    TEXT NOT NULL"
                           ");";

  QSqlQuery query;

  if(!query.exec(queryStr))
  {
    qFatal("Error: Failed to create table");
  }

}

void ServiceDB::delete_table()
{
  QSqlQuery query;
  if(!query.exec("DROP TABLE IF EXISTS TabServList;"))
  {
    qFatal("Error: Failed to delete table");
  }
}


void ServiceDB::add_entry(const QString & iChannel, const QString & iServiceName)
{
  QSqlQuery query;
  query.prepare("INSERT OR IGNORE INTO TabServList (Channel, Name) VALUES (:channel, :name)");
  query.bindValue(":channel", iChannel);
  query.bindValue(":name", iServiceName);

  if(!query.exec())
  {
    QSqlError err = mDB.lastError();
    qFatal("Error: Unable insert entry: %s", err.text().toStdString().c_str());
  }
}

//void ServiceDB::bind_to_table_view(QTableView * ipQTableView)
//{
//  QSqlTableModel *model = new QSqlTableModel;
//  model->setTable("TabServList");
//  model->select();
//
//  ipQTableView->setModel(model);
//  //ipQTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
//  ipQTableView->show();
//  ipQTableView->resizeColumnsToContents();
//}

QSqlQueryModel * ServiceDB::create_model()
{
  QSqlQueryModel *model = new QSqlQueryModel;

  //QSqlTableModel *model = new QSqlTableModel;
//  model->setTable("TabServList");
//  model->select();

  QSqlQuery query;
  query.prepare("SELECT * FROM TabServList ORDER BY Name ASC");

  if (query.exec())
  {
    model->setQuery(query);
  }
  else
  {
    QSqlError err = mDB.lastError();
    qFatal("Error: Invalid SELECT query: %s", err.text().toStdString().c_str());
  }

  return model;
}
