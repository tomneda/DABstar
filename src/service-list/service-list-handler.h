/*
 * Copyright (c) 2023 by Thomas Neder (https://github.com/tomneda)
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

#ifndef DABSTAR_SERVICELISTHANDLER_H
#define DABSTAR_SERVICELISTHANDLER_H

#include <QObject>
#include <QStandardItemModel>
#include <QItemSelection>
#include "service-db.h"

class RadioInterface;
class QTableView;

class ServiceListHandler : public QObject
{
  Q_OBJECT
public:
  explicit ServiceListHandler(RadioInterface * ipRI, QTableView * const ipSL);
  ~ServiceListHandler() override = default;

  ServiceDB & DB() { return mServiceDB; }
  void update();

private:
  RadioInterface * const mpRadio;
  QTableView * const mpTableView;
  QStandardItemModel mModel{};
  ServiceDB mServiceDB;
  QString mLastChannel;

public slots:

private slots:
  void _slot_selection_changed(const QItemSelection &selected, const QItemSelection &deselected);

signals:
  void signal_channel_changed(const QString & oChannel, const QString & oService);
  void signal_service_changed(const QString & oService);
};


#endif // DABSTAR_SERVICELISTHANDLER_H
