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
#include "epg_viewer.h"
#include "epg_manager.h"
#include "setting_helper.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QSplitter>
#include <QTabBar>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimeZone>
#include <QVBoxLayout>

namespace
{
enum EColumn : i32
{
  ColTime = 0,
  ColDuration,
  ColTitle,
  ColNum
};
}

EpgViewer::EpgViewer(const EpgManager* const ipEpgManager)
  : QWidget(nullptr)
  , mpEpgManager(ipEpgManager)
{
  setWindowTitle(PRJ_NAME " - EPG");
  setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  setAttribute(Qt::WA_QuitOnClose, false);

  mpServiceList = new QListWidget(this);
  mpServiceList->setIconSize(QSize(cLogoSize, cLogoSize));
  mpServiceList->setMinimumWidth(120);

  mpDayBar = new QTabBar(this);
  mpDayBar->setExpanding(false);
  mpDayBar->setUsesScrollButtons(true);

  mpProgrammeTable = new QTableWidget(0, ColNum, this);
  mpProgrammeTable->setHorizontalHeaderLabels({ "Time", "Duration", "Programme" });
  mpProgrammeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  mpProgrammeTable->setSelectionMode(QAbstractItemView::SingleSelection);
  mpProgrammeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  mpProgrammeTable->setWordWrap(false);
  mpProgrammeTable->verticalHeader()->hide();
  mpProgrammeTable->horizontalHeader()->setSectionResizeMode(ColTime, QHeaderView::ResizeToContents);
  mpProgrammeTable->horizontalHeader()->setSectionResizeMode(ColDuration, QHeaderView::ResizeToContents);
  mpProgrammeTable->horizontalHeader()->setStretchLastSection(true);

  mpDetails = new QTextBrowser(this);
  mpDetails->setOpenExternalLinks(true);

  mpSplitterV = new QSplitter(Qt::Vertical, this);
  mpSplitterV->addWidget(mpProgrammeTable);
  mpSplitterV->addWidget(mpDetails);
  mpSplitterV->setStretchFactor(0, 3);
  mpSplitterV->setStretchFactor(1, 1);
  mpSplitterV->setChildrenCollapsible(false);

  auto* const pRight = new QWidget(this);
  auto* const pRightLayout = new QVBoxLayout(pRight);
  pRightLayout->setContentsMargins(0, 0, 0, 0);
  pRightLayout->addWidget(mpDayBar);
  pRightLayout->addWidget(mpSplitterV);

  mpSplitterH = new QSplitter(Qt::Horizontal, this);
  mpSplitterH->addWidget(mpServiceList);
  mpSplitterH->addWidget(pRight);
  mpSplitterH->setStretchFactor(0, 0);
  mpSplitterH->setStretchFactor(1, 1);
  mpSplitterH->setChildrenCollapsible(false);

  auto* const pLayout = new QHBoxLayout(this);
  pLayout->setContentsMargins(4, 4, 4, 4);
  pLayout->addWidget(mpSplitterH);

  resize(900, 600);
  Settings::EpgViewer::posAndSize.read_widget_geometry(this);

  if (!mpSplitterH->restoreState(Settings::EpgViewer::varSplitterStateH.read().toByteArray()))
  {
    mpSplitterH->setSizes({ 220, std::max(300, width() - 220) });
  }
  mpSplitterV->restoreState(Settings::EpgViewer::varSplitterStateV.read().toByteArray());

  connect(mpServiceList, &QListWidget::itemSelectionChanged, this, &EpgViewer::_slot_service_selected);
  connect(mpServiceList, &QListWidget::itemDoubleClicked, this, &EpgViewer::_slot_service_double_clicked);
  connect(mpDayBar, &QTabBar::currentChanged, this, &EpgViewer::_slot_day_selected);
  connect(mpProgrammeTable, &QTableWidget::itemSelectionChanged, this, &EpgViewer::_slot_programme_selected);
  connect(&mHighlightTimer, &QTimer::timeout, this, &EpgViewer::_update_highlight);

  slot_update();
}

EpgViewer::~EpgViewer()
{
  mHighlightTimer.stop();
  Settings::EpgViewer::posAndSize.write_widget_geometry(this);
  Settings::EpgViewer::varSplitterStateH.write(mpSplitterH->saveState());
  Settings::EpgViewer::varSplitterStateV.write(mpSplitterV->saveState());
}

void EpgViewer::hideEvent(QHideEvent* const ipEvent)
{
  mHighlightTimer.stop();
  Settings::EpgViewer::posAndSize.write_widget_geometry(this);
  Settings::EpgViewer::varSplitterStateH.write(mpSplitterH->saveState());
  Settings::EpgViewer::varSplitterStateV.write(mpSplitterV->saveState());
  QWidget::hideEvent(ipEvent);
}

void EpgViewer::show_and_raise()
{
  mScrollToCurrent = true;
  slot_update();
  show();
  raise();
  activateWindow();
  mHighlightTimer.start(cHighlightUpdateIntervalMs);
}

void EpgViewer::set_current_service(const u32 iSId)
{
  if (iSId == 0 || iSId == mSelectedSId)
  {
    return;
  }

  mSelectedSId = iSId;
  mSelectedDay = QDate();
  mScrollToCurrent = true;
  slot_update();
}

