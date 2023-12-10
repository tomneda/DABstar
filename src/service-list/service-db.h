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

#ifndef DABSTAR_SERVICE_DB_H
#define DABSTAR_SERVICE_DB_H

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQueryModel>

class QTableView;

class MySqlQueryModel : public QSqlQueryModel
{
  using QSqlQueryModel::QSqlQueryModel;
public:
  [[nodiscard]] QVariant data(const QModelIndex & index, int role /*= Qt::DisplayRole*/) const override;
};

class ServiceDB
{
public:
  explicit ServiceDB(const QString & iDbFileName);
  ~ServiceDB();

  enum EColNum
  {
    CN_Service = 0,
    CN_Channel = 1,
    CN_Id      = 2
  };

  void create_table();
  void delete_table();
  void add_entry(const QString & iChannel, const QString & iServiceName);

  MySqlQueryModel * create_model();

private:
  QSqlDatabase mDB;
  QString mDbFileName;

  [[nodiscard]] const char * _error_str() const;
  void _delete_db_file();
  bool _open_db();
};

#endif
