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
#pragma once

#include "tii-codes.h"
#include <QString>
#include <QWidget>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QObject>
#include <QSettings>
#include <QScopedPointer>


class TiiListDisplay : public QFrame
{
  Q_OBJECT

public:                                                                        
  TiiListDisplay(QSettings *);
  ~TiiListDisplay();

  struct SDerivedData
  {
    float strength_dB;
    float phase_deg;
    bool  isNonEtsiPhase;
    float distance_km;
    float corner_deg;
  };

  void set_window_title(const QString &);
  void add_row(const SCacheElem & iTr, const SDerivedData & iDD);
  void start_adding();
  void show();
  void hide();
  int get_nr_rows();
  void finish_adding();

private:
  static constexpr int32_t cColNr = 10;
  QScopedPointer<QScrollArea> mpWidget;
  QScopedPointer<QTableWidget> mpTableWidget;
  QSettings * mpSettings;
  
  void _set_position_and_size(QSettings * settings, QScopedPointer<QScrollArea> & w, QScopedPointer<QTableWidget> & t, const QString & key);
  void _store_widget_position(QSettings * settings, QScopedPointer<QScrollArea> & w, QScopedPointer<QTableWidget> & t, const QString & key);
};
