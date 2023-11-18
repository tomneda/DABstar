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

#include <QTreeView>
#include "radio.h"
#include "ServiceListHandler.h"

ServiceListHandler::ServiceListHandler(RadioInterface * ipRI, QTreeView * const ipSL) :
  mpRadio(ipRI),
  mpTreeView(ipSL)
{
  QStringList headerLabels;
  headerLabels << "Fav" << "Service" << "Ch";
  mModel.setHorizontalHeaderLabels(headerLabels);

  QList<QStandardItem*> rowData;
  rowData << new QStandardItem("Line 1") << new QStandardItem("30") << new QStandardItem("77");
  mModel.appendRow(rowData);
  rowData.clear();
  rowData << new QStandardItem("Line 2") << new QStandardItem("40") << new QStandardItem("88");
  mModel.appendRow(rowData);
//  mModel.insertRow(0, rowData);
//  mModel.setItem(0, 0, new QStandardItem("A"));
//  mModel.setItem(0, 1, new QStandardItem("B"));
//  mModel.setItem(0, 2, new QStandardItem("C"));
  mpTreeView->setModel(&mModel);
  //mpTreeView->setModel(pStringListModel);

  //mpTreeView->setRootIndex(mModel.index(0, 0)); // Setze den Index auf den Anfang des Models

  //mpTreeView->show();
  mpTreeView->hide();


}
