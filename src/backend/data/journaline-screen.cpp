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
#include <QTimer>

JournalineScreen::JournalineScreen(const std::vector<STableElement> & iTableVec)
  : mTableVec(iTableVec)
{
  Settings::Journaline::posAndSize.read_widget_geometry(&mFrame);
  // mFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon, but has also no minimize button then, bad for such big window

  mpTimerRecMarker = new QTimer(this);
  mpTimerRecMarker->setSingleShot(true);
  mpTimerRecMarker->setInterval(100);

  // Create UI elements (need no delete)
  mpBtnHome = new QPushButton("Home", &mFrame);
  mpBtnUp = new QPushButton("Up", &mFrame);
  mpBtnReload = new QPushButton("Reload", &mFrame);
  mpLblDataReceiving = new QLabel("  Data receiving  ", &mFrame);
  mpLblDataReceiving->setAlignment(Qt::AlignCenter);
  mpLblMainText = new QLabel("", &mFrame);
  mpLblHtml = new QLabel(&mFrame);
  mpLblHtml->setWordWrap(true);
  mpScrollArea = new QScrollArea(&mFrame);
  mpScrollArea->setWidget(mpLblHtml);
  mpScrollArea->setWidgetResizable(true); // Very important!
  mpListView = new QListView(&mFrame);

  QHBoxLayout * LH = new QHBoxLayout();
  QVBoxLayout * LV = new QVBoxLayout(&mFrame);

  LH->addWidget(mpBtnHome);
  LH->addWidget(mpBtnUp);
  LH->addStretch();
  LH->addWidget(mpLblDataReceiving); 
  LH->addStretch();
  LH->addWidget(mpBtnReload);
  LV->addWidget(mpLblMainText, 0);
  LV->addWidget(mpListView, 0);
  LV->addLayout(LH, 0);
  LV->addWidget(mpScrollArea, 2);
  mFrame.setLayout(LV);

  mpBtnHome->setEnabled(false);
  mpBtnUp->setEnabled(false);
  mpBtnReload->setEnabled(false);
  _set_receiver_marker_color(false);

  mFrame.setWindowTitle("Journaline");

  mpListView->setToolTip("Features NewsService Journaline(R) decoder technology by\n"
                         "Fraunhofer IIS, Erlangen, Germany.\n"
                         "For more information visit http://www.iis.fhg.de/dab");

  mpLblHtml->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextBrowserInteraction | Qt::TextBrowserInteraction);
  mpLblHtml->setOpenExternalLinks(true);

  connect(mpBtnHome, &QPushButton::clicked, this, &JournalineScreen::_slot_handle_home_button);
  connect(mpBtnUp, &QPushButton::clicked, this, &JournalineScreen::_slot_handle_up_button);
  connect(mpBtnReload, &QPushButton::clicked, this, &JournalineScreen::_slot_handle_reload_button);
  connect(mpListView, &QListView::clicked, this, &JournalineScreen::_slot_select_sub);
  connect(mpTimerRecMarker, &QTimer::timeout, this, &JournalineScreen::_slot_colorize_receive_marker_timeout);

  mFrame.show();
}

JournalineScreen::~JournalineScreen()
{
  Settings::Journaline::posAndSize.write_widget_geometry(&mFrame);
  mFrame.hide();
}

void JournalineScreen::slot_start(const i32 index)
{
  mPathVec.push_back(0);
  _display_element(*(mTableVec[index].element));
  _slot_handle_reload_button(); // loads the first data elements to screen
  mDataTransferStarted = true;

  mpBtnHome->setEnabled(true);
  mpBtnUp->setEnabled(true);
  mpBtnReload->setEnabled(true);
}

void JournalineScreen::slot_new_data()
{
  _set_receiver_marker_color(true);
  mpTimerRecMarker->start();
  if (mDataTransferStarted)
  {
    mpBtnReload->setEnabled(true);
  }
}

void JournalineScreen::_slot_handle_home_button()
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

void JournalineScreen::_slot_handle_reload_button()
{
  mpBtnReload->setEnabled(false);

  const i32 startIdx = _find_index(0);
  if (startIdx >= 0)
  {
    mpLblHtml->setText(_get_journaline_as_HTML());
  }
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

void JournalineScreen::_slot_colorize_receive_marker_timeout() const
{
  _set_receiver_marker_color(false);
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
    qWarning() << "Unhandled object type" << element.object_type;
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
    mModel.appendRow(new QStandardItem(QString::fromStdString(item.text)));
  }
  mpListView->setModel(&mModel);
}

void JournalineScreen::_build_html_tree_recursive(const NML::News_t & iElement, QString & ioHtml, const i32 iLevel) const
{
  static const QString colorList[6] = { "#E0E0E0", "#87CEFA ", "#90EE90", "#FFD700", "#F08080", "#B0C4DE" };
  const i32 captionIdx = std::min(iLevel + 1, 6);
  const QString capTagBeg = "<h" + QString::number(captionIdx) + " style=\"color: " + colorList[captionIdx - 1] + ";\">";
  const QString capTagEnd = "</h" + QString::number(captionIdx) + ">";
  ioHtml += capTagBeg + QString::fromUtf8(iElement.title) + capTagEnd;

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

void JournalineScreen::_set_receiver_marker_color(bool iReceivingData) const
{
  if (iReceivingData)
  {
    mpLblDataReceiving->setStyleSheet("background-color: #C85C3C; color: white;");
  }
  else
  {
    mpLblDataReceiving->setStyleSheet("background-color: #222222; color: darkgray;");
  }
}
