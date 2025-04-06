/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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
#include "tii_list_display.h"
#include "radio.h"
#include <QHeaderView>


TiiListDisplay::TiiListDisplay(QSettings * s)
{
  mpSettings = s;

  mpTableWidget.reset(new QTableWidget(0, 6));
  mpTableWidget->setHorizontalHeaderLabels(QStringList() << tr("Main") << ("Sub") << tr("dB") << ("Phase") << tr("Transmitter") << tr("Distance, Direction, Power, Height"));

  mpWidget.reset(new QScrollArea(nullptr));
  mpWidget->setWidgetResizable(true);
  mpWidget->setWindowTitle("TII list");
  mpWidget->setWidget(mpTableWidget.get());

  _set_position_and_size(s, mpWidget, mpTableWidget, "DX_DISPLAY");
}

TiiListDisplay::~TiiListDisplay()
{
  _store_widget_position(mpSettings, mpWidget, mpTableWidget, "DX_DISPLAY");
  mpTableWidget->setRowCount(0);
}

void TiiListDisplay::set_window_title(const QString & channel)
{
  mpWidget->setWindowTitle(channel);
}

void TiiListDisplay::clean_up()
{
  mpTableWidget->setRowCount(0);
  mpWidget->setWindowTitle("TII list");
}

int TiiListDisplay::get_nr_rows()
{
  return mpTableWidget->rowCount();
}

void TiiListDisplay::show()
{
  mpWidget->show();
  setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
}

void TiiListDisplay::hide()
{
  mpWidget->hide();
}

void TiiListDisplay::add_row(const uint8_t mainId, const uint8_t subId, const float strength,
                             const float phase, const bool non_etsi_phase, const QString & ds, const QString & tr)
{
  int16_t row = mpTableWidget->rowCount();
  mpTableWidget->insertRow(row);

  mpTableWidget->setItem(row, 0, new QTableWidgetItem(QString::number(mainId)));
  mpTableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(subId)));
  mpTableWidget->setItem(row, 2, new QTableWidgetItem(QString::number(strength, 'f', 1)));
  mpTableWidget->setItem(row, 3, new QTableWidgetItem(QString::number(phase, 'f', 0) + " " + (non_etsi_phase ? '*' : ' ')));
  mpTableWidget->setItem(row, 4, new QTableWidgetItem(tr));
  mpTableWidget->setItem(row, 5, new QTableWidgetItem(ds));

  mpTableWidget->item(row, 0)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
  mpTableWidget->item(row, 1)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
  mpTableWidget->item(row, 2)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
  mpTableWidget->item(row, 3)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
  mpTableWidget->item(row, 4)->setTextAlignment(Qt::AlignLeft  | Qt::AlignVCenter);
  mpTableWidget->item(row, 5)->setTextAlignment(Qt::AlignLeft  | Qt::AlignVCenter);
}

void TiiListDisplay::format_content()
{
  mpTableWidget->resizeColumnsToContents();
}

void TiiListDisplay::_set_position_and_size(QSettings * settings, QScopedPointer<QScrollArea> & w, QScopedPointer<QTableWidget> & t, const QString & key)
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

void TiiListDisplay::_store_widget_position(QSettings * settings, QScopedPointer<QScrollArea> & w, QScopedPointer<QTableWidget> & t, const QString & key)
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
