//
// Created by work on 09/12/23.
//

#ifndef DABSTAR_SERVICE_DB_H
#define DABSTAR_SERVICE_DB_H

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQueryModel>

class QTableView;

class ServiceDB
{
public:
  explicit ServiceDB(const QString & iFilePathName);
  ~ServiceDB();
  void create_table();
  void delete_table();
  void add_entry(const QString & iChannel, const QString & iServiceName);
  //void bind_to_table_view(QTableView * ipQTableView);
  QSqlQueryModel * create_model();

private:
  QSqlDatabase mDB;
};


#endif
