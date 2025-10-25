/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2021
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
 *    along with dab-scanner; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef  CONTENT_TABLE_H
#define  CONTENT_TABLE_H

#include "glob_data_types.h"
#include <QObject>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QObject>
#include <QString>

class DabRadio;
class QSettings;

class SegmentedTableWidgetItem : public QTableWidgetItem
{
public:
  bool operator<(const QTableWidgetItem & iOther) const override;
};

class FibContentTable : public QObject
{
Q_OBJECT
public:
  FibContentTable(DabRadio * ipDabRadio, QSettings * s, const QString & iChannel, i32 iNumCols);
  ~FibContentTable() override;

  void show() const;
  void hide() const;
  bool is_visible() const;
  void add_line(const QString &);
  void dump(FILE *) const;

private:
  const QString mChannel;
  const i32 mNumColumns;
  DabRadio * const mpDabRadio;
  QScopedPointer<QScrollArea> mpScrollArea;
  QScopedPointer<QTableWidget> mpTableWidget;
  i32 mSegmentId = 0;

  i16 _add_row() const;
  u32 _hex_to_u32(const std::string & iHexString) const;

private slots:
  void _slot_select_service(i32, i32);
  void _slot_dump(i32, i32);

signals:
  void signal_go_service_id(u32);
};

#endif
