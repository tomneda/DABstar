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
#pragma once

#include "glob_data_types.h"
#include <QtSql/QSqlDatabase>

class QTableView;
class QAbstractItemModel;

class ServiceDB : public QObject
{
  Q_OBJECT
public:
  explicit ServiceDB(const QString iDbFileName); // copy is intended
  ~ServiceDB();

  enum class EDataMode
  {
    DevicePlayer,
    FilePlayer
    // Temporary
  };

  enum EColIdx
  {
    CI_Fav,     // = 0
    CI_Service, // = 1
    CI_Channel, // = 2
    CI_SId,     // = 3
    CI_MAX 
  };

  enum class ESortDir
  {
    ChangeSortDirection,
    KeepSortDirection,  // keep sorting-direction only if same column is chosen again, else it is forced to ascend sorting
    ForceSortAsc,
    ForceSortDesc
  };

  void set_data_mode(EDataMode iDataMode);
  void open_db();
  void create_table();
  void delete_table(const bool iDeleteFavorites);
  bool add_entry(const QString & iChannel, const QString & iServiceLabel, u32 iSId);
  bool delete_entry(const QString & iChannel, u32 iSId);
  bool delete_channel(const QString & iChannel);
  void sort_column(EColIdx iColIdx, ESortDir iSortDir);

  void set_favorite(const QString & iChannel, u32 iSId, bool iIsFavorite) const;
  void retrieve_favorites_from_backup_table();
  [[nodiscard]] QList<QString> get_list_of_channels() const;
  [[nodiscard]] bool is_sort_desc() const;
  QAbstractItemModel * create_model(const QString & iFilterToFIdOrCh);

private:
  QSqlDatabase mDB;
  QString mDbFileName;
  EColIdx mSortColIdx = CI_Service;
  bool mSortDesc = false;
  EDataMode mDataMode = EDataMode::DevicePlayer;

  [[nodiscard]] QString _error_str() const;
  void _delete_db_file();
  bool _open_db();
  void _exec_simple_query(const QString & iQuery);
  bool _check_if_entry_exists(const QString & iTableName, const QString & iChannel, u32 iSId);
  void _set_favorite(const QString & iChannel, u32 iSId, bool iIsFavorite, bool iStoreInFavTable) const;
  [[nodiscard]] const QString & _cur_tab_name() const;
};
