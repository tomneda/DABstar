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
#include  <QSettings>
#include  "content-table.h"
#include  "openfiledialog.h"
#include  "setting-helper.h"

//static const char *uep_rates  [] = {nullptr, "7/20", "2/5", "1/2", "3/5", "3/4"};
//static const char *eep_Arates [] = {nullptr, "1/4",  "3/8", "1/2", "3/4"};
//static const char *eep_Brates [] = {nullptr, "4/9",  "4/7", "4/6", "4/5"};

ContentTable::ContentTable(DabRadio * theRadio, QSettings * /*s*/, const QString & channel, i32 cols)
{
  this->theRadio = theRadio;
  this->channel = channel;
  this->columns = cols;
  myWidget.reset(new QScrollArea(nullptr));
  Settings::FibContentViewer::posAndSize.read_widget_geometry(myWidget.get(), 400, 400);;
  myWidget->setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  myWidget->setWindowTitle("FIB content of current ensemble");
  myWidget->setWidgetResizable(true);

  contentWidget.reset(new QTableWidget(0, cols));
  contentWidget->setColumnWidth(0, 150);
  contentWidget->setColumnWidth(4, 150);
  myWidget->setWidget(contentWidget.get());

  connect(contentWidget.get(), &QTableWidget::cellClicked, this, &ContentTable::_slot_select_service);
  connect(contentWidget.get(), &QTableWidget::cellDoubleClicked, this, &ContentTable::_slot_dump);
  //connect(this, &ContentTable::signal_go_service, theRadio, &RadioInterface::slot_handle_content_selector);
}

ContentTable::~ContentTable()
{
  Settings::FibContentViewer::posAndSize.write_widget_geometry(myWidget.get());;
  // clearTable();
}

// void ContentTable::clearTable()
// {
//   i32 rows = contentWidget->rowCount();
//   for (i32 i = rows; i > 0; i--)
//   {
//     contentWidget->removeRow(i - 1);
//   }
//   addRow();  // for the ensemble name
// }

void ContentTable::show()
{
  contentWidget->resizeColumnsToContents();
  myWidget->show();
}

void ContentTable::hide()
{
  myWidget->hide();
}

bool ContentTable::isVisible()
{
  return !myWidget->isHidden();
}

void ContentTable::_slot_select_service(i32 row, i32 column)
{
  QTableWidgetItem * theItem = contentWidget->item(row, 0);

  if (row < 2)
  {
    return;
  }
  (void)column;
  const QString theService = theItem->text();
  qInfo() << "Selecting" << theService;
  emit signal_go_service(theService);
}

void ContentTable::_slot_dump(i32 /*row*/, i32 /*column*/)
{
  OpenFileDialog filenameFinder(&Settings::Storage::instance());
  FILE * dumpFile = filenameFinder.open_content_dump_file_ptr(channel);

  if (dumpFile == nullptr)
  {
    return;
  }

  for (i32 i = 0; i < contentWidget->rowCount(); i++)
  {
    for (i32 j = 0; j < contentWidget->columnCount(); j++)
    {
      QString t = contentWidget->item(i, j)->text();
      fprintf(dumpFile, "%s;", t.toUtf8().data());
    }
    fprintf(dumpFile, "\n");
  }
  fclose(dumpFile);
}

void ContentTable::dump(FILE * dumpFilePointer)
{
  if (dumpFilePointer == nullptr)
  {
    return;
  }
  for (i32 i = 0; i < contentWidget->rowCount(); i++)
  {
    for (i32 j = 0; j < contentWidget->columnCount(); j++)
    {
      QString t = contentWidget->item(i, j)->text();
      fprintf(dumpFilePointer, "%s;", t.toUtf8().data());
    }
    fprintf(dumpFilePointer, "\n");
  }
}

i16 ContentTable::addRow()
{
  i16 row = contentWidget->rowCount();

  contentWidget->insertRow(row);

  for (i32 i = 0; i < columns; i++)
  {
    QTableWidgetItem * item = new QTableWidgetItem;
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    contentWidget->setItem(row, i, item);
  }
  return row;
}

void ContentTable::addLine(const QString & s)
{
  i32 row = addRow();
  QStringList h = s.split(";");

  if (h.empty() || h.at(0).isEmpty())
  {
    return;
  }

  QColor fg, bg;

  switch (h.at(0).front().toLatin1())
  {
    case 'E': fg = Qt::black;  bg = Qt::black; break; // Empty line
    case 'C': fg = Qt::yellow; bg = Qt::white; break; // Caption
    case 'H': fg = 0xFF8800;   bg = Qt::white; break; // Header
    case 'D': fg = Qt::cyan;   bg = Qt::white; break; // Data
    default:
      qWarning() << "Unknown line type" << h.at(0).at(0);
      return;
  }

  for (i32 i = 1; i < h.size(); i++)
  {
    if (i <= columns)
    {
      auto * item = contentWidget->item(row, i - 1);
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

