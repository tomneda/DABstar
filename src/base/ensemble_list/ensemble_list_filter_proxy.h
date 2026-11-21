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
#pragma once

#include "glob_data_types.h"
#include <QSortFilterProxyModel>

// Proxy model which additionally filters rows by a free search text (case-insensitive) over all columns except the ignored ones.
// A (possibly long) note column is shown shortened, the search and the sorting consider the full text.
class EnsembleListFilterProxy final : public QSortFilterProxyModel
{
public:
  using QSortFilterProxyModel::QSortFilterProxyModel;

  static constexpr i32 cNoteMaxDisplayChars = 20;

  void set_search_text(const QString& iText)
  {
    const QString text = iText.trimmed();
    if (text == mSearchText)
    {
      return;
    }
    _begin_rows_filter_change();
    mSearchText = text;
    _end_rows_filter_change();
  }

  void set_ignored_columns(const QList<i32>& iCols)
  {
    if (iCols == mIgnoredCols)
    {
      return;
    }
    _begin_rows_filter_change();
    mIgnoredCols = iCols;
    _end_rows_filter_change();
  }

  void set_note_column(const i32 iCol)
  {
    mNoteCol = iCol;
  }

  [[nodiscard]] const QString& get_search_text() const { return mSearchText; }

  [[nodiscard]] QVariant data(const QModelIndex& iIndex, const int iRole = Qt::DisplayRole) const override
  {
    if (iIndex.column() == mNoteCol && (iRole == Qt::DisplayRole || iRole == Qt::ToolTipRole))
    {
      const QString note = QSortFilterProxyModel::data(iIndex, Qt::DisplayRole).toString();

      if (iRole == Qt::ToolTipRole)
      {
        return (note.isEmpty() ? QVariant() : QVariant(note));
      }

      if (note.size() > cNoteMaxDisplayChars)
      {
        return QString(note.left(cNoteMaxDisplayChars) + "...");
      }

      return note;
    }

    return QSortFilterProxyModel::data(iIndex, iRole);
  }

  // The sort direction is shown as an arrow behind the header label of the sorted column (like in the service list).
  // The header sort indicator of Qt is not used as it reserves its width in each column, which makes all columns much wider.
  [[nodiscard]] QVariant headerData(const int iSection, const Qt::Orientation iOrientation, const int iRole = Qt::DisplayRole) const override
  {
    QVariant value = QSortFilterProxyModel::headerData(iSection, iOrientation, iRole);

    if (iOrientation == Qt::Horizontal && iRole == Qt::DisplayRole && iSection == sortColumn())
    {
      return QString(value.toString() + (sortOrder() == Qt::DescendingOrder ? "↑" : "↓"));
    }

    return value;
  }

  void sort(const int iColumn, const Qt::SortOrder iOrder = Qt::AscendingOrder) override
  {
    QSortFilterProxyModel::sort(iColumn, iOrder);

    if (const i32 colCnt = columnCount();
        colCnt > 0)
    {
      emit headerDataChanged(Qt::Horizontal, 0, colCnt - 1); // the sort arrow has moved
    }
  }

protected:
  bool filterAcceptsRow(const int iSrcRow, const QModelIndex& iSrcParent) const override
  {
    if (mSearchText.isEmpty())
    {
      return true;
    }

    const QAbstractItemModel* const pModel = sourceModel();
    const i32 colCnt = pModel->columnCount(iSrcParent);

    for (i32 col = 0; col < colCnt; ++col)
    {
      if (mIgnoredCols.contains(col))
      {
        continue;
      }

      if (pModel->data(pModel->index(iSrcRow, col, iSrcParent)).toString().contains(mSearchText, Qt::CaseInsensitive))
      {
        return true;
      }
    }

    return false;
  }

private:
  QString mSearchText;
  QList<i32> mIgnoredCols;
  i32 mNoteCol = -1;

  void _begin_rows_filter_change()
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
#endif
  }

  void _end_rows_filter_change()
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateRowsFilter();
#endif
  }
};
