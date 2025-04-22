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
#include  "dabradio.h"
#include  "openfiledialog.h"

//static const char *uep_rates  [] = {nullptr, "7/20", "2/5", "1/2", "3/5", "3/4"};
//static const char *eep_Arates [] = {nullptr, "1/4",  "3/8", "1/2", "3/4"};
//static const char *eep_Brates [] = {nullptr, "4/9",  "4/7", "4/6", "4/5"};

ContentTable::ContentTable(DabRadio * theRadio, QSettings * s, const QString & channel, int cols)
{
  this->theRadio = theRadio;
  this->dabSettings = s;
  this->channel = channel;
  this->columns = cols;
  dabSettings->beginGroup("contentTable");
  int x = dabSettings->value("position-x", 200).toInt();
  int y = dabSettings->value("position-y", 200).toInt();
  int wi = dabSettings->value("table-width", 200).toInt();
  int hi = dabSettings->value("table-height", 200).toInt();
  myWidget = new QScrollArea(nullptr);
  myWidget->resize(wi, hi);
  myWidget->setWidgetResizable(true);
  myWidget->move(x, y);
  dabSettings->endGroup();

  contentWidget = new QTableWidget(0, cols);
  contentWidget->setColumnWidth(0, 150);
  contentWidget->setColumnWidth(4, 150);
  myWidget->setWidget(contentWidget);
  contentWidget->setHorizontalHeaderLabels(QStringList() << tr("current ensemble"));

  connect(contentWidget, &QTableWidget::cellClicked, this, &ContentTable::_slot_select_service);
  connect(contentWidget, &QTableWidget::cellDoubleClicked, this, &ContentTable::_slot_dump);
  //connect(this, &ContentTable::signal_go_service, theRadio, &RadioInterface::slot_handle_content_selector);

  addRow();  // for the ensemble name
}

ContentTable::~ContentTable()
{
  dabSettings->beginGroup("contentTable");
  dabSettings->setValue("position-x", myWidget->pos().x());
  dabSettings->setValue("position-y", myWidget->pos().y());
  dabSettings->setValue("table-width", myWidget->width());
  dabSettings->setValue("table-height", myWidget->height());
  dabSettings->endGroup();
  clearTable();
  delete contentWidget;
  delete myWidget;
}

void ContentTable::clearTable()
{
  int rows = contentWidget->rowCount();
  for (int i = rows; i > 0; i--)
  {
    contentWidget->removeRow(i - 1);
  }
  addRow();  // for the ensemble name
}

void ContentTable::show()
{
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

void ContentTable::_slot_select_service(int row, int column)
{
  QTableWidgetItem * theItem = contentWidget->item(row, 0);

  if (row < 2)
  {
    return;
  }
  (void)column;
  QString theService = theItem->text();
  fprintf(stderr, "selecting %s\n", theService.toUtf8().data());
  emit signal_go_service(theService);
}

void ContentTable::_slot_dump(int /*row*/, int /*column*/)
{
  OpenFileDialog filenameFinder(dabSettings);
  FILE * dumpFile = filenameFinder.open_content_dump_file_ptr(channel);

  if (dumpFile == nullptr)
  {
    return;
  }

  for (int i = 0; i < contentWidget->rowCount(); i++)
  {
    for (int j = 0; j < contentWidget->columnCount(); j++)
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
  for (int i = 0; i < contentWidget->rowCount(); i++)
  {
    for (int j = 0; j < contentWidget->columnCount(); j++)
    {
      QString t = contentWidget->item(i, j)->text();
      fprintf(dumpFilePointer, "%s;", t.toUtf8().data());
    }
    fprintf(dumpFilePointer, "\n");
  }
}

int16_t ContentTable::addRow()
{
  int16_t row = contentWidget->rowCount();

  contentWidget->insertRow(row);

  for (int i = 0; i < columns; i++)
  {
    QTableWidgetItem * item0 = new QTableWidgetItem;
    item0->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    contentWidget->setItem(row, i, item0);
  }
  return row;
}

void ContentTable::addLine(const QString & s)
{
  int row = addRow();
  QStringList h = s.split(";");

  for (int i = 0; i < h.size(); i++)
  {
    if (i < columns)
    {    // just for safety
      contentWidget->item(row, i)->setText(h.at(i));
    }
  }
}

