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

#include <QTableView>
#include <QPainter>
#include "radio.h"
#include "service-list-handler.h"

FavoriteDelegate::FavoriteDelegate(QObject * parent /*= nullptr*/) :
  QStyledItemDelegate(parent)
{}

void FavoriteDelegate::paint(QPainter * iPainter, const QStyleOptionViewItem & iOption, const QModelIndex & iModelIdx) const
{
  assert(iModelIdx.column() == ServiceDB::CI_Fav);
  const QPixmap & curPM = (iModelIdx.data(Qt::DisplayRole).toBool() ? mStarFilledPixmap : mStarEmptyPixmap);
  QRect targetRect = iOption.rect;
  //targetRect.adjust(2, 2, -2, -2);
  iPainter->drawPixmap(targetRect, curPM, curPM.rect());

  //QStyledItemDelegate::paint(iPainter, iOption, iModelIdx);
}


ServiceListHandler::ServiceListHandler(const QString & iDbFileName, QTableView * const ipSL) :
  mpTableView(ipSL),
  mServiceDB(iDbFileName)
{
  //mServiceDB.delete_table();
  mServiceDB.create_table();

  connect(mpTableView->horizontalHeader(), &QHeaderView::sectionClicked, this, &ServiceListHandler::_slot_header_clicked);
}

void ServiceListHandler::update()
{
  // hope this makes no problem if first time no model is attached to QTableView
  disconnect(mpTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ServiceListHandler::_slot_selection_changed);

  mpTableView->setModel(mServiceDB.create_model());
  mpTableView->setItemDelegateForColumn(ServiceDB::CI_Fav, &mFavoriteDelegate);  // Adjust the column index
  mpTableView->resizeColumnsToContents();
  mpTableView->verticalHeader()->setDefaultSectionSize(0); // Use minimum possible size (seems work so)
  mpTableView->setSelectionMode(QAbstractItemView::SingleSelection); // Allow only one row to be selected at a time
  mpTableView->setSelectionBehavior(QAbstractItemView::SelectRows);  // Select entire rows, not individual items
  //mpTableView->verticalHeader()->hide(); // hide first column
  mpTableView->show();

  connect(mpTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ServiceListHandler::_slot_selection_changed);
}

void ServiceListHandler::_slot_selection_changed(const QItemSelection & selected, const QItemSelection & deselected)
{
  const QModelIndexList indexes = selected.indexes();

  if(!indexes.empty())
  {
    const int row = indexes.first().row();

    const QString curService = mpTableView->model()->index(row, ServiceDB::CI_Service).data().toString();
    const QString curChannel = mpTableView->model()->index(row, ServiceDB::CI_Channel).data().toString();

    if (curChannel != mLastChannel)
    {
      emit signal_channel_changed(curChannel, curService);
    }
    else
    {
      emit signal_service_changed(curService);
    }

    mLastChannel = curChannel;
  }
}

void ServiceListHandler::_slot_header_clicked(int iIndex)
{
  mServiceDB.sort_column(static_cast<ServiceDB::EColIdx>(iIndex));
  update();
}
