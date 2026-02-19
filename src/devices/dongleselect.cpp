/*
 *    Copyright (C) 2013 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
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

#include    "dongleselect.h"
#include    <QVBoxLayout>

//  Whenever there are two or more devices connected
//  to the computer, the user is asked to make a choice.

dongleSelect::dongleSelect()
{
  toptext = new QLabel(this);
  toptext->setText("Select a device");
  selectorDisplay = new QListView (this);
  QVBoxLayout *layOut = new QVBoxLayout;
  layOut->addWidget(selectorDisplay);
  layOut->addWidget(toptext);
  setWindowTitle(tr("device select"));
  setLayout(layOut);

  Dongles = QStringList();
  dongleList.setStringList(Dongles);
  selectorDisplay->setModel(&dongleList);
  connect(selectorDisplay, &QListView::clicked,
          this, &dongleSelect::selectDongle);
  selectedItem = -1;
}

dongleSelect::~dongleSelect() {}

void dongleSelect::addtoDongleList(const QString &s)
{
  Dongles << s;
  dongleList.setStringList(Dongles);
  selectorDisplay->setModel(&dongleList);
  selectorDisplay->adjustSize();
  adjustSize();
}

void dongleSelect::selectDongle(QModelIndex s)
{
  QDialog::done(s.row());
}
