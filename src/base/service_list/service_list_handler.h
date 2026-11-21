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
#include "service_db.h"
#include "ensemble_list/ensemble_list_filter_proxy.h"
#include <QObject>
#include <QStyledItemDelegate>

static constexpr bool cShowSIdInServiceList = false;

class DabRadio;
class QTableView;
class QSettings;

class CustomItemDelegate : public QStyledItemDelegate
{
  using QStyledItemDelegate::QStyledItemDelegate;
  Q_OBJECT
public:
  void set_current_service(const QString& iChannel, u32 iSId) { mCurChannel = iChannel; mCurSId = iSId; }

protected:
  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

private:
  const QPixmap mStarEmptyPixmap{":res/icons/starempty16.png"};   // draw a empty star icon for favorites
  const QPixmap mStarFilledPixmap{":res/icons/starfilled16.png"}; // draw a filled star icon for favorites
  QString mCurChannel;
  u32 mCurSId = 0;

signals:
  void signal_selection_changed_with_fav(const QString& oChannel, const QString& oService, u32 oSId, bool oIsFav);
};

class ServiceListHandler : public QObject
{
  Q_OBJECT
public:
  ServiceListHandler(const DabDatabase& iDabDatabase, QTableView* const ipSL);
  ~ServiceListHandler() override = default;

  using EDataMode = ServiceDB::EDataMode;
  using TSIdList = QList<u32>;
  using TChannelList = QList<QString>;

  void set_data_mode(EDataMode iDataMode);
  void set_sort_to_FId_Or_Ch(bool iShowOnlyCurrentFIdOrCh);
  void delete_table(const bool iDeleteFavorites);
  void create_new_table();
  void set_ecc(const QString& iChannel, u8 iEcc); // ECC of the ensemble, stored in the ensemble entry (not shown in the service list)
  void add_entry(const QString& iChannel, const QString& iServiceLabel, u32 iSId);
  void delete_not_existing_SId_at_channel(const QString& iChannel, const TSIdList& iSIdList);
  void delete_not_existing_channel(const TChannelList& iChList);
  void delete_channel(const QString& iChannel);
  void clear_selector();
  void set_selector(const QString& iChannel, u32 iSId);
  void set_selector_channel_only(const QString& iChannel);
  void set_favorite_state(const bool iIsFavorite);
  void reload_from_db(); // e.g. after a scan or after deleting ensemble list entries (their services are deleted, too)
  void jump_entries(i32 iSteps); // typ -1/+1, with wrap around
  void set_search_text(const QString& iText); // shows only services containing the text in the service name or channel
  TSIdList get_list_of_SId_in_channel(const QString& iChannel) const;

private:
  QTableView* const mpTableView;
  ServiceDB mServiceDB;
  EnsembleListFilterProxy mProxyModel; // used here only for the search text filter, the sorting is done by the database query
  CustomItemDelegate mCustomItemDelegate;
  QString mChannelLast;
  u32 mServiceIdLast = 0;
  bool mShowOnlyCurrentFIdOrCh = false;

  void _fill_table_view_from_db();
  void _jump_to_list_entry_and_emit_fav_status(const i32 iSkipOffset = 0, const bool iCenterContent = false);
  [[nodiscard]] i32 _find_current_service_row(const QAbstractItemModel* ipModel) const; // returns -1 if not found
  void _sort_and_update_service_list(i32 iIndex, ServiceDB::ESortDir iSortDir);

private slots:
  void _slot_selection_changed_with_fav(const QString& iChannel, const QString& iServiceLabel, u32 iSId, const bool iIsFav);
  void _slot_header_clicked(i32 iIndex);

signals:
  void signal_selection_changed(const QString& oChannel, const QString& oService, const u32 oSId);
  void signal_favorite_status(bool oIsFav);
};
