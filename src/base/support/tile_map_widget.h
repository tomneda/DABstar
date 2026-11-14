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

#include "glob_data_types.h"
#include "map_projection.h"
#include <QWidget>
#include <QCache>
#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QSet>
#include <QString>
#include <vector>

class QNetworkAccessManager;
class QNetworkReply;
class QToolButton;

// Simple slippy map widget showing OpenStreetMap tiles with some text markers on top.
// Tiles are cached in memory and on disk (respecting the HTTP cache headers of the tile server).
class TileMapWidget : public QWidget
{
  Q_OBJECT

public:
  struct SMarker
  {
    MapProjection::SGeoPos pos;
    QString label;
    QString toolTip;
    QColor color;
  };

  explicit TileMapWidget(QWidget* ipParent = nullptr);
  ~TileMapWidget() override = default;

  void set_home(const MapProjection::SGeoPos& iHome);
  void set_markers(const std::vector<SMarker>& iMarkers);
  void set_view(const MapProjection::SGeoPos& iCenter, i32 iZoom);
  void fit_to(const std::vector<MapProjection::SGeoPos>& iPositions, i32 iPadding, i32 iMaxZoom);

signals:
  void signal_marker_clicked(i32 oMarkerIdx);
  void signal_view_changed_by_user();

protected:
  void paintEvent(QPaintEvent* ipEvent) override;
  void mousePressEvent(QMouseEvent* ipEvent) override;
  void mouseMoveEvent(QMouseEvent* ipEvent) override;
  void mouseReleaseEvent(QMouseEvent* ipEvent) override;
  void mouseDoubleClickEvent(QMouseEvent* ipEvent) override;
  void wheelEvent(QWheelEvent* ipEvent) override;
  bool event(QEvent* ipEvent) override;

private:
  static constexpr i32 cMaxParallelRequests = 4;
  static constexpr i32 cMaxFallbackLevels   = 5;     // how many zoom levels up a cached tile is searched to fill gaps
  static constexpr i64 cRetryFailedTileMs   = 30000;
  static constexpr i32 cDragThresholdPx     = 4;

  QNetworkAccessManager* mpNetMgr = nullptr;
  QToolButton* mpBtnZoomIn = nullptr;
  QToolButton* mpBtnZoomOut = nullptr;

  QCache<u64, QPixmap> mTileCache{600};   // in-memory tiles (cost 1 per tile)
  QSet<u64> mTilesInFlight;
  QHash<u64, i64> mFailedTiles;           // tile key -> time of failure (ms since epoch)
  std::vector<u64> mWantedTiles;          // missing tiles of the current view, rebuilt on each paint

  MapProjection::SWorldPos mCenter{};     // world pixel position of the widget center at mZoom
  i32 mZoom = 8;
  MapProjection::SGeoPos mHome{};
  bool mHomeValid = false;
  std::vector<SMarker> mMarkers;
  std::vector<QRect> mMarkerRects;        // screen rectangles of the markers of the last paint

  QPoint mPressPos;
  MapProjection::SWorldPos mPressCenter{};
  bool mLeftPressed = false;
  bool mDragging = false;
  i32 mWheelAccu = 0;

  static u64 _tile_key(i32 iZoom, i32 iX, i32 iY);
  void _request_tiles();
  void _slot_tile_received(QNetworkReply* ipReply);
  void _zoom_at(i32 iNewZoom, const QPointF& iScreenPos);
  void _normalize_center();
  [[nodiscard]] QPointF _geo_to_screen(const MapProjection::SGeoPos& iGeo) const;
  [[nodiscard]] i32 _marker_at(const QPoint& iPos) const;
  void _draw_tile(QPainter& ioPainter, i32 iZoom, i32 iX, i32 iY, const QRect& iTarget);
  void _draw_markers(QPainter& ioPainter);
  void _draw_attribution(QPainter& ioPainter) const;
};
