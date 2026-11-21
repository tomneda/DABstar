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

// Proxy model which additionally filters rows by a free search text (case-insensitive) over all columns except an ignored one
class EnsembleListFilterProxy final : public QSortFilterProxyModel
{
public:
  using QSortFilterProxyModel::QSortFilterProxyModel;

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

  void set_ignored_column(const i32 iCol)
  {
    if (iCol == mIgnoredCol)
    {
      return;
    }
    _begin_rows_filter_change();
    mIgnoredCol = iCol;
    _end_rows_filter_change();
  }

  [[nodiscard]] const QString& get_search_text() const { return mSearchText; }

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
      if (col == mIgnoredCol)
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
  i32 mIgnoredCol = -1;

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