void EpgViewer::slot_update()
{
  if (mIsUpdating)
  {
    return;
  }

  mIsUpdating = true;
  _update_service_list();
  _update_day_bar();
  _update_programme_table();
  mIsUpdating = false;
}

QDateTime EpgViewer::_to_display_time(const QDateTime& iUtc) const
{
  if (Settings::Config::cbUseUtcTime.read().toBool())
  {
    return iUtc.toTimeZone(QTimeZone::UTC);
  }
  // show the times in the local time of the ensemble, as the programmes are billed in this time
  return iUtc.toTimeZone(QTimeZone::fromSecondsAheadOfUtc(mpEpgManager->get_lto_minutes() * 60));
}

void EpgViewer::_update_service_list()
{
  const std::vector<u32> SIds = mpEpgManager->get_service_ids();

  // keep the current selection if possible, else take the first service
  if (std::find(SIds.cbegin(), SIds.cend(), mSelectedSId) == SIds.cend())
  {
    if (!SIds.empty())
    {
      mSelectedSId = SIds.front();
      mSelectedDay = QDate();
      mScrollToCurrent = true;
    }
  }

  mpServiceList->clear();

  for (const u32 SId : SIds)
  {
    auto* const pItem = new QListWidgetItem(mpEpgManager->get_service_label(SId), mpServiceList);
    pItem->setData(Qt::UserRole, SId);
    pItem->setToolTip(QString("SId %1\nDouble-click to select this service").arg(SId, 0, 16).toUpper());

    if (const QPixmap logo = mpEpgManager->get_logo(SId, cLogoSize); !logo.isNull())
    {
      pItem->setIcon(QIcon(logo.scaled(cLogoSize, cLogoSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }

    if (SId == mSelectedSId)
    {
      pItem->setSelected(true);
      mpServiceList->setCurrentItem(pItem);
    }
  }
}

void EpgViewer::_update_day_bar()
{
  mShownProgrammes = mpEpgManager->get_programmes(mSelectedSId);

  mDays.clear();
  for (const auto& p : mShownProgrammes)
  {
    const QDate day = _to_display_time(p.startTime).date();
    if (mDays.empty() || mDays.back() != day) // the programmes are sorted by the start time
    {
      mDays.push_back(day);
    }
  }

  const QDate today = _to_display_time(mpEpgManager->get_current_time()).date();

  if (std::find(mDays.cbegin(), mDays.cend(), mSelectedDay) == mDays.cend())
  {
    // prefer today, else the first day with programmes
    mSelectedDay = (std::find(mDays.cbegin(), mDays.cend(), today) != mDays.cend() ? today : (mDays.empty() ? QDate() : mDays.front()));
    mScrollToCurrent = true;
  }

  while (mpDayBar->count() > 0)
  {
    mpDayBar->removeTab(0);
  }

  for (usize i = 0; i < mDays.size(); ++i)
  {
    const QDate& day = mDays[i];
    QString text = QLocale().toString(day, "ddd dd.MM.");
    if (day == today)
    {
      text = "Today";
    }
    else if (day == today.addDays(1))
    {
      text = "Tomorrow";
    }
    mpDayBar->addTab(text);
    if (day == mSelectedDay)
    {
      mpDayBar->setCurrentIndex((i32)i);
    }
  }
}

void EpgViewer::_update_programme_table()
{
  // only keep the programmes of the selected day
  std::vector<Spi::SProgramme> dayProgrammes;
  for (const auto& p : mShownProgrammes)
  {
    if (_to_display_time(p.startTime).date() == mSelectedDay)
    {
      dayProgrammes.push_back(p);
    }
  }
  mShownProgrammes = std::move(dayProgrammes);

  const i32 selectedRow = mpProgrammeTable->currentRow();

  mpProgrammeTable->clearContents();
  mpProgrammeTable->setRowCount((i32)mShownProgrammes.size());

  for (i32 row = 0; row < (i32)mShownProgrammes.size(); ++row)
  {
    const Spi::SProgramme& p = mShownProgrammes[row];
    QString timeStr = _to_display_time(p.startTime).toString("hh:mm");
    if (const QDateTime stop = p.stop_time(); stop.isValid())
    {
      timeStr += " - " + _to_display_time(stop).toString("hh:mm");
    }

    QString title = p.names.best();
    if (!p.isOnAir)
    {
      title += " (off-air)";
    }

    auto* const pTimeItem = new QTableWidgetItem(timeStr);
    auto* const pDurationItem = new QTableWidgetItem(_get_duration_str(p.durationSec));
    auto* const pTitleItem = new QTableWidgetItem(title);
    pDurationItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pTitleItem->setToolTip(p.shortDescription.isEmpty() ? p.longDescription : p.shortDescription);

    mpProgrammeTable->setItem(row, ColTime, pTimeItem);
    mpProgrammeTable->setItem(row, ColDuration, pDurationItem);
    mpProgrammeTable->setItem(row, ColTitle, pTitleItem);
  }

  if (selectedRow >= 0 && selectedRow < mpProgrammeTable->rowCount() && !mScrollToCurrent)
  {
    mpProgrammeTable->selectRow(selectedRow);
  }

  _update_highlight();

  if (mpProgrammeTable->currentRow() < 0)
  {
    mpDetails->clear();
    if (mShownProgrammes.empty())
    {
      mpDetails->setText("No programme information received yet.");
    }
  }
}

void EpgViewer::_update_highlight()
{
  const QDateTime now = mpEpgManager->get_current_time();
  const QPalette pal = mpProgrammeTable->palette();
  i32 currentRow = -1;

  for (i32 row = 0; row < (i32)mShownProgrammes.size(); ++row)
  {
    const Spi::SProgramme& p = mShownProgrammes[row];
    const QDateTime stop = p.stop_time();
    const bool isPast = stop.isValid() && stop <= now;
    const bool isCurrent = p.startTime <= now && stop.isValid() && now < stop;

    if (isCurrent)
    {
      currentRow = row;
    }

    for (i32 col = 0; col < ColNum; ++col)
    {
      QTableWidgetItem* const pItem = mpProgrammeTable->item(row, col);
      if (pItem == nullptr)
      {
        continue;
      }

      QFont font = pItem->font();
      font.setBold(isCurrent);
      pItem->setFont(font);
      pItem->setForeground(isPast ? pal.color(QPalette::Disabled, QPalette::Text) : pal.color(QPalette::Active, QPalette::Text));
      pItem->setBackground(isCurrent ? QBrush(QColor(0x40, 0x88, 0x70, 0x60)) : QBrush());
    }
  }

  if (mScrollToCurrent && mpProgrammeTable->rowCount() > 0)
  {
    mScrollToCurrent = false;
    const i32 row = (currentRow >= 0 ? currentRow : 0);
    mpProgrammeTable->selectRow(row);
    mpProgrammeTable->scrollToItem(mpProgrammeTable->item(row, ColTitle), QAbstractItemView::PositionAtCenter);
  }
}

void EpgViewer::_show_details(const i32 iRow)
{
  if (iRow < 0 || iRow >= (i32)mShownProgrammes.size())
  {
    mpDetails->clear();
    return;
  }

  const Spi::SProgramme& p = mShownProgrammes[iRow];
  QString html = "<h3>" + p.names.best().toHtmlEscaped() + "</h3>";

  QString timeStr = _to_display_time(p.startTime).toString("ddd dd.MM.yyyy hh:mm");
  if (const QDateTime stop = p.stop_time(); stop.isValid())
  {
    timeStr += " - " + _to_display_time(stop).toString("hh:mm") + " (" + _get_duration_str(p.durationSec) + ")";
  }
  html += "<p><i>" + timeStr.toHtmlEscaped() + "</i></p>";

  if (!p.shortDescription.isEmpty())
  {
    html += "<p>" + p.shortDescription.toHtmlEscaped() + "</p>";
  }
  if (!p.longDescription.isEmpty() && p.longDescription != p.shortDescription)
  {
    html += "<p>" + p.longDescription.toHtmlEscaped().replace("\n", "<br>") + "</p>";
  }
  if (p.isRecommended)
  {
    html += "<p><b>Recommended</b></p>";
  }
  if (!p.isOnAir)
  {
    html += "<p>Not broadcast (off-air)</p>";
  }

  mpDetails->setHtml(html);
}

QString EpgViewer::_get_duration_str(const i32 iSeconds)
{
  if (iSeconds < 0)
  {
    return {};
  }

  const i32 minutes = (iSeconds + 30) / 60;
  if (minutes < 60)
  {
    return QString("%1 min").arg(minutes);
  }
  return QString("%1:%2 h").arg(minutes / 60).arg(minutes % 60, 2, 10, QChar('0'));
}

void EpgViewer::_slot_service_selected()
{
  if (mIsUpdating)
  {
    return;
  }

  const QList<QListWidgetItem*> items = mpServiceList->selectedItems();
  if (items.isEmpty())
  {
    return;
  }

  const u32 SId = items.front()->data(Qt::UserRole).toUInt();
  if (SId == mSelectedSId)
  {
    return;
  }

  mSelectedSId = SId;
  mSelectedDay = QDate();
  mScrollToCurrent = true;

  mIsUpdating = true;
  _update_day_bar();
  _update_programme_table();
  mIsUpdating = false;
}

void EpgViewer::_slot_service_double_clicked(const QListWidgetItem* const ipItem)
{
  if (ipItem != nullptr)
  {
    emit signal_service_selected(ipItem->data(Qt::UserRole).toUInt());
  }
}

void EpgViewer::_slot_day_selected(const i32 iIdx)
{
  if (mIsUpdating || iIdx < 0 || iIdx >= (i32)mDays.size())
  {
    return;
  }

  mSelectedDay = mDays[iIdx];
  mScrollToCurrent = true;

  mIsUpdating = true;
  _update_day_bar();
  _update_programme_table();
  mIsUpdating = false;
}

void EpgViewer::_slot_programme_selected()
{
  _show_details(mpProgrammeTable->currentRow());
}
