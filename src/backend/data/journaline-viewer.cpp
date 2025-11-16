/*
 * Copyright (c) 2025 by Thomas Neder (https://github.com/tomneda)
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "journaline-viewer.h"
#include "setting-helper.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>

static const QString cColorHeader    = "#87CEFA";  // header elements
static const QString cColorNotLoaded = "#909090";  // data not loaded yet
static const QString cColorNotOpened = "#80FF80";  // loaded but not opened
static const QString cColorVisiting  = "#FFFF00";  // opened and actual visiting
static const QString cColorVisited   = "#FF6060";  // closed but visited before
static const QString cColorText      = "#FFFFFF";  // information text

JournalineViewer::JournalineViewer(TMapData & ioTableVec, const i32 iSubChannel)
  : mDataMap(ioTableVec)
  , mSubChannel(iSubChannel)
{
  Settings::Journaline::posAndSize.read_widget_geometry(&mFrame);

  mFrame.setWindowTitle("DABstar Journaline");
  mFrame.setWindowIcon(QIcon(":res/logo/dabstar_j.png"));

  const QString copyRightStr = "<span style=\"color: #FFD700; font-size: small;\">"
                               "Features NewsService Journaline(R) decoder technology by<br>"
                               "Fraunhofer IIS, Erlangen, Germany.<br>"
                               "For more information visit "
                               "<a href=\"http://www.iis.fhg.de/dab\" style=\"color: #87CEFA;\">http://www.iis.fhg.de/dab</a>"
                               "</span>";
  
  const QString legendStr = "<table style=\"border-collapse: collapse; font-size: small;\">"
                            "<tr>"
                            "<td colspan=\"2\" style=\"padding-bottom: 5px;\"><span style=\"color: lightgray;\">Menu legend:</span></td>"
                            "</tr>"
                            "<tr>"
                            "<td><span style=\"color: " + cColorNotLoaded + ";\">&nbsp;Data not loaded yet&nbsp;</span></td>"
                            "<td><span style=\"color: " + cColorNotOpened + ";\">&nbsp;Data loaded but element not opened yet&nbsp;</span></td>"
                            "</tr>"
                            "<tr>"
                            "<td><span style=\"color: " + cColorVisiting  + ";\">&nbsp;Element opened&nbsp;</span></td>"
                            "<td><span style=\"color: " + cColorVisited   + ";\">&nbsp;Element closed but visited before&nbsp;</span></td>"
                            "</tr>"
                            "</table>";
  
  mpTimerRecMarker = new QTimer(this);
  mpTimerRecMarker->setSingleShot(true);
  mpTimerRecMarker->setInterval(100);

  mpTimerHtmlRebuild = new QTimer(this);
  mpTimerHtmlRebuild->setSingleShot(true);
  mpTimerHtmlRebuild->setInterval(5000);

  mpLblCopyRight = new QLabel(copyRightStr, &mFrame);
  mpLblCopyRight->setStyleSheet("background-color: #351515; border-radius: 8px; padding-left: 8px; padding-right: 8px;");
  mpLblCopyRight->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
  mpLblCopyRight->setOpenExternalLinks(true);

  mpLblLegends = new QLabel(legendStr, &mFrame);
  mpLblLegends->setAlignment(Qt::AlignCenter);
  mpLblLegends->setStyleSheet("background-color: #151535; border-radius: 8px; padding-left: 8px; padding-right: 8px;");

  mpLblDataReceiving = new QLabel(&mFrame);
  mpLblDataReceiving->setAlignment(Qt::AlignCenter);

  mpLblTitle = new QLabel("<h1 style=\"color: lightgray;\">Waiting...</h1>", &mFrame);
  mpLblTitle->setAlignment(Qt::AlignLeft);

  mpLblHtml = new QLabel(&mFrame);
  mpLblHtml->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  mpLblHtml->setWordWrap(true);
  mpLblHtml->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextBrowserInteraction | Qt::TextBrowserInteraction);
  mpLblHtml->setOpenExternalLinks(false);

  mpScrollArea = new QScrollArea(&mFrame);
  mpScrollArea->setWidget(mpLblHtml);
  mpScrollArea->setWidgetResizable(true);

  QHBoxLayout * pHBoxLayout = new QHBoxLayout(); // there is no memory leakage here (as clang tidy is thinking)
  pHBoxLayout->addWidget(mpLblCopyRight);
  pHBoxLayout->addStretch();
  pHBoxLayout->addWidget(mpLblLegends);
  pHBoxLayout->addStretch();
  pHBoxLayout->addWidget(mpLblDataReceiving);

  QVBoxLayout * pVBoxLayout = new QVBoxLayout(&mFrame);  // there is no memory leakage here (as clang tidy is thinking)
  pVBoxLayout->addWidget(mpLblTitle, 0);
  pVBoxLayout->addWidget(mpScrollArea, 2);
  pVBoxLayout->addLayout(pHBoxLayout, 0);

  mFrame.setLayout(pVBoxLayout);

  _set_receiver_marker_color(false);

  connect(mpLblHtml, &QLabel::linkActivated, this, &JournalineViewer::_slot_html_link_activated);
  connect(mpTimerRecMarker, &QTimer::timeout, this, &JournalineViewer::_slot_colorize_receive_marker_timeout);
  connect(mpTimerHtmlRebuild, &QTimer::timeout, this, &JournalineViewer::_slot_html_rebuild_timeout);
  connect(&mFrame, &CustomFrame::signal_frame_closed, [this](){ JournalineViewer::signal_window_closed(mSubChannel); });

  mFrame.show();
}

JournalineViewer::~JournalineViewer()
{
  Settings::Journaline::posAndSize.write_widget_geometry(&mFrame);
  mFrame.hide();
}

void JournalineViewer::slot_new_data()
{
  _set_receiver_marker_color(true);
  mpTimerRecMarker->start();

  if (!mpTimerHtmlRebuild->isActive()) // only restart time if it was not running yet
  {
    mpTimerHtmlRebuild->start();
  }
}

void JournalineViewer::_slot_colorize_receive_marker_timeout() const
{
  _set_receiver_marker_color(false);
}

void JournalineViewer::_slot_html_rebuild_timeout() const
{
  if (mDataMap.contains(0))
  {
    mpLblHtml->setText(_get_journaline_as_HTML());
  }
}

void JournalineViewer::_slot_html_link_activated(const QString & iLink)
{
  // Expecting links like "open:<id>"
  if (!iLink.startsWith("open:"))
  {
    qCritical() << "Unexpected link" << iLink;
    return;
  }

  const i32 linkId = iLink.mid(5).toInt();

  const auto it = mDataMap.find(linkId);

  // Toggle open state
  if (it != mDataMap.end())
  {
    it.value().isOpened = !it.value().isOpened;
    it.value().wasVisited = true;
  }
  
  // Refresh HTML view
  if (mDataMap.contains(0))
  {
    mpLblHtml->setText(_get_journaline_as_HTML());
  }
}

void JournalineViewer::_build_html_tree_recursive(const TMapData::iterator & iItElem, QString & ioHtml, const i32 iLevel, const bool iSuppressTitle) const
{
  const auto & pElem = iItElem.value().pElement;
  const i32 captionIdx = std::min(iLevel + 1, 6);
  const QString capTagBeg = "<h" + QString::number(captionIdx) + " style=\"color: " + cColorHeader + "; margin-left: -20px;\">";
  const QString capTagEnd = "</h" + QString::number(captionIdx) + ">";

  if (iLevel == 0)
  {
    mpLblTitle->setText("<h1 style=\"color: white;\">" + QString::fromUtf8(pElem->title) + "</h1>");
  }
  else if (!iSuppressTitle)
  {
    ioHtml += capTagBeg + QString::fromUtf8(pElem->title) + capTagEnd;
  }

  switch (pElem->object_type)
  {
  case NML::MENU:
    ioHtml += "<ul style=\"margin-left: -20px;\">";
    for (const auto & item : pElem->item)
    {
      const i32 linkId = item.link_id;
      const auto itSubMenu = mDataMap.find(linkId);
      const bool isLoaded = (itSubMenu != mDataMap.end());
      const bool isOpened = itSubMenu.value().isOpened;
      const bool wasVisited = itSubMenu.value().wasVisited;

      if (isLoaded)
      {
        const QString & color = wasVisited ? (isOpened ? cColorVisiting : cColorVisited) : cColorNotOpened;
        ioHtml += "<li><a href=\"open:" + QString::number(linkId) + "\" style=\"color: " + color + "; text-decoration:none;\">" + QString::fromUtf8(item.text) + "</a>";
      }
      else
      {
        ioHtml += "<li><span style=\"color: " + cColorNotLoaded + "; text-decoration:none;\">" + QString::fromUtf8(item.text) + "</span>";
      }

      if (isLoaded && isOpened)
      {
        const bool isMenuTextSameLikeTitle = (item.text == itSubMenu->pElement->title);
        ioHtml += "<ul style=\"list-style-type: none; margin-left: -20px;\"><li>";
        _build_html_tree_recursive(itSubMenu, ioHtml, iLevel + 1, isMenuTextSameLikeTitle);
        ioHtml += "</li></ul>";
      }
      ioHtml += "</li>";
    }
    ioHtml += "</ul>";
    break;

  case NML::PLAIN:
    if (!pElem->item.empty())
    {
      ioHtml += "<p style=\"color: " + cColorText + "; margin-left: -10px;\">" + QString::fromUtf8(pElem->item[0].text) + "</p>";
    }
    break;

  case NML::LIST:
    ioHtml += "<ul style=\"color: " + cColorText + "; margin-left: -20px;\">";
    for (const auto & item : pElem->item)
    {
      ioHtml += "<li>" + QString::fromUtf8(item.text) + "</li>";
    }
    ioHtml += "</ul>";
    break;
  default:
    break;
  }
}

QString JournalineViewer::_get_journaline_as_HTML() const
{
  const auto itDataStart = mDataMap.find(0);

  if (itDataStart == mDataMap.end())
  {
    return "";
  }

  QString html = "<html><body>";
  _build_html_tree_recursive(itDataStart, html, 0, false);
  html += "</body></html>";
  return html;
}

void JournalineViewer::_set_receiver_marker_color(bool iReceivingData) const
{
  if (iReceivingData)
  {
    mpLblDataReceiving->setText("<span style=\"font-size: small;\">&nbsp;&nbsp;Receiving data&nbsp;&nbsp;<br>&nbsp;&nbsp;" + QString::number(mDataMap.size()) + " elements&nbsp;&nbsp;</span>");
    mpLblDataReceiving->setStyleSheet("background-color: #C85C3C; color: yellow; border-radius: 12px;");
  }
  else
  {
    mpLblDataReceiving->setStyleSheet("background-color: #222222; color: darkgray; border-radius: 12px;");
  }
}
