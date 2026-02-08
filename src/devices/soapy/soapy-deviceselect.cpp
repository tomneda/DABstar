#
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
#include    "soapy-deviceselect.h"
#include    <cstdio>
#include    <QVBoxLayout>
//
//  Whenever there are two or more soapy devices connected
//  to the computer, the user is asked to make a choice.

soapy_deviceSelect::soapy_deviceSelect()
{
  toptext = new QLabel(this);
  toptext->setText("Select a device");
  selectorDisplay = new QListView (this);
  QVBoxLayout *layOut = new QVBoxLayout;
  layOut->addWidget(selectorDisplay);
  layOut->addWidget(toptext);
  setWindowTitle(tr("device select"));
  setLayout(layOut);

  deviceList.setStringList(Devices);
  Devices = QStringList();
  deviceList.setStringList(Devices);
  selectorDisplay->setModel(&deviceList);
  connect(selectorDisplay, SIGNAL(clicked(QModelIndex)),
          this, SLOT(selectDevice(QModelIndex)));
  selectedItem = -1;
}

soapy_deviceSelect::~soapy_deviceSelect() {}

void soapy_deviceSelect::addtoDeviceList(const char *v)
{
  QString s (v);

  Devices << s;
  deviceList.setStringList(Devices);
  selectorDisplay->setModel(&deviceList);
  selectorDisplay->adjustSize();
  adjustSize();
}

void soapy_deviceSelect::selectDevice(QModelIndex s)
{
  QDialog::done(s.row());
}
