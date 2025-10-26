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
  mpBtnReset = new QPushButton("Reset / Reload", &mFrame);
  mpBtnUp = new QPushButton("Up", &mFrame);
  mpLblMainText = new QLabel("", &mFrame);
  mpLblHtml = new QLabel(&mFrame);
  mpLblHtml->setWordWrap(true);
  mpScrollArea = new QScrollArea(&mFrame);
  mpScrollArea->setWidget(mpLblHtml);
  mpScrollArea->setWidgetResizable(true); // Very important!
  mpListView = new QListView(&mFrame);

  QHBoxLayout * LH = new QHBoxLayout();
  QVBoxLayout * LV = new QVBoxLayout(&mFrame);

  LH->addWidget(mpBtnReset);
  LH->addWidget(mpBtnUp);
  LV->addLayout(LH);
  LV->addWidget(mpLblMainText);
  LV->addWidget(mpListView);
  LV->addWidget(mpScrollArea);
  mFrame.setLayout(LV);

  mFrame.setWindowTitle("Journaline");

  mpListView->setToolTip("Features NewsService Journaline(R) decoder technology by\n"
                         "Fraunhofer IIS, Erlangen, Germany.\n"
                         "For more information visit http://www.iis.fhg.de/dab");

  mpLblHtml->setTextInteractionFlags(Qt::TextSelectableByMouse);
  mpLblHtml->setTextInteractionFlags(Qt::TextSelectableByKeyboard);
  mpLblHtml->setOpenExternalLinks(true);

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

  const i32 startIdx = _find_index(0);
  if (startIdx >= 0)
  {
    for (int32_t i = 0; i < 5; i++) qDebug();
    assert(startIdx >= 0);
    // _print_element(*(mTableVec[startIdx].element), 0);
    const QString html = _get_journaline_as_HTML();
    mpLblHtml->setText(html);
    qDebug() << html;
  }



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

void JournalineScreen::_slot_select_sub(const QModelIndex & iModIdx)
{
  // first, identify the current element
  const i32 key = mPathVec.back();
  const i32 currIdx = _find_index(key);

  if (currIdx < 0)
  {
    return;
  }

  _print_debug_data("_slot_select_sub");
  qDebug() << "Key" << key << "CurrIdx" << currIdx;

  const NML::News_t * const pCurrElem = mTableVec[currIdx].element;

  // for sure, the PLAIN element does not have siblings
  if (pCurrElem->object_type == NML::PLAIN)
  {
    return;
  }

  const NML::Item_t & item = pCurrElem->item[iModIdx.row()];

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
    // qDebug() << "Display menu";
    _display_menu(element);
    break;
  case NML::PLAIN:
    // qDebug() << "Display plain";
    _display_plain(element);
    break;
  case NML::LIST:
    // qDebug() << "Display list";
    _display_list(element);
    break;
  default:
    qDebug() << "Unknown object type" << element.object_type;
    break;
  }
}

void JournalineScreen::_display_menu(const NML::News_t & element)
{
  // _print_debug_data("_display_menu");
  const std::string t = element.title;
  mpLblMainText->setText(QString::fromStdString(t));
  mModel.clear();

  for (u32 i = 0; i < element.item.size(); i++)
  {
    const NML::Item_t & item = element.item[i];
    qDebug() << "Display menu item" << i << item.text << item.link_id_available << item.link_id;
    mModel.appendRow(new QStandardItem(QString::fromStdString(item.text)));
  }
  mpListView->setModel(&mModel);
}

void JournalineScreen::_display_plain(const NML::News_t & element)
{
  // _print_debug_data("_display_plain");
  const std::string t = element.title;
  mpLblMainText->setText(QString::fromStdString(t));
  mModel.clear();
  const NML::Item_t & item = element.item[0];
  qDebug() << "Display plain item" << item.text << item.link_id_available << item.link_id;
  mModel.appendRow(new QStandardItem(QString::fromStdString(item.text)));
  mpListView->setModel(&mModel);
}

void JournalineScreen::_display_list(const NML::News_t & element)
{
  // _print_debug_data("_display_list");
  const std::string t = element.title;
  mpLblMainText->setText(QString::fromStdString(t));
  mModel.clear();

  for (u32 i = 0; i < element.item.size(); i++)
  {
    const NML::Item_t & item = element.item[i];
    qDebug() << "Display list item" << i << item.text << item.link_id_available << item.link_id;
    mModel.appendRow(new QStandardItem(QString::fromStdString(item.text)));
  }
  mpListView->setModel(&mModel);
}

QString JournalineScreen::_ident_str(const u32 iLevel) const
{
  if (iLevel == 0) return "";

  QString s("|-");
  s.reserve(iLevel * 4);
  for (u32 i = 0; i < iLevel-1; i++) s += "----";
  return s + "->";
}

void JournalineScreen::_print_element(const NML::News_t & element, const u32 iLevel) const
{
  if (iLevel == 0)
  {
    qDebug() << "Title:" << element.title;
  }

  switch (element.object_type)
  {
  case NML::MENU:
    _print_menu(element, iLevel);
    break;
  case NML::PLAIN:
    _print_plain(element, iLevel);
    break;
  case NML::LIST:
    _print_list(element, iLevel);
    break;
  default:
    qDebug() << "Unknown object type" << element.object_type;
    break;
  }
}


