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
#pragma once

#include "NML.h"
#include "glob_data_types.h"
#include "custom_frame.h"
#include <memory>
#include <QObject>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QMap>

class JournalineViewer : public QObject
{
  Q_OBJECT

public:
  struct STableElement
  {
    std::shared_ptr<NML::News_t> pElement;
    bool isOpened = false;
    bool wasVisited = false;
  };

  using TMapData = QMap<i32, STableElement>;

  JournalineViewer(TMapData & ioTableVec, i32 iSubChannel);
  ~JournalineViewer() override;

private:
  TMapData & mDataMap;
  const i32 mSubChannel;
  STableElement mCurrTableElement;
  QTimer * mpTimerRecMarker;
  QTimer * mpTimerHtmlRebuild;

  // UI elements
  CustomFrame mFrame;
  QLabel * mpLblTitle;
  QLabel * mpLblHtml;
  QLabel * mpLblCopyRight;
  QLabel * mpLblLegends;
  QLabel * mpLblDataReceiving;
  QScrollArea * mpScrollArea;

  QString _get_journaline_as_HTML() const;
  void _build_html_tree_recursive(const TMapData::iterator & iItElem, QString & ioHtml, i32 iLevel, bool iSuppressTitle) const;

  void _set_receiver_marker_color(bool iReceivingData) const;

public slots:
  void slot_new_data();

private slots:
  void _slot_colorize_receive_marker_timeout() const;
  void _slot_html_rebuild_timeout() const;
  void _slot_html_link_activated(const QString & iLink);

signals:
  void signal_window_closed(i32 oSubChannel);
};
