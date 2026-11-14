/*
 * Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
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

#include "glob_defs.h"
#include <QWidget>
#include <QString>
#include <QTimer>
#include <vector>

class DabRadio;
class TileMapWidget;
class QTextBrowser;
struct STiiDataEntry;

// Transmitter map window: shows the home position and the received transmitters on an OpenStreetMap
// map, plus a side bar with details. When the user closes the window, DabRadio::slot_map_terminate()
// is called (queued), which deletes this object.
class MapViewer : public QWidget
{
  Q_OBJECT

public:
  MapViewer(DabRadio* ipDabRadio, const cf32& iHomeLocation);
  ~MapViewer() override;

  void reset();
  void add_transmitter(const STiiDataEntry& iTiiDataEntry, f32 iStrength, f32 iDistance, f32 iAzimuth, bool iNonEtsi);

protected:
  void closeEvent(QCloseEvent* ipEvent) override;

private:
  static constexpr i32 cUpdateIntervalMs     = 2000;
  static constexpr f32 cHomeInFrameMaxDistKm = 200.0f;
  static constexpr i32 cFramePaddingPx       = 20;
  static constexpr i32 cFrameMaxZoom         = 13;

  struct STransmitter
  {
    f32 latitude;
    f32 longitude;
    QString transmitterName;
    QString channelName;
    QString polarization;
    QString direction;
    u8 mainId;
    u8 subId;
    f32 strength;
    f32 distance;
    f32 azimuth;
    f32 power;
    i32 altitude;
    i32 height;
    bool nonEtsi;
  };

  const cf32 mHomeLocation;
  TileMapWidget* mpMap = nullptr;
  QTextBrowser* mpSideBar = nullptr;
  QTimer mUpdateTimer;

  std::vector<STransmitter> mPendingTransmitters;   // collected since last update
  std::vector<STransmitter> mShownTransmitters;
  QString mSelectedText;
  usize mNrTransmittersFramed = 0;
  bool mSuppressAutoFrame = false;
  bool mTerminating = false;

  static QString _get_transmitter_text(const STransmitter& iTr);
  static QColor _get_marker_color(f32 iStrength);
  void _update_side_bar() const;
  void _auto_frame();

private slots:
  void _slot_update_timeout();
  void _slot_marker_clicked(i32 iIdx);

signals:
  void signal_terminating();
};