void JournalineScreen::_print_menu(const NML::News_t & element, const u32 iLevel) const
{
  const QString ident = _ident_str(iLevel);
  for (u32 i = 0; i < element.item.size(); i++)
  {
    const NML::Item_t & item = element.item[i];
    qDebug() << ident.toStdString().c_str() << "MenuItem" << i << QString::fromUtf8(item.text);
    const i32 subMenuIdx = _find_index(item.link_id);
    if (subMenuIdx >= 0)
    {
      _print_element(*mTableVec[subMenuIdx].element, iLevel + 1);
    }
  }
}

void JournalineScreen::_print_plain(const NML::News_t & element, const u32 iLevel) const
{
  const QString ident = _ident_str(iLevel);
  const NML::Item_t & item = element.item[0];
  qDebug() << ident.toStdString().c_str() << "PlainItem" << QString::fromUtf8(item.text);
}

void JournalineScreen::_print_list(const NML::News_t & element, const u32 iLevel) const
{
  const QString ident = _ident_str(iLevel);
  for (u32 i = 0; i < element.item.size(); i++)
  {
    const NML::Item_t & item = element.item[i];
    qDebug() << ident.toStdString().c_str() << "ListItem" << i << QString::fromUtf8(item.text);
  }
}

void JournalineScreen::_build_html_tree_recursive(const NML::News_t & iElement, QString & ioHtml, const i32 iLevel) const
{
  const QString captionTag = "h" + QString::number(std::min(iLevel + 1, 6));
  ioHtml += "<" + captionTag  + ">" + QString::fromUtf8(iElement.title) + "</"  + captionTag + ">";

  switch (iElement.object_type)
  {
  case NML::MENU:
    ioHtml += "<ul>";
    for (const auto & item : iElement.item)
    {
      ioHtml += "<li>" + QString::fromUtf8(item.text);
      const i32 subMenuIdx = _find_index(item.link_id);
      if (subMenuIdx >= 0)
      {
        ioHtml += "<ul><li>";
        _build_html_tree_recursive(*mTableVec[subMenuIdx].element, ioHtml, iLevel + 1);
        ioHtml += "</li></ul>";
      }
      ioHtml += "</li>";
    }
    ioHtml += "</ul>";
    break;
  case NML::PLAIN:
    if (!iElement.item.empty())
    {
      ioHtml += "<p>" + QString::fromUtf8(iElement.item[0].text) + "</p>";
    }
    break;
  case NML::LIST:
    ioHtml += "<ul>";
    for (const auto & item : iElement.item)
    {
      ioHtml += "<li>" + QString::fromUtf8(item.text) + "</li>";
    }
    ioHtml += "</ul>";
    break;
  default:
    break;
  }
}

QString JournalineScreen::_get_journaline_as_HTML() const
{
  const i32 startIdx = _find_index(0);
  if (startIdx < 0)
  {
    return "";
  }

  QString html = "<html><body>";
  _build_html_tree_recursive(*(mTableVec[startIdx].element), html, 0);
  html += "</body></html>";
  return html;
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
  qDebug() << "Start" << index;
  _display_element(*(mTableVec[index].element));
  // _print_element(*(mTableVec[index].element), 0);
}

void JournalineScreen::_print_debug_data(const QString & iTitle)
{
  // qInfo() << "Title" << iTitle << "PathVec size" << mPathVec.size() << "Content" << mPathVec;
  //
  // std::vector<i32> keys;
  // keys.reserve(mTableVec.size());
  // for (auto & [key,  pElem]: mTableVec)
  // {
  //   keys.push_back(key);
  // }
  // qDebug() << "TableVec size" << mTableVec.size() << "keys" << keys;
  //
  // // for (auto & [key,  pElem]: mTableVec)
  // // {
  // //   qDebug() << "key" << key << "ObjType" << pElem->object_type << "title" << pElem->title << "itemsize" << pElem->item.size();
  // //   for (i32 elemIdx = 0; elemIdx < std::min((i32)pElem->item.size(), 2) ; elemIdx++)
  // //   {
  // //     const NML::Item_t & item = pElem->item[elemIdx];
  // //     qDebug() << "  --> elemIdx" << elemIdx << "text" << item.text << item.link_id_available << item.link_id;
  // //   }
  // // }
  //
  // const i32 startIdx = _find_index(0);
  // if (startIdx < 0)
  // {
  //   qDebug() << "StartIdx not found";
  //   return;
  // }
  // const NML::News_t * const pStartElem = mTableVec[startIdx].element;
  // qDebug() << "StartIdx" << startIdx << "ObjType" << pStartElem->object_type << "title" << pStartElem->title << "itemsize" << pStartElem->item.size();
  //
  //
  // for (i32 elemIdx = 0; elemIdx < std::min((i32)pStartElem->item.size(), 2) ; elemIdx++)
  // {
  //   const NML::Item_t & item = pStartElem->item[elemIdx];
  // }
}

