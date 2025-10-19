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

ContentTable::ContentTable(DabRadio * ipDabRadio, QSettings * /*s*/, const QString & iChannel, i32 iNumCols)
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
  mpTableWidget->setColumnWidth(0, 150);
  mpTableWidget->setColumnWidth(4, 150);
  mpScrollArea->setWidget(mpTableWidget.get());

  connect(mpTableWidget.get(), &QTableWidget::cellClicked, this, &ContentTable::_slot_select_service);
  connect(mpTableWidget.get(), &QTableWidget::cellDoubleClicked, this, &ContentTable::_slot_dump);
}

ContentTable::~ContentTable()
{
  Settings::FibContentViewer::posAndSize.write_widget_geometry(mpScrollArea.get());;
}

void ContentTable::show() const
{
  mpTableWidget->resizeColumnsToContents();
  mpScrollArea->show();
}

void ContentTable::hide() const
{
  mpScrollArea->hide();
}

bool ContentTable::is_visible() const
{
  return !mpScrollArea->isHidden();
}

void ContentTable::add_line(const QString & s)
{
  i32 row = _add_row();
  QStringList h = s.split(";");

  if (h.empty() || h.at(0).isEmpty())
  {
    return;
  }

  QColor fg, bg;

  switch (h.at(0).front().toLatin1())
  {
  case 'E': fg = Qt::black;
    bg = Qt::black;
    break; // Empty line
  case 'C': fg = Qt::yellow;
    bg = Qt::white;
    break; // Caption
  case 'H': fg = 0xFF8800;
    bg = Qt::white;
    break; // Header
  case 'D': fg = Qt::cyan;
    bg = Qt::white;
    break; // Data
  case 'I': fg = Qt::gray;
    bg = Qt::white;
    break; // Data
  default:
    qWarning() << "Unknown line type" << h.at(0).at(0);
    return;
  }

  for (i32 i = 1; i < h.size(); i++)
  {
    if (i <= mNumColumns)
    {
      auto * item = mpTableWidget->item(row, i - 1);
      QFont font = item->font();
      font.setBold(true);
      item->setFont(font);
      // if (bg != Qt::black || fg != Qt::black)
      {
        // item->setBackground(bg);
        item->setForeground(fg);
      }
      item->setText(h.at(i));
    }
  }
}

void ContentTable::dump(FILE * dumpFilePointer) const
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

i16 ContentTable::_add_row() const
{
  const i16 row = mpTableWidget->rowCount();
  mpTableWidget->insertRow(row);

  for (i32 i = 0; i < mNumColumns; i++)
  {
    QTableWidgetItem * item = new QTableWidgetItem; // there is no memory leakage here
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mpTableWidget->setItem(row, i, item);
  }
  return row;
}

uint32_t ContentTable::_hex_to_u32(const std::string & iHexString) const
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

void ContentTable::_slot_select_service(const i32 iRowIdx, const i32 /*iColIdx*/)
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

void ContentTable::_slot_dump(i32 /*row*/, i32 /*column*/)
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
