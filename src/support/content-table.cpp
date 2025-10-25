/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013, 2014, 2015, 2016, 2017, 2018, 2019
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include  <QDataStream>
#include  "content-table.h"
#include  "openfiledialog.h"
#include  "setting-helper.h"

bool SegmentedTableWidgetItem::operator<(const QTableWidgetItem & iOther) const
{
  // returning true then current and another element are swapped
  const QVariant currData = data(Qt::UserRole);
  const QVariant otherData = iOther.data(Qt::UserRole);

  if (currData.isNull() || otherData.isNull())
  {
    return false;
  }

  const QList<QVariant> currList = currData.toList();
  const QList<QVariant> otherList = otherData.toList();

  const i32 currSegmentId = currList.first().toInt();
  const i32 otherSegmentId = otherList.first().toInt();

  const char currRowType = currList.last().toChar().toLatin1();
  const char otherRowType = otherList.last().toChar().toLatin1();

  if (currRowType == 'D' && otherRowType == 'D' && currSegmentId == otherSegmentId) // look only to data rows from same segment
  {
    // try to interpret data first as a (positive) number
    bool currOk = false;
    const u32 currVal = text().toUInt(&currOk);
    bool otherOk = false;
    const u32 otherVal = iOther.text().toUInt(&otherOk);

    if (currOk && otherOk)
    {
      return currVal < otherVal;
    }

    return text().toUpper() < iOther.text().toUpper();
  }

  return false;
}


FibContentTable::FibContentTable(DabRadio * ipDabRadio, QSettings * /*s*/, const QString & iChannel, i32 iNumCols)
  : mChannel(iChannel)
  , mNumColumns(iNumCols)
  , mpDabRadio(ipDabRadio)
{
  mpScrollArea.reset(new QScrollArea(nullptr));
  Settings::FibContentViewer::posAndSize.read_widget_geometry(mpScrollArea.get(), 400, 400);;
  mpScrollArea->setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  mpScrollArea->setWindowTitle("FIB content of current ensemble");
  mpScrollArea->setWidgetResizable(true);

  mpTableWidget.reset(new QTableWidget(0, iNumCols));
  mpTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  mpTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
  mpScrollArea->setWidget(mpTableWidget.get());

  connect(mpTableWidget.get(), &QTableWidget::cellClicked, this, &FibContentTable::_slot_select_service);
  connect(mpTableWidget.get(), &QTableWidget::cellDoubleClicked, this, &FibContentTable::_slot_dump);
}

FibContentTable::~FibContentTable()
{
  Settings::FibContentViewer::posAndSize.write_widget_geometry(mpScrollArea.get());;
}

void FibContentTable::show() const
{
  mpTableWidget->resizeColumnsToContents();
  mpScrollArea->show();
  mpTableWidget->setSortingEnabled(true);
  mpTableWidget->sortByColumn(0, Qt::AscendingOrder);
}

void FibContentTable::hide() const
{
  mpScrollArea->hide();
}

bool FibContentTable::is_visible() const
{
  return !mpScrollArea->isHidden();
}

void FibContentTable::add_line(const QString & s)
{
  i32 row = _add_row();
  QStringList h = s.split(";");

  if (h.empty() || h.at(0).isEmpty())
  {
    return;
  }

  QColor fg;

  const char lineType = h.at(0).front().toLatin1();

  switch (lineType)
  {
  case 'E': fg = Qt::black;  mSegmentId++; break; // Empty line
  case 'C': fg = Qt::yellow; mSegmentId++; break; // Caption
  case 'H': fg = 0xFF8800;   mSegmentId++; break; // Header
  case 'D': fg = Qt::cyan;                 break; // Data, keep segment number
  case 'I': fg = Qt::gray;   mSegmentId++; break; // Comment
  default:
    qWarning() << "Unknown line type" << h.at(0).at(0);
    return;
  }

  for (i32 i = 1; i < h.size(); i++)
  {
    if (i <= mNumColumns)
    {
      auto * item = mpTableWidget->item(row, i - 1);

      QList<QVariant> itemData;
      itemData.append(mSegmentId);
      itemData.append(QChar(lineType));
      item->setData(Qt::UserRole, itemData);

      QFont font = item->font();
      font.setBold(true);
      item->setFont(font);
      item->setForeground(fg);
      item->setText(h.at(i));
    }
  }
}

void FibContentTable::dump(FILE * dumpFilePointer) const
{
  if (dumpFilePointer == nullptr)
  {
    return;
  }
  for (i32 i = 0; i < mpTableWidget->rowCount(); i++)
  {
    for (i32 j = 0; j < mpTableWidget->columnCount(); j++)
    {
      QString t = mpTableWidget->item(i, j)->text();
      fprintf(dumpFilePointer, "%s;", t.toUtf8().data());
    }
    fprintf(dumpFilePointer, "\n");
  }
}

i16 FibContentTable::_add_row() const
{
  const i16 row = mpTableWidget->rowCount();
  mpTableWidget->insertRow(row);

  for (i32 i = 0; i < mNumColumns; i++)
  {
    QTableWidgetItem * item = new SegmentedTableWidgetItem; // there is no memory leakage here
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mpTableWidget->setItem(row, i, item);
  }
  return row;
}

uint32_t FibContentTable::_hex_to_u32(const std::string & iHexString) const
{
  try
  {
    return static_cast<uint32_t>(std::stoul(iHexString, nullptr, 16));
  }
  catch (const std::invalid_argument & e)
  {
    qCritical() << "Invalid argument:" << e.what();
    return 0;
  }
  catch (const std::out_of_range & e)
  {
    qCritical() << "Out of range:" << e.what();
    return 0;
  }
}

void FibContentTable::_slot_select_service(const i32 iRowIdx, const i32 /*iColIdx*/)
{
  QTableWidgetItem * theItem = mpTableWidget->item(iRowIdx, 2);

  if (iRowIdx < 2)
  {
    return;
  }

  const QString serviceIdStr = theItem->text();
  const u32 serviceId = _hex_to_u32(serviceIdStr.toStdString());

  emit signal_go_service_id(serviceId);
}

void FibContentTable::_slot_dump(i32 /*row*/, i32 /*column*/)
{
  OpenFileDialog filenameFinder(&Settings::Storage::instance());
  FILE * dumpFile = filenameFinder.open_content_dump_file_ptr(mChannel);

  if (dumpFile == nullptr)
  {
    return;
  }

  for (i32 i = 0; i < mpTableWidget->rowCount(); i++)
  {
    for (i32 j = 0; j < mpTableWidget->columnCount(); j++)
    {
      QString t = mpTableWidget->item(i, j)->text();
      fprintf(dumpFile, "%s;", t.toUtf8().data());
    }
    fprintf(dumpFile, "\n");
  }
  fclose(dumpFile);
}
