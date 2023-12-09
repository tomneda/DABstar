/*
 * Copyright (c) 2023 by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
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

#include <QTableView>
#include "radio.h"
#include "service-list-handler.h"

ServiceListHandler::ServiceListHandler(RadioInterface * ipRI, QTableView * const ipSL) :
  mpRadio(ipRI),
  mpTableView(ipSL),
  mServiceDB("/home/work/servicelist_v01.db")
{
  mServiceDB.delete_table();
  mServiceDB.create_table();
  //mServiceDB.add_entry("5D","Antenne");
  //mServiceDB.add_entry("5C","AberHallo");

  //mServiceDB.bind_to_table_view(mpTableView);
  mpTableView->setModel(mServiceDB.create_model());
  //ipQTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  mpTableView->resizeColumnsToContents();
  mpTableView->show();
}

void ServiceListHandler::update()
{
  mpTableView->setModel(mServiceDB.create_model());
  mpTableView->resizeColumnsToContents();
  mpTableView->show();
}
