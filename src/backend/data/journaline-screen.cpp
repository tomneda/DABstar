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

#include "journaline-screen.h"
#include "setting-helper.h"
#include <QHBoxLayout>
#include <QVBoxLayout>

JournalineScreen::JournalineScreen(const std::vector<STableElement> & iTableVec)
  : mTableVec(iTableVec)
{
  Settings::Journaline::posAndSize.read_widget_geometry(&mFrame);

  // Create UI elements (need no delete)
  mpBtnReset = new QPushButton("Reset", &mFrame);
  mpBtnUp = new QPushButton("Up", &mFrame);
  mpLblMainText = new QLabel("", &mFrame);
  mpListView = new QListView(&mFrame);
  QHBoxLayout * LH = new QHBoxLayout();
  QVBoxLayout * LV = new QVBoxLayout(&mFrame);

  LH->addWidget(mpBtnReset);
  LH->addWidget(mpBtnUp);
  LV->addLayout(LH);
  LV->addWidget(mpLblMainText);
  LV->addWidget(mpListView);
  mFrame.setLayout(LV);

  mFrame.setWindowTitle("Journaline");

  mpListView->setToolTip("Features NewsService Journaline(R) decoder technology by\n"
                         "Fraunhofer IIS, Erlangen, Germany.\n"
                         "For more information visit http://www.iis.fhg.de/dab");

  connect(mpBtnReset, &QPushButton::clicked, this, &JournalineScreen::_slot_handle_reset_button);
  connect(mpBtnUp, &QPushButton::clicked, this, &JournalineScreen::_slot_handle_up_button);
  connect(mpListView, &QListView::clicked, this, &JournalineScreen::_slot_select_sub);

  mFrame.show();
}

JournalineScreen::~JournalineScreen()
{
  Settings::Journaline::posAndSize.write_widget_geometry(&mFrame);
  mFrame.hide();
}

void JournalineScreen::_slot_handle_reset_button()
{
  mPathVec.clear();
  for (u32 i = 0; i < mTableVec.size(); i++)
  {
    if (mTableVec[i].key == 0)
    {
      _display_element(*(mTableVec[i].element));
      mPathVec.push_back(0);
      return;
    }
  }
}

void JournalineScreen::_slot_handle_up_button()
{
  if (mPathVec.size() < 2)
    return;
  mPathVec.pop_back();
  i32 index = _find_index(mPathVec.back());
  if (index < 0)
    return;
  _display_element(*(mTableVec[index].element));
}

void JournalineScreen::_slot_select_sub(const QModelIndex & ind)
{
  // first, identify the current element
  const i32 t = mPathVec.back();
  const i32 currentIndex = _find_index(t);

  if (currentIndex < 0)
  {
    return;
  }

  const NML::News_t * const currentElement = mTableVec[currentIndex].element;

  // for sure, the PLAIN element does not have siblings
  if (currentElement->object_type == NML::PLAIN)
  {
    return;
  }

  const NML::Item_t & item = currentElement->item[ind.row()];

  if (true)
  {
    // if (item.link_id_available)
    const i32 idx = _find_index(item.link_id);
    if (idx < 0)
    {
      fprintf (stderr, "Link %d not found\n", item. link_id);
      return;
    }
    const NML::News_t * const target = mTableVec[idx].element;
    _display_element(*target);
    mPathVec.push_back(item.link_id);
  }
}

void JournalineScreen::_display_element(const NML::News_t & element)
{
  switch (element.object_type)
  {
  case NML::MENU:
    _display_menu(element);
    break;
  case NML::PLAIN:
    _display_plain(element);
    break;
  case NML::LIST:
    _display_list(element);
    break;
  default:
    qDebug() << "Unknown object type" << element.object_type;
    break;
  }
}

void JournalineScreen::_display_menu(const NML::News_t & element)
{
  const std::string t = element.title;
  mpLblMainText->setText(QString::fromStdString(t));
  mModel.clear();

  for (u32 i = 0; i < element.item.size(); i++)
  {
    const NML::Item_t & item = element.item[i];
    mModel.appendRow(new QStandardItem(QString::fromStdString(item.text)));
  }

  mpListView->setModel(&mModel);
}

void JournalineScreen::_display_plain(const NML::News_t & element)
{
  const std::string t = element.title;
  mpLblMainText->setText(QString::fromStdString(t));
  mModel.clear();
  const NML::Item_t & item = element.item[0];
  mModel.appendRow(new QStandardItem(QString::fromStdString(item.text)));
}

void JournalineScreen::_display_list(const NML::News_t & element)
{
  const std::string t = element.title;
  mpLblMainText->setText(QString::fromStdString(t));
  mModel.clear();

  for (u32 i = 0; i < element.item.size(); i++)
  {
    const NML::Item_t * const item = &(element.item[i]);
    mModel.appendRow(new QStandardItem(QString::fromStdString(item->text)));
  }

  mpListView->setModel(&mModel);
}

i32 JournalineScreen::_find_index(const i32 key) const
{
  for (u32 i = 0; i < mTableVec.size(); i++)
  {
    if (mTableVec[i].key == key)
    {
      return i;
    }
  }
  return -1;
}

void JournalineScreen::slot_start(const i32 index)
{
  mPathVec.push_back(0);
  _display_element(*(mTableVec[index].element));
}
