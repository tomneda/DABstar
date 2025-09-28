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
#include "dabradio.h"
#include <QHeaderView>
#include "setting-helper.h"


TiiListDisplay::TiiListDisplay()
{
  mpTableWidget.reset(new QTableWidget(0, cColNr));
  mpTableWidget->setHorizontalHeaderLabels(QStringList() << "Main" << "Sub" << "Level" << "Phase" << "Transmitter"
                                                         << "Distance" << "Dir" << "Power" << "Altitude" << "Height");

  mpWidget.reset(new QScrollArea(nullptr));
  mpWidget->setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  mpWidget->setWidgetResizable(true);
  mpWidget->setWindowTitle("TII list");
  mpWidget->setWidget(mpTableWidget.get());

  Settings::TiiViewer::posAndSize.read_widget_geometry(mpWidget.get(), 660, 250, false);
}

TiiListDisplay::~TiiListDisplay()
{
  Settings::TiiViewer::posAndSize.write_widget_geometry(mpWidget.get());
  mpTableWidget->setRowCount(0);
}

void TiiListDisplay::set_window_title(const QString & channel)
{
  mpWidget->setWindowTitle(channel);
}

void TiiListDisplay::start_adding()
{
  mpTableWidget->setRowCount(0);
  mpWidget->setWindowTitle("TII list");
}

void TiiListDisplay::finish_adding()
{
  mpTableWidget->resizeColumnsToContents();
}

i32 TiiListDisplay::get_nr_rows()
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

void TiiListDisplay::add_row(const STiiDataEntry & iTr, const SDerivedData & iDD)
{
  i16 row = mpTableWidget->rowCount();
  mpTableWidget->insertRow(row);

  mpTableWidget->setItem(row, 0, new QTableWidgetItem(QString::number(iTr.mainId)));
  mpTableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(iTr.subId)));
  mpTableWidget->setItem(row, 2, new QTableWidgetItem(QString::number(iDD.strength_dB, 'f', 1) + "dB"));
  mpTableWidget->setItem(row, 3, new QTableWidgetItem(QString::number(iDD.phase_deg, 'f', 0) + "°" + (iDD.isNonEtsiPhase ? '*' : ' ')));
  mpTableWidget->setItem(row, 4, new QTableWidgetItem(iTr.transmitterName));
  mpTableWidget->setItem(row, 5, new QTableWidgetItem(QString::number(iDD.distance_km, 'f', 1) + "km"));
  mpTableWidget->setItem(row, 6, new QTableWidgetItem(QString::number(iDD.corner_deg, 'f', 1) + "°"));
  mpTableWidget->setItem(row, 7, new QTableWidgetItem(QString::number(iTr.power, 'f', 1) + "kW"));
  mpTableWidget->setItem(row, 8, new QTableWidgetItem(QString::number(iTr.altitude) + "m"));
  mpTableWidget->setItem(row, 9, new QTableWidgetItem(QString::number(iTr.height) + "m"));

  for (i32 colIdx = 0; colIdx < cColNr; colIdx++)
  {
    if (colIdx == 4) // transmitter name is left aligned
    {
      mpTableWidget->item(row, colIdx)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    else
    {
      mpTableWidget->item(row, colIdx)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
  }
}

