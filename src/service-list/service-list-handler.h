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

#include <QObject>
#include <QStyledItemDelegate>
#include "service-db.h"

class RadioInterface;
class QTableView;
class QSettings;

class CustomItemDelegate : public QStyledItemDelegate
{
  using QStyledItemDelegate::QStyledItemDelegate;
  Q_OBJECT
public:
  inline void set_current_service(const QString & iChannel, const QString & iService) { mCurChannel = iChannel; mCurService = iService; }

protected:
  void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

private:
  const QPixmap mStarEmptyPixmap{":res/icons/starempty16.png"}; // draw a star icon for favorites
  const QPixmap mStarFilledPixmap{":res/icons/starfilled16.png"}; // draw a star icon for favorites
  QString mCurChannel;
  QString mCurService;

signals:
  void signal_selection_changed_with_fav(const QString & oChannel, const QString & oService, const bool oIsFav);
};

class ServiceListHandler : public QObject
{
  Q_OBJECT
public:
  ServiceListHandler(QSettings * const iopSettings, const QString & iDbFileName, QTableView * const ipSL);
  ~ServiceListHandler() override = default;

  void delete_table(const bool iDeleteFavorites);
  void create_new_table();
  void add_entry(const QString & iChannel, const QString & iService);
  void set_selector(const QString & iChannel, const QString & iService);
  void set_favorite_state(const bool iIsFavorite);
  void restore_favorites();
  void jump_entries(int32_t iSteps); // typ -1/+1, with wrap around

private:
  QTableView * const mpTableView;
  QSettings * const mpSettings;
  ServiceDB mServiceDB;
  CustomItemDelegate mCustomItemDelegate;
  QString mChannelLast;
  QString mServiceLast;

  void _fill_table_view_from_db();
  void _jump_to_list_entry_and_emit_fav_status(const int32_t iSkipOffset = 0);

private slots:
  void _slot_selection_changed_with_fav(const QString & iChannel, const QString & iService, const bool iIsFav);
  void _slot_header_clicked(int iIndex);

signals:
  void signal_selection_changed(const QString & oChannel, const QString & oService);
  void signal_favorite_status(const bool oIsFav);
};


#endif // DABSTAR_SERVICELISTHANDLER_H
