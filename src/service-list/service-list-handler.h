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
#include <QStandardItemModel>
#include <QItemSelection>
#include <QStyledItemDelegate>
#include "service-db.h"

class RadioInterface;
class QTableView;

class MyItemDelegate : public QStyledItemDelegate
{
public:
  explicit MyItemDelegate(QObject * parent = nullptr);
  ~MyItemDelegate() override = default;

  void paint(QPainter * iPainter, const QStyleOptionViewItem & iOption, const QModelIndex & iModelIdx) const override;

  inline void set_current_channel(const QString & iChannel) { mCurChannel = iChannel; }

private:
  const QPixmap mStarEmptyPixmap{":res/icons/starempty16.png"}; // draw a star icon for favorites
  const QPixmap mStarFilledPixmap{":res/icons/starfilled16.png"}; // draw a star icon for favorites
  QString mCurChannel;
};

class ServiceListHandler : public QObject
{
  Q_OBJECT
public:
  explicit ServiceListHandler(const QString & iDbFileName, QTableView * const ipSL);
  ~ServiceListHandler() override = default;

  void delete_table(const bool iDeleteFavorites);
  void create_new_table();
  void add_entry_to_db(const QString & iChannel, const QString & iService);
  void set_selector_to_service(const QString & iChannel, const QString & iService);
  void set_favorite(const bool iIsFavorite);
  void restore_favorites();

private:
  QTableView * const mpTableView;
  ServiceDB mServiceDB;
  MyItemDelegate mMyItemDelegate;
  QString mChannelLast;
  QString mServiceLast;

  void _update_from_db();
  void _show_selection(const QModelIndex & iModelIdx) const;
  void _set_selector_to_service();

public slots:

private slots:
  void _slot_selection_changed(const QItemSelection &iSelected, const QItemSelection &iDeselected);
  void _slot_header_clicked(int iIndex);

signals:
  void signal_selection_changed(const QString & oChannel, const QString & oService);
  void signal_favorite_changed(const bool oIsFav);
};


#endif // DABSTAR_SERVICELISTHANDLER_H
