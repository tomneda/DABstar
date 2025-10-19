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

#ifndef DABSTAR_SERVICELISTHANDLER_H
#define DABSTAR_SERVICELISTHANDLER_H

#include "glob_data_types.h"
#include "service-db.h"
#include <QObject>
#include <QStyledItemDelegate>

class DabRadio;
class QTableView;
class QSettings;

class CustomItemDelegate : public QStyledItemDelegate
{
  using QStyledItemDelegate::QStyledItemDelegate;
  Q_OBJECT
public:
  void set_current_service(const QString & iChannel, u32 iSId) { mCurChannel = iChannel; mCurSId = iSId; }

protected:
  void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

private:
  const QPixmap mStarEmptyPixmap{":res/icons/starempty16.png"};   // draw a empty star icon for favorites
  const QPixmap mStarFilledPixmap{":res/icons/starfilled16.png"}; // draw a filled star icon for favorites
  QString mCurChannel;
  u32 mCurSId = 0;

signals:
  void signal_selection_changed_with_fav(const QString & oChannel, const QString & oService, u32 oSId, bool oIsFav);
};

class ServiceListHandler : public QObject
{
  Q_OBJECT
public:
  ServiceListHandler(const QString & iDbFileName, QTableView * const ipSL);
  ~ServiceListHandler() override = default;

  using EDataMode = ServiceDB::EDataMode;
  using TSIdList = QList<u32>;

  void set_data_mode(EDataMode iDataMode);
  void delete_table(const bool iDeleteFavorites);
  void create_new_table();
  void add_entry(const QString & iChannel, const QString & iServiceLabel, u32 iSId);
  void delete_not_existing_SId_at_channel(const QString & iChannel, const TSIdList & iSIdList);
  void set_selector(const QString & iChannel, u32 iSId);
  void set_selector_channel_only(const QString & iChannel);
  void set_favorite_state(const bool iIsFavorite);
  void restore_favorites();
  void jump_entries(i32 iSteps); // typ -1/+1, with wrap around
  TSIdList get_list_of_SId_in_channel(const QString & iChannel) const;

private:
  QTableView * const mpTableView;
  ServiceDB mServiceDB;
  CustomItemDelegate mCustomItemDelegate;
  QString mChannelLast;
  u32 mServiceIdLast = 0;

  void _fill_table_view_from_db();
  void _jump_to_list_entry_and_emit_fav_status(const i32 iSkipOffset = 0, const bool iCenterContent = false);

private slots:
  void _slot_selection_changed_with_fav(const QString & iChannel, const QString & iServiceLabel, u32 iSId, const bool iIsFav);
  void _slot_header_clicked(i32 iIndex);

signals:
  void signal_selection_changed(const QString & oChannel, const QString & oService, const u32 oSId);
  void signal_favorite_status(const bool oIsFav);
};


#endif // DABSTAR_SERVICELISTHANDLER_H
