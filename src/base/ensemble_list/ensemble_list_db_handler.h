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
#include "ensemble_list_db.h"
#include <QObject>
#include <QStyledItemDelegate>
#include "ensemble_list_filter_proxy.h"

class QTableView;
class QSettings;

constexpr u32 cBgColorUnselected = 0x3a3e46;
constexpr u32 cBgColorNewEntries = 0x1e6b52;
constexpr u32 cBgColorFailed     = 0x5a2f3a;

class CustomItemDelegate2 final: public QStyledItemDelegate
{
  using QStyledItemDelegate::QStyledItemDelegate;
  Q_OBJECT

public:
  void set_current_fid_or_ch(const QString& iFIdOrCh, const i32 iFIdOrChColIdx) { mCurFIdOrCh = iFIdOrCh; mCurFIdOrChColIdx = iFIdOrChColIdx; }
  void set_current_scan_level(const i32 iScanLevelIdx) { mCurScanLevelIdx = iScanLevelIdx; }

protected:
  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

private:
  const QPixmap mStarEmptyPixmap{":res/icons/starempty16.png"};   // draw a empty star icon for favorites
  const QPixmap mStarFilledPixmap{":res/icons/starfilled16.png"}; // draw a filled star icon for favorites
  QString mCurFIdOrCh;
  i32 mCurFIdOrChColIdx = 0;
  i32 mCurScanLevelIdx = 0;

signals:
  void signal_selection_changed(const QString& oFIdOrCh);
};

class EnsembleListDbHandler : public QObject
{
  Q_OBJECT
public:
  EnsembleListDbHandler(const DabDatabase& iDabDatabase, QTableView* const ipSL);
  ~EnsembleListDbHandler() override = default;

  using EDataMode = EnsembleListDB::EDataMode;
  using TSIdList = QList<u32>;

  void set_data_mode(EDataMode iDataMode);
  void delete_table();
  void create_new_table();
  [[nodiscard]] bool is_table_existing(EDataMode iDataMode) const;
  void insert_or_update_entry(const EnsembleListDB::SDbEntryData& iEntryData, EnsembleListDB::EDbDataType iDataType);
  bool delete_entry(const QString& iFIdOrCh);
  bool delete_invalid_entries();
  [[nodiscard]] EnsembleListDB::SDbEntryData get_entry(const QString& iFIdOrCh) const;
  [[nodiscard]] i32 get_nr_unscanned_entries() const; // get number of entries with EId not given yet
  [[nodiscard]] bool is_entry_existing(const QString& iFIdOrCh) const;
  i32 set_selector(const QString& iFId);
  [[nodiscard]] QString get_full_path(const QString& iFId) const;
  void jump_entries(i32 iSteps); // typ -1/+1, with wrap around
  void set_data_filter(const EnsembleListDB::SDataFilter& iDataFilter);
  void set_search_text(const QString& iText);
  void set_sorting_to_FId_or_Ch(bool iRestoreSorting);
  [[nodiscard]] EnsembleListDB::SUserData get_user_data(const EnsembleListDB::SUserDataKey& iKey) const;
  void set_favorite(const EnsembleListDB::SUserDataKey& iKey, bool iIsFav);
  void set_note(const EnsembleListDB::SUserDataKey& iKey, const QString& iNote);

private:
  QTableView* const mpTableView;
  EnsembleListFilterProxy mProxyModel;
  EnsembleListDB mEnsembleListDb;
  CustomItemDelegate2 mCustomItemDelegate;
  QString mFIdOrChLast;
  EnsembleListDB::SUserDataKey mUserDataKeyLast; // user data key of mFIdOrChLast
  bool mHideUnusedEntries = false;
  i32 mOldSortCol = -1;
  Qt::SortOrder mOldSortOrder = Qt::AscendingOrder;

  void _fill_table_view_from_db();
  i32 _jump_to_list_entry(i32 iSkipOffset = 0, bool iCenterContent = false); // returns row index
  i32 _get_fid_or_ch_col_idx() const;
  i32 _get_scan_level_col_idx() const;
  i32 _get_note_col_idx() const;
  void _hide_columns() const;
  void _set_current_fid_or_ch(const QString& iFIdOrCh);
  void _update_user_data_key();
  void _emit_entry_stats();

private slots:
  void _slot_selection_changed(const QString& iFIdOrCh);
  void _slot_header_clicked(i32 iIndex);

signals:
  void signal_selection_changed(const QString& oFId);
  void signal_current_entry_changed(const EnsembleListDB::SUserDataKey& oKey); // the user data key of the highlighted (current) entry has changed (also its EId), invalid if none
  void signal_entry_stats_changed(i32 oNrShown, const EnsembleListDB::SEntryCounts& oCounts); // number of shown entries and counts of all entries
};
