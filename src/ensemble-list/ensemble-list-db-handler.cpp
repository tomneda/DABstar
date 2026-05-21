/*
 * Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
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

#include "setting-helper.h"
#include "ensemble-list-db-handler.h"
#include <QEvent>
#include <QTableView>
#include <QPainter>
#include <QHeaderView>
#include <QLoggingCategory>

// Q_LOGGING_CATEGORY(sLogFilePlayerDbHandler, "FilePlayerDbHandler", QtDebugMsg)
Q_LOGGING_CATEGORY(sLogFilePlayerDbHandler, "FilePlayerDbHandler", QtWarningMsg)

void CustomItemDelegate2::paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const
{
  const QAbstractItemModel * pModel = index.model();
  const QModelIndex modIdxFId = pModel->index(index.row(), mCurFIdOrChColIdx);
  const QModelIndex modIdxSL = pModel->index(index.row(), mCurScanLevelIdx);
  const EScanLevel scanLevel = static_cast<EScanLevel>(pModel->data(modIdxSL).toInt());
  const bool isLiveFile = (pModel->data(modIdxFId).toString() == mCurFIdOrCh);

  QColor bgColor;

  if (isLiveFile)
  {
    bgColor = 0x864e1a;
  }
  else
  {
    switch (scanLevel)
    {
    case EScanLevel::SL0_Init: bgColor = cBgColorNotScanned; break;
    case EScanLevel::SL1_ScanFailed: bgColor = cBgColorFailed; break;
    default: bgColor = cBgColorUnselected;
    }
  }

  painter->fillRect(option.rect, bgColor); // background color

  QStyledItemDelegate::paint(painter, option, index);
}

bool CustomItemDelegate2::editorEvent(QEvent * event, QAbstractItemModel * model, const QStyleOptionViewItem & option, const QModelIndex & index)
{
  if (event->type() == QEvent::MouseButtonPress)
  {
    const QString fId = model->data(model->index(index.row(), mCurFIdOrChColIdx)).toString();

    emit signal_selection_changed(fId);

    return true; // Indicate that the event is handled
  }

  return QStyledItemDelegate::editorEvent(event, model, option, index);
}


EnsembleListDbHandler::EnsembleListDbHandler(const QString & iDbFileName, QTableView * const ipSL) :
  mpTableView(ipSL),
  mEnsembleListDb(iDbFileName)
{
  mpTableView->setItemDelegate(&mCustomItemDelegate);
  mpTableView->setSelectionMode(QAbstractItemView::NoSelection);
  //mpTableView->verticalHeader()->hide(); // hide first column
  mpTableView->verticalHeader()->setDefaultSectionSize(0); // Use minimum possible size (seems work so)
  mpTableView->setFocusPolicy(Qt::NoFocus);

  mProxyModel.setSortCaseSensitivity(Qt::CaseInsensitive);
  mpTableView->setModel(&mProxyModel);

  mEnsembleListDb.open_db(); // program will exit if table could not be opened

  mEnsembleListDb.set_data_mode(EnsembleListDB::EDataMode::Device);
  //mServiceDB.delete_table();
  mEnsembleListDb.create_table();

  mEnsembleListDb.set_data_mode(EnsembleListDB::EDataMode::Files);
  //mServiceDB.delete_table();
  mEnsembleListDb.create_table();

  mpTableView->horizontalHeader()->setSortIndicatorShown(true);

  _fill_table_view_from_db();

  // Restore sort order from previous session
  const i32 savedSortCol = Settings::EnsembleList::varSortCol.read().toInt();
  const bool savedSortDesc = Settings::EnsembleList::varSortDesc.read().toBool();
  const auto savedSortOrder = savedSortDesc ? Qt::DescendingOrder : Qt::AscendingOrder;
  mProxyModel.sort(savedSortCol, savedSortOrder);
  mpTableView->horizontalHeader()->setSortIndicator(savedSortCol, savedSortOrder);

  connect(mpTableView->horizontalHeader(), &QHeaderView::sectionClicked, this, &EnsembleListDbHandler::_slot_header_clicked);
  connect(&mCustomItemDelegate, &CustomItemDelegate2::signal_selection_changed, this, &EnsembleListDbHandler::_slot_selection_changed);
}

void EnsembleListDbHandler::insert_or_update_entry(const EnsembleListDB::SDbEntryData & iEntryData, const EnsembleListDB::EDbDataType iDataType)
{
  if (mEnsembleListDb.insert_or_update_entry(iEntryData, iDataType)) // true if new entry was added
  {
    qCDebug(sLogFilePlayerDbHandler) << "Added/Updated in database: file" << iEntryData.S0Ws.filePath << "with FId/channel" << iEntryData.key.fIdOrCh << "DataType" << (int)iDataType;
    _fill_table_view_from_db();
    _jump_to_list_entry();
  }
}

bool EnsembleListDbHandler::delete_entry(const QString & iFIdOrCh)
{
  const bool result = mEnsembleListDb.delete_entry(iFIdOrCh);
  _fill_table_view_from_db();
  return result;
}

bool EnsembleListDbHandler::delete_invalid_entries()
{
  const bool result = mEnsembleListDb.delete_invalid_entries();
  _fill_table_view_from_db();
  return result;
}

EnsembleListDB::SDbEntryData EnsembleListDbHandler::get_entry(const QString & iFIdOrCh) const
{
  return mEnsembleListDb.get_entry(iFIdOrCh);
}

i32 EnsembleListDbHandler::get_nr_unscanned_entries() const
{
  return mEnsembleListDb.get_nr_unscanned_entries();
}

bool EnsembleListDbHandler::is_entry_existing(const QString & iFIdOrCh) const
{
  return mEnsembleListDb.is_entry_existing(iFIdOrCh);
}

i32 EnsembleListDbHandler::_get_fid_or_ch_col_idx() const
{
  return (mEnsembleListDb.get_data_mode() == EnsembleListDB::EDataMode::Device ? EnsembleListDB::CI_CH : EnsembleListDB::CI_FId);
}

i32 EnsembleListDbHandler::_get_scan_level_col_idx() const
{
  return (mEnsembleListDb.get_data_mode() == EnsembleListDB::EDataMode::Device ? EnsembleListDB::CI_CSL : EnsembleListDB::CI_FSL);
}

void EnsembleListDbHandler::_hide_columns() const
{
  mpTableView->showColumn(EnsembleListDB::CI_CSL);
  mpTableView->showColumn(EnsembleListDB::CI_FSL);
  mpTableView->hideColumn(_get_scan_level_col_idx()); // hide service level column
}

void EnsembleListDbHandler::delete_table()
{
  // avoid coloring list while scan
  mFIdOrChLast.clear();
  mCustomItemDelegate.set_current_fid_or_ch(mFIdOrChLast, _get_fid_or_ch_col_idx());

  mEnsembleListDb.delete_table();
}

void EnsembleListDbHandler::create_new_table()
{
  mEnsembleListDb.create_table();
  _fill_table_view_from_db();
}

bool EnsembleListDbHandler::is_table_existing(const EDataMode iDataMode) const
{
  return mEnsembleListDb.is_table_existing(iDataMode);
}

i32 EnsembleListDbHandler::set_selector(const QString & iFIdOrCh)
{
  mFIdOrChLast = iFIdOrCh;
  mCustomItemDelegate.set_current_fid_or_ch(mFIdOrChLast, _get_fid_or_ch_col_idx());

  return _jump_to_list_entry(); // return row index of mFIdOrChLast
}

QString EnsembleListDbHandler::get_full_path(const QString & iFId) const
{
  return mEnsembleListDb.get_full_path(iFId);
}

void EnsembleListDbHandler::_fill_table_view_from_db()
{
  QAbstractItemModel * const oldModel = mProxyModel.sourceModel();

  mProxyModel.setSourceModel(mEnsembleListDb.create_model());

  if (oldModel != nullptr)
  {
    oldModel->deleteLater();
  }

  _hide_columns(); // hide service level column

  QHeaderView * const header = mpTableView->horizontalHeader();
  header->setSectionResizeMode(QHeaderView::ResizeToContents);
  header->setStretchLastSection(true);
}

void EnsembleListDbHandler::jump_entries(i32 iSteps)
{
  _jump_to_list_entry(iSteps, true);
}

void EnsembleListDbHandler::set_data_filter(const EnsembleListDB::SDataFilter & iDataFilter)
{
  mEnsembleListDb.set_data_filter(iDataFilter);
  _fill_table_view_from_db();
}

i32 EnsembleListDbHandler::_jump_to_list_entry(const i32 iSkipOffset /*= 0*/, const bool iCenterContent /*= false*/)
{
  if (mFIdOrChLast.isEmpty())
  {
    return -1;
  }

  i32 rowIdxFound = -1;

  for (i32 rowIdx = 0; rowIdx < mProxyModel.rowCount(); ++rowIdx)
  {
    QModelIndex modIdxFId = mProxyModel.index(rowIdx, _get_fid_or_ch_col_idx());

    if (mProxyModel.data(modIdxFId).toString() == mFIdOrChLast)
    {
      rowIdxFound = rowIdx;
      break;
    }
  }

  if (rowIdxFound >= 0)
  {
    const i32 maxRows = mProxyModel.rowCount();
    rowIdxFound += iSkipOffset;
    while (rowIdxFound < 0) rowIdxFound += maxRows;
    while (rowIdxFound >= maxRows) rowIdxFound -= maxRows;

    if (iSkipOffset != 0) // only trigger new status to the radio frontend if it was changed by a skip
    {
      mFIdOrChLast = mProxyModel.data(mProxyModel.index(rowIdxFound, _get_fid_or_ch_col_idx())).toString();
      mCustomItemDelegate.set_current_fid_or_ch(mFIdOrChLast, _get_fid_or_ch_col_idx());

      emit signal_selection_changed(mFIdOrChLast);
    }

    mpTableView->scrollTo(mProxyModel.index(rowIdxFound, _get_fid_or_ch_col_idx()),
                          (iCenterContent ? QAbstractItemView::PositionAtCenter : QAbstractItemView::EnsureVisible));
    mpTableView->update();
  }

  return rowIdxFound;
}

