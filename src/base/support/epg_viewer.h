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

#include "glob_defs.h"
#include "spi_types.h"
#include <QDate>
#include <QTimer>
#include <QWidget>
#include <vector>

class EpgManager;
class QListWidget;
class QListWidgetItem;
class QSplitter;
class QTabBar;
class QTableWidget;
class QTextBrowser;

// EPG window: the services with programme data on the left, the programmes of the selected service and day on the right.
// Closing the window only hides it.
class EpgViewer : public QWidget
{
  Q_OBJECT

public:
  explicit EpgViewer(const EpgManager* ipEpgManager);
  ~EpgViewer() override;

  void set_current_service(u32 iSId); // preselects the service (e.g. the currently played audio service)
  void show_and_raise();

public slots:
  void slot_update();

protected:
  void hideEvent(QHideEvent* ipEvent) override;

private:
  static constexpr i32 cHighlightUpdateIntervalMs = 20'000;
  static constexpr i32 cLogoSize = 32;

  const EpgManager* const mpEpgManager;
  QListWidget* mpServiceList = nullptr;
  QTabBar* mpDayBar = nullptr;
  QTableWidget* mpProgrammeTable = nullptr;
  QTextBrowser* mpDetails = nullptr;
  QSplitter* mpSplitterH = nullptr;
  QSplitter* mpSplitterV = nullptr;
  QTimer mHighlightTimer;

  u32 mSelectedSId = 0;
  QDate mSelectedDay;
  std::vector<QDate> mDays;
  std::vector<Spi::SProgramme> mShownProgrammes;
  bool mScrollToCurrent = true;
  bool mIsUpdating = false;

  [[nodiscard]] QDateTime _to_display_time(const QDateTime& iUtc) const;
  void _update_service_list();
  void _update_day_bar();
  void _update_programme_table();
  void _update_highlight();
  void _show_details(i32 iRow);
  static QString _get_duration_str(i32 iSeconds);

private slots:
  void _slot_service_selected();
  void _slot_service_double_clicked(const QListWidgetItem* ipItem);
  void _slot_day_selected(i32 iIdx);
  void _slot_programme_selected();

signals:
  void signal_service_selected(u32 oSId); // the user wants to listen to this service
};
