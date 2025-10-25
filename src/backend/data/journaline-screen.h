/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2017 .. 2024
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
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#pragma once

#include "NML.h"
#include "glob_data_types.h"
#include <QObject>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QListView>
#include <QModelIndex>
#include <QStandardItemModel>

struct STableElement
{
  i32 key;
  NML::News_t * element;
};

class JournalineScreen : public QObject
{
  Q_OBJECT

public:
  JournalineScreen(const std::vector<STableElement> & iTableVec);
  ~JournalineScreen() override;

private:
  const std::vector<STableElement> & mTableVec;
  STableElement mCurrTableElement;
  std::vector<i32> mPathVec;

  QFrame mFrame;
  QPushButton * mpBtnReset;
  QPushButton * mpBtnUp;
  QLabel * mpLblMainText;
  QListView * mpListView;
  QStandardItemModel mModel;

  void _display_element(const NML::News_t & element);
  void _display_menu(const NML::News_t & element);
  void _display_plain(const NML::News_t & element);
  void _display_list(const NML::News_t & element);

  i32 _find_index(i32) const;

public slots:
  void slot_start(i32);

private slots:
  void _slot_handle_reset_button();
  void _slot_handle_up_button();
  void _slot_select_sub(const QModelIndex & ind);
};
