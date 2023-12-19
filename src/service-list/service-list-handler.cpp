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

MyItemDelegate::MyItemDelegate(QObject * parent /*= nullptr*/) :
  QStyledItemDelegate(parent)
{}

void MyItemDelegate::paint(QPainter * iPainter, const QStyleOptionViewItem & iOption, const QModelIndex & iModelIdx) const
{
  const QAbstractItemModel * pModel = iModelIdx.model();
  const QModelIndex modIdxChannel = pModel->index(iModelIdx.row(), ServiceDB::CI_Channel);

  if (pModel->data(modIdxChannel).toString() == mCurChannel)
  {
    iPainter->fillRect(iOption.rect, QColor(40, 0, 90));
  }
  else
  {
    iPainter->fillRect(iOption.rect, QColor(60, 0, 60));
  }

  if (iModelIdx.column() == ServiceDB::CI_Fav)
  {
    const QPixmap & curPM = (iModelIdx.data(Qt::DisplayRole).toBool() ? mStarFilledPixmap : mStarEmptyPixmap);

    // Calculate the position to center the pixmap within the item rectangle
    const int x = iOption.rect.x() + (iOption.rect.width() - 16) / 2; // 16 = width of icon
    const int y = iOption.rect.y() + (iOption.rect.height() - 16) / 2; // 16 = height of icon

    iPainter->drawPixmap(x, y, curPM);
    return;
  }

  QStyledItemDelegate::paint(iPainter, iOption, iModelIdx);
}


ServiceListHandler::ServiceListHandler(const QString & iDbFileName, QTableView * const ipSL) :
  mpTableView(ipSL),
  mServiceDB(iDbFileName)
{
  mServiceDB.open_db(); // program will exit if table could not be opened
  //mServiceDB.delete_table();
  mServiceDB.create_table();
  _update_from_db();
  connect(mpTableView->horizontalHeader(), &QHeaderView::sectionClicked, this, &ServiceListHandler::_slot_header_clicked);
}

void ServiceListHandler::_update_from_db()
{
  if (mpTableView->selectionModel())  // the first time no model is attached to QTableView
  {
    disconnect(mpTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ServiceListHandler::_slot_selection_changed);
  }

  mpTableView->setModel(mServiceDB.create_model());
  //mpTableView->setItemDelegateForColumn(ServiceDB::CI_Fav, &mFavoriteDelegate);  // Adjust the column index
  mpTableView->setItemDelegate(&mMyItemDelegate);  // Adjust the column index
  mpTableView->resizeColumnsToContents();
  mpTableView->verticalHeader()->setDefaultSectionSize(0); // Use minimum possible size (seems work so)
  //mpTableView->verticalHeader()->hide(); // hide first column

  connect(mpTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ServiceListHandler::_slot_selection_changed);
}

void ServiceListHandler::add_entry_to_db(const QString & iChannel, const QString & iService)
{
  if (mServiceDB.add_entry(iChannel, iService))
  {
    _update_from_db();
    _set_selector_to_service();
  }
}

void ServiceListHandler::delete_table(const bool iDeleteFavorites)
{
  mServiceDB.delete_table(iDeleteFavorites);
}

void ServiceListHandler::create_new_table()
{
  mServiceDB.create_table();
  _update_from_db();
}

void ServiceListHandler::set_selector_to_service(const QString & iChannel, const QString & iService)
{
  if (iChannel.isEmpty() || iService.isEmpty())
  {
    return;
  }

  if (mChannelLast == iChannel && mServiceLast == iService)
  {
    return;
  }

  mChannelLast = iChannel;
  mServiceLast = iService;
  mMyItemDelegate.set_current_channel(mChannelLast);

  _set_selector_to_service();
}

void ServiceListHandler::_set_selector_to_service()
{
  if (mChannelLast.isEmpty() || mServiceLast.isEmpty())
  {
    return;
  }

  qDebug() << "ServiceListHandler: Channel: " << mChannelLast << ", Service: " << mServiceLast;

  QAbstractItemModel * pModel = mpTableView->model();

  if (pModel == nullptr)
  {
    _update_from_db();
    pModel = mpTableView->model();
    assert(pModel != nullptr);
  }

  QModelIndex foundModIdx;

  for (int rowIdx = 0; rowIdx < pModel->rowCount(); ++rowIdx)
  {
    QModelIndex modIdxChannel = pModel->index(rowIdx, ServiceDB::CI_Channel);
    QModelIndex modIdxService = pModel->index(rowIdx, ServiceDB::CI_Service);

    if (pModel->data(modIdxService).toString() == mServiceLast &&
        pModel->data(modIdxChannel).toString() == mChannelLast)
    {
      foundModIdx = modIdxChannel;
      const bool isFav = pModel->data(pModel->index(rowIdx, ServiceDB::CI_Fav)).toBool();
      emit signal_favorite_changed(isFav);
      break;
    }
  }

  if (foundModIdx.isValid())
  {
    _show_selection(foundModIdx);
  }
}

void ServiceListHandler::_slot_selection_changed(const QItemSelection & iSelected, const QItemSelection & /*iDeselected*/)
{
  const QModelIndexList indexes = iSelected.indexes();

  if (indexes.size() == 1)
  {
    const int rowIdx = indexes.first().row();

    QAbstractItemModel *pModel = mpTableView->model();
    mServiceLast = pModel->index(rowIdx, ServiceDB::CI_Service).data().toString();
    mChannelLast = pModel->index(rowIdx, ServiceDB::CI_Channel).data().toString();
    const bool isFav = pModel->index(rowIdx, ServiceDB::CI_Fav).data().toBool();
    mMyItemDelegate.set_current_channel(mChannelLast);

    emit signal_selection_changed(mChannelLast, mServiceLast);  // triggers an service change in radio.h
    emit signal_favorite_changed(isFav); // this emit speeds up the FavButton setting, the other one is more important
  }
}

void ServiceListHandler::_slot_header_clicked(int iIndex)
{
  mServiceDB.sort_column(static_cast<ServiceDB::EColIdx>(iIndex));
  _update_from_db();
  _set_selector_to_service();
}

void ServiceListHandler::_show_selection(const QModelIndex & iModelIdx) const
{
  QAbstractItemModel * pModel = mpTableView->model();
  QItemSelectionModel * const pSm = mpTableView->selectionModel();

  if (pModel == nullptr || pSm == nullptr)
  {
    return;
  }

  pSm->blockSignals(true); // avoid a recursive call to _slot_selection_changed()
  QItemSelection rowSelection(pModel->index(iModelIdx.row(), ServiceDB::CI_Service), pModel->index(iModelIdx.row(), ServiceDB::CI_Channel));
  pSm->select(rowSelection, QItemSelectionModel::ClearAndSelect);
  mpTableView->scrollTo(iModelIdx, QAbstractItemView::EnsureVisible);
  mpTableView->update();
  pSm->blockSignals(false);
}

void ServiceListHandler::set_favorite(const bool iIsFavorite)
{
  mServiceDB.set_favorite(mChannelLast, mServiceLast, iIsFavorite);
  _update_from_db();
  _set_selector_to_service();
}

void ServiceListHandler::restore_favorites()
{
  mServiceDB.retrieve_favorites_from_backup_table();
  _update_from_db();
  _set_selector_to_service();
}

