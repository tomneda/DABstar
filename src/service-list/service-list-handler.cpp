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

void ServiceListHandler::_update_from_db()
{
  if (mpTableView->selectionModel())  // the first time no model is attached to QTableView
  {
    disconnect(mpTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ServiceListHandler::_slot_selection_changed);
  }

  mpTableView->setModel(mServiceDB.create_model());
  mpTableView->setItemDelegateForColumn(ServiceDB::CI_Fav, &mFavoriteDelegate);  // Adjust the column index
  mpTableView->resizeColumnsToContents();
  mpTableView->verticalHeader()->setDefaultSectionSize(0); // Use minimum possible size (seems work so)
//  mpTableView->setSelectionMode(QAbstractItemView::SingleSelection); // Allow only one row to be selected at a time
//  mpTableView->setSelectionBehavior(QAbstractItemView::SelectRows);  // Select entire rows, not individual items
  mpTableView->verticalHeader()->hide(); // hide first column
//  _show_selection(mpTableView->model()->mod);
  //mpTableView->show();

  connect(mpTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ServiceListHandler::_slot_selection_changed);
}

void ServiceListHandler::add_entry_to_db(const QString & iChannel, const QString & iService)
{
  if (mServiceDB.add_entry(iChannel, iService))
  {
    _update_from_db();
    set_selector_to_service(mChannelLast, mServiceLast);
  }
}

void ServiceListHandler::delete_table()
{
  mServiceDB.delete_table();
}

void ServiceListHandler::create_new_table()
{
  mServiceDB.create_table();
  _update_from_db();
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

    emit signal_selection_changed(mChannelLast, mServiceLast);
  }
}

void ServiceListHandler::_slot_header_clicked(int iIndex)
{
  mServiceDB.sort_column(static_cast<ServiceDB::EColIdx>(iIndex));
  _update_from_db();
  set_selector_to_service(mChannelLast, mServiceLast);
}

void ServiceListHandler::set_selector_to_service(const QString & iChannel, const QString & iService)
{
  qDebug() << "ServiceListHandler: Channel: " << iChannel << ", Service: " << iService;

  if (iChannel.isEmpty() || iService.isEmpty())
  {
    return;
  }

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

    if (pModel->data(modIdxService).toString() == iService &&
        pModel->data(modIdxChannel).toString() == iChannel)
    {
      foundModIdx = modIdxChannel;
      break;
    }
  }

  if (foundModIdx.isValid())
  {
    _show_selection(foundModIdx);
  }
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
