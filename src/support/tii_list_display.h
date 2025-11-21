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
#include <QObject>
#include <QScopedPointer>

class CustomScrollArea : public QScrollArea
{
  using QScrollArea::QScrollArea;
  Q_OBJECT

protected:
  void closeEvent(QCloseEvent *event) override;

signals:
  void signal_frame_closed();
};


class TiiListDisplay : public QObject
{
  Q_OBJECT

public:                                                                        
  TiiListDisplay();
  ~TiiListDisplay();

  struct SDerivedData
  {
    f32 strength_dB;
    f32 phase_deg;
    bool  isNonEtsiPhase;
    f32 distance_km;
    f32 corner_deg;
  };

  void set_window_title(const QString &);
  void add_row(const STiiDataEntry & iTr, const SDerivedData & iDD);
  void start_adding();
  void show();
  void hide();
  i32 get_nr_rows();
  void finish_adding();

private:
  static constexpr i32 cColNr = 10;
  QScopedPointer<CustomScrollArea> mpWidget;
  QScopedPointer<QTableWidget> mpTableWidget;

signals:
  void signal_frame_closed();
};
