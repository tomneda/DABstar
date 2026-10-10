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

#include "tii_codes.h"
#include <QString>
#include <QWidget>
#include <QTableWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QObject>
#include <QScopedPointer>
#include <vector>

class TiiListDisplay : public QObject
{
  Q_OBJECT

public:
  TiiListDisplay();
  ~TiiListDisplay() override;

  struct SDerivedData
  {
    f32  strength_dB;
    f32  phase_deg;
    bool isNonEtsiPhase;
    f32  distance_km;
    f32  corner_deg;
  };

  struct STableRowEntry
  {
    STiiDataEntry tr;
    SDerivedData  dd;
    QString       dateTime;
    bool          isCurrent  = true;
    bool          hasDbEntry = true;
  };

  void set_window_title(const QString &);
  void set_channel(const QString & iChannel) { mCurChannel = iChannel; }
  void add_row(const STiiDataEntry & iTr, const SDerivedData & iDD, bool iHasDbEntry = true);
  void start_adding();
  void show() const;
  void hide() const;
  void setVisible(bool iVisible) const { if (iVisible) show(); else hide(); }
  [[nodiscard]] QWidget * get_widget() const { return mpWidget.get(); }
  i32 get_nr_rows() const;
  void finish_adding();
  void clear();

private slots:
  void _slot_keep_persistent_changed(i32 iState);
  void _slot_ignore_no_db_changed(i32 iState);
  void _slot_save_list();
  void _slot_open_folder();

private:
  void _update_table();

  static constexpr i32 cColNr = 10;
  QScopedPointer<QWidget> mpWidget;
  QTableWidget * mpTableWidget = nullptr;
  QCheckBox    * mpCbKeepPersistent = nullptr;
  QCheckBox    * mpCbIgnoreNoDb = nullptr;
  QPushButton  * mpBtnSaveList = nullptr;
  QPushButton  * mpBtnOpenFolder = nullptr;

  std::vector<STableRowEntry> mEntries;
  std::vector<STableRowEntry> mCurrentScanEntries;
  QString mLastSavedFilePath;
  QString mCurChannel;
};
