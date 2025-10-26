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
#include <QScrollArea>
#include <QListView>
#include <QModelIndex>
#include <QStandardItemModel>

class JournalineScreen : public QObject
{
  Q_OBJECT

public:
  struct STableElement
  {
    i32 key;
    NML::News_t * element;
  };

  JournalineScreen(const std::vector<STableElement> & iTableVec);
  ~JournalineScreen() override;

private:
  const std::vector<STableElement> & mTableVec;
  STableElement mCurrTableElement;
  std::vector<i32> mPathVec;
  QTimer * mpTimerRecMarker;
  bool mDataTransferStarted = false;

  // UI elements
  QFrame mFrame;
  QPushButton * mpBtnHome;
  QPushButton * mpBtnUp;
  QPushButton * mpBtnReload;
  QLabel * mpLblDataReceiving;
  QLabel * mpLblMainText;
  QListView * mpListView;
  QScrollArea * mpScrollArea;
  QLabel * mpLblHtml;
  QStandardItemModel mModel;

  QString _get_journaline_as_HTML() const;
  void _build_html_tree_recursive(const NML::News_t & iElement, QString & ioHtml, i32 iLevel) const;

  void _display_element(const NML::News_t & element);
  void _display_menu(const NML::News_t & element);
  void _display_plain(const NML::News_t & element);
  void _display_list(const NML::News_t & element);

  i32 _find_index(i32) const;
  void _set_receiver_marker_color(bool iReceivingData) const;

public slots:
  void slot_start(i32);
  void slot_new_data();

private slots:
  void _slot_handle_home_button();
  void _slot_handle_up_button();
  void _slot_handle_reload_button();
  void _slot_select_sub(const QModelIndex & iModIdx);
  void _slot_colorize_receive_marker_timeout() const;
};
