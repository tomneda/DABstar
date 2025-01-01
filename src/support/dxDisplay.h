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
#pragma once

#include	<QString>
#include	<QWidget>
#include	<QScrollArea>
#include	<QTableWidget>
#include	<QTableWidgetItem>
#include	<QObject>
#include	<QSettings>

//class	RadioInterface;

class DxDisplay : public QFrame
{
  Q_OBJECT

public:
  DxDisplay(QSettings *);
  ~DxDisplay();
  void set_window_title(const QString &);
  void add_row(const unsigned char, const unsigned char, const float, const float, const bool, const QString &, const QString &);
  void clean_up();
  void show();
  void hide();
  int get_nr_rows();

private:
  QScrollArea * myWidget;
  QTableWidget * tableWidget;
  QSettings * dxSettings;
  void _set_position_and_size(QSettings *, QWidget *, QTableWidget *, const QString &);
  void _store_widget_position(QSettings *, QWidget *, QTableWidget *, const QString &);
};