void EnsembleListDbHandler::_slot_selection_changed(const QString & iFIdOrCh)
{
  mFIdOrChLast = iFIdOrCh;
  mCustomItemDelegate.set_current_fid_or_ch(mFIdOrChLast, _get_fid_or_ch_col_idx());
  mpTableView->update();

  emit signal_selection_changed(iFIdOrCh);
}

void EnsembleListDbHandler::_slot_header_clicked(i32 iIndex)
{
  const auto oldSortCol = mProxyModel.sortColumn();
  const auto oldSortOrder = mProxyModel.sortOrder();

  auto newSortOrder = Qt::AscendingOrder;
  if (oldSortCol == iIndex)
  {
    newSortOrder = (oldSortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder;
  }

  mProxyModel.sort(iIndex, newSortOrder);
  mpTableView->horizontalHeader()->setSortIndicator(iIndex, newSortOrder);

  Settings::EnsembleList::varSortCol.write(iIndex);
  Settings::EnsembleList::varSortDesc.write(newSortOrder == Qt::DescendingOrder ? 1 : 0);

  // No need to call _fill_table_view_from_db() as sorting is handled by proxy model
  _jump_to_list_entry();
}

void EnsembleListDbHandler::set_data_mode(EDataMode iDataMode)
{
  mCustomItemDelegate.set_current_scan_level(iDataMode == EDataMode::Device ? EnsembleListDB::CI_CSL : EnsembleListDB::CI_FSL);
  mEnsembleListDb.set_data_mode(iDataMode);
  _fill_table_view_from_db();
}

void EnsembleListDbHandler::set_sorting_to_FId_or_Ch(const bool iRestoreSorting)
{
  i32 sortColIdx = -1;
  auto sortOrder = Qt::AscendingOrder;

  if (iRestoreSorting)
  {
    if (mOldSortCol >= 0)
    {
      sortColIdx = mOldSortCol;
      sortOrder = mOldSortOrder;
    }
    else
    {
      return; // Nothing to restore
    }
  }
  else
  {
    mOldSortCol = mProxyModel.sortColumn();
    mOldSortOrder = mProxyModel.sortOrder();

    sortColIdx = _get_fid_or_ch_col_idx();
    sortOrder = Qt::AscendingOrder;
  }

  mProxyModel.sort(sortColIdx, sortOrder);
  mpTableView->horizontalHeader()->setSortIndicator(sortColIdx, sortOrder);

  Settings::EnsembleList::varSortCol.write(sortColIdx);
  Settings::EnsembleList::varSortDesc.write(sortOrder == Qt::DescendingOrder ? 1 : 0);

  _jump_to_list_entry();
}
