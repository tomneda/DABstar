/*
 *    Copyright (C) 2024
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
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
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "dxDisplay.h"
#include "radio.h"
#include <QHeaderView>


DxDisplay::DxDisplay(QSettings * s)
{
  dxSettings = s;
  myWidget = new QScrollArea(nullptr);
  myWidget->setWidgetResizable(true);
  tableWidget = new QTableWidget(0, 6);
  QHeaderView * headerView = tableWidget->horizontalHeader();
  headerView->setSectionResizeMode(1, QHeaderView::Stretch);
  //headerView->resizeSection(0, 50);
  myWidget->setWidget(tableWidget);
  tableWidget->setHorizontalHeaderLabels(QStringList() << tr("Main") << ("Sub") << tr("dB") << ("Phase") << tr("Distance, Direction, Power, Height") << tr("Transmitter"));
  _set_position_and_size(s, myWidget, tableWidget, "DX_DISPLAY");
  myWidget->setWindowTitle("TII list");
}

DxDisplay::~DxDisplay()
{
  _store_widget_position(dxSettings, myWidget, tableWidget, "DX_DISPLAY");
  int16_t rows = tableWidget->rowCount();
  for (int row = rows; row > 0; row--)
    tableWidget->removeRow(row);
  delete tableWidget;
  delete myWidget;
}

void DxDisplay::set_window_title(const QString & channel)
{
  myWidget->setWindowTitle(channel);
}

void DxDisplay::clean_up()
{
  int16_t rows = tableWidget->rowCount();
  for (int row = rows; row >= 0; row--)
    tableWidget->removeRow(row);
  myWidget->setWindowTitle("TII list");
}

int DxDisplay::get_nr_rows()
{
  return tableWidget->rowCount();
}

void DxDisplay::show()
{
  myWidget->show();
  setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
}

void DxDisplay::hide()
{
  myWidget->hide();
}

void DxDisplay::add_row(const unsigned char mainId, const unsigned char subId, const float strength,
                       const float phase, const bool non_etsi_phase, const QString & ds, const QString & tr)
{
  int16_t row = tableWidget->rowCount();

  tableWidget->insertRow(row);
  QTableWidgetItem * item0 = new QTableWidgetItem;
  item0->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  tableWidget->setItem(row, 0, item0);

  QTableWidgetItem * item1 = new QTableWidgetItem;
  item1->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  tableWidget->setItem(row, 1, item1);

  QTableWidgetItem * item2 = new QTableWidgetItem;
  item2->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  tableWidget->setItem(row, 2, item2);

  QTableWidgetItem * item3 = new QTableWidgetItem;
  item2->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  tableWidget->setItem(row, 3, item3);

  QTableWidgetItem * item4 = new QTableWidgetItem;
  item2->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  tableWidget->setItem(row, 4, item4);

  QTableWidgetItem * item5 = new QTableWidgetItem;
  item2->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  tableWidget->setItem(row, 5, item5);

  //tableWidget->setCurrentItem(item0);
  tableWidget->item(row, 0)->setText(QString::number(mainId));
  tableWidget->item(row, 1)->setText(QString::number(subId));
  tableWidget->item(row, 2)->setText(QString::number(strength, 'f', 1));
	tableWidget->item(row, 3)->setText(QString::number(phase, 'f', 0) + " " + (non_etsi_phase ? '*':' '));
  tableWidget->item(row, 4)->setText(ds);
  tableWidget->item(row, 5)->setText(tr);
}

void DxDisplay::_set_position_and_size(QSettings * settings, QWidget * w, QTableWidget * t, const QString & key)
{
  settings->beginGroup(key);
  int x = settings->value(key + "-x", 100).toInt();
  int y = settings->value(key + "-y", 100).toInt();
  int wi = settings->value(key + "-w", 748).toInt();
  int he = settings->value(key + "-h", 200).toInt();
  int c0 = settings->value(key + "-c0", 39).toInt();
  int c1 = settings->value(key + "-c1", 41).toInt();
  int c2 = settings->value(key + "-c2", 52).toInt();
  int c3 = settings->value(key + "-c3", 44).toInt();
  int c4 = settings->value(key + "-c4", 268).toInt();
  int c5 = settings->value(key + "-c5", 268).toInt();
  settings->endGroup();
  w->resize(QSize(wi, he));
  w->move(QPoint(x, y));
  t->setColumnWidth(0, c0);
  t->setColumnWidth(1, c1);
  t->setColumnWidth(2, c2);
  t->setColumnWidth(3, c3);
  t->setColumnWidth(4, c4);
  t->setColumnWidth(5, c5);
}

void DxDisplay::_store_widget_position(QSettings * settings, QWidget * w, QTableWidget * t, const QString & key)
{
  settings->beginGroup(key);
  settings->setValue(key + "-x", w->pos().x());
  settings->setValue(key + "-y", w->pos().y());
  settings->setValue(key + "-w", w->size().width());
  settings->setValue(key + "-h", w->size().height());
  settings->setValue(key + "-c0", t->columnWidth(0));
  settings->setValue(key + "-c1", t->columnWidth(1));
  settings->setValue(key + "-c2", t->columnWidth(2));
  settings->setValue(key + "-c3", t->columnWidth(3));
  settings->setValue(key + "-c4", t->columnWidth(4));
  settings->setValue(key + "-c5", t->columnWidth(5));
  settings->endGroup();
}
