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
#include "tile_map_widget.h"
#include <QDateTime>
#include <QDir>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QStandardPaths>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace MapProjection;

namespace
{
constexpr char cTileUrlTemplate[] = "https://tile.openstreetmap.org/%1/%2/%3.png";
constexpr char cUserAgent[] = PRJ_NAME "/" PRJ_VERS " (+https://github.com/tomneda/DABstar)";
constexpr char cAttribution[] = "© OpenStreetMap contributors";
constexpr char cTileKeyProperty[] = "tileKey";
constexpr i64 cDiskCacheSize = 200 * 1024 * 1024;
}

TileMapWidget::TileMapWidget(QWidget* const ipParent)
  : QWidget(ipParent)
{
  setMouseTracking(true);
  setCursor(Qt::OpenHandCursor);
  setMinimumSize(200, 200);
  setFocusPolicy(Qt::WheelFocus);

  mpNetMgr = new QNetworkAccessManager(this);
  const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  if (!cacheDir.isEmpty())
  {
    auto* const pDiskCache = new QNetworkDiskCache(mpNetMgr);
    pDiskCache->setCacheDirectory(QDir(cacheDir).filePath("map_tiles"));
    pDiskCache->setMaximumCacheSize(cDiskCacheSize);
    mpNetMgr->setCache(pDiskCache);
  }
  connect(mpNetMgr, &QNetworkAccessManager::finished, this, &TileMapWidget::_slot_tile_received);

  const auto create_zoom_button = [this](const QString& iText, const QString& iToolTip, const i32 iDelta)
  {
    auto* const pBtn = new QToolButton(this);
    pBtn->setText(iText);
    pBtn->setToolTip(iToolTip);
    pBtn->setFixedSize(28, 28);
    pBtn->setCursor(Qt::ArrowCursor);
    pBtn->setStyleSheet("QToolButton { font-size: 16px; font-weight: bold; background: rgba(255,255,255,220); color: black;"
                        " border: 1px solid #888; border-radius: 3px; }"
                        "QToolButton:hover { background: white; }");
    connect(pBtn, &QToolButton::clicked, this, [this, iDelta]()
    {
      _zoom_at(mZoom + iDelta, QPointF(width() / 2.0, height() / 2.0));
      emit signal_view_changed_by_user();
    });
    return pBtn;
  };
  mpBtnZoomIn  = create_zoom_button("+", "Zoom in", +1);
  mpBtnZoomOut = create_zoom_button("−", "Zoom out", -1);
  mpBtnZoomIn->move(8, 8);
  mpBtnZoomOut->move(8, 8 + 28 + 2);

  set_view({ 50.0, 10.0 }, 5);
}

void TileMapWidget::set_home(const SGeoPos& iHome)
{
  mHome = iHome;
  mHomeValid = true;
  update();
}

void TileMapWidget::set_markers(const std::vector<SMarker>& iMarkers)
{
  mMarkers = iMarkers;
  update();
}

void TileMapWidget::set_view(const SGeoPos& iCenter, const i32 iZoom)
{
  mZoom = std::clamp(iZoom, cMinZoom, cMaxZoom);
  mCenter = geo_to_world(iCenter, mZoom);
  _normalize_center();
  update();
}

void TileMapWidget::fit_to(const std::vector<SGeoPos>& iPositions, const i32 iPadding, const i32 iMaxZoom)
{
  if (iPositions.empty())
  {
    return;
  }

  SGeoPos minPos{ std::numeric_limits<f64>::max(), std::numeric_limits<f64>::max() };
  SGeoPos maxPos{ std::numeric_limits<f64>::lowest(), std::numeric_limits<f64>::lowest() };
  for (const SGeoPos& p : iPositions)
  {
    minPos.lat = std::min(minPos.lat, p.lat);
    minPos.lon = std::min(minPos.lon, p.lon);
    maxPos.lat = std::max(maxPos.lat, p.lat);
    maxPos.lon = std::max(maxPos.lon, p.lon);
  }

  const i32 zoom = fit_zoom(minPos, maxPos, width(), height(), iPadding, std::min(iMaxZoom, cMaxZoom));
  const SWorldPos p1 = geo_to_world({ maxPos.lat, minPos.lon }, zoom);
  const SWorldPos p2 = geo_to_world({ minPos.lat, maxPos.lon }, zoom);
  mZoom = zoom;
  mCenter = { (p1.x + p2.x) / 2.0, (p1.y + p2.y) / 2.0 };
  _normalize_center();
  update();
}

u64 TileMapWidget::_tile_key(const i32 iZoom, const i32 iX, const i32 iY)
{
  return ((u64)iZoom << 48) | ((u64)(u32)iX << 24) | (u64)(u32)iY;
}

void TileMapWidget::_normalize_center()
{
  const f64 size = world_size(mZoom);
  mCenter.x = std::fmod(mCenter.x, size);
  if (mCenter.x < 0)
  {
    mCenter.x += size;
  }
  mCenter.y = std::clamp<f64>(mCenter.y, 0.0, size);
}

QPointF TileMapWidget::_geo_to_screen(const SGeoPos& iGeo) const
{
  const f64 size = world_size(mZoom);
  const SWorldPos w = geo_to_world(iGeo, mZoom);
  f64 dx = w.x - mCenter.x;
  // take the nearest copy of the (horizontally repeated) world
  dx = std::remainder(dx, size);
  return { std::floor(width() / 2.0) + dx, std::floor(height() / 2.0) + (w.y - mCenter.y) };
}

void TileMapWidget::_zoom_at(const i32 iNewZoom, const QPointF& iScreenPos)
{
  const i32 newZoom = std::clamp(iNewZoom, cMinZoom, cMaxZoom);
  if (newZoom == mZoom)
  {
    return;
  }

  const f64 offsX = iScreenPos.x() - std::floor(width() / 2.0);
  const f64 offsY = iScreenPos.y() - std::floor(height() / 2.0);
  const SGeoPos geoAtCursor = world_to_geo({ mCenter.x + offsX, mCenter.y + offsY }, mZoom);

  mZoom = newZoom;
  const SWorldPos w = geo_to_world(geoAtCursor, mZoom);
  mCenter = { w.x - offsX, w.y - offsY };
  _normalize_center();
  update();
}

void TileMapWidget::paintEvent(QPaintEvent* /*ipEvent*/)
{
  QPainter painter(this);
  painter.fillRect(rect(), QColor(0xDD, 0xDD, 0xDD));

  const i32 n = 1 << mZoom;
  const i64 left = (i64)std::floor(mCenter.x - std::floor(width() / 2.0));
  const i64 top  = (i64)std::floor(mCenter.y - std::floor(height() / 2.0));
  const i64 tx0 = (i64)std::floor((f64)left / cTileSize);
  const i64 tx1 = (i64)std::floor((f64)(left + width() - 1) / cTileSize);
  const i64 ty0 = std::max<i64>(0, (i64)std::floor((f64)top / cTileSize));
  const i64 ty1 = std::min<i64>(n - 1, (i64)std::floor((f64)(top + height() - 1) / cTileSize));

  mWantedTiles.clear();
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  for (i64 ty = ty0; ty <= ty1; ++ty)
  {
    for (i64 tx = tx0; tx <= tx1; ++tx)
    {
      const i32 wx = (i32)(((tx % n) + n) % n);
      const QRect target((i32)(tx * cTileSize - left), (i32)(ty * cTileSize - top), cTileSize, cTileSize);
      _draw_tile(painter, mZoom, wx, (i32)ty, target);
    }
  }

  _draw_markers(painter);
  _draw_attribution(painter);

  _request_tiles();
}

void TileMapWidget::_draw_tile(QPainter& ioPainter, const i32 iZoom, const i32 iX, const i32 iY, const QRect& iTarget)
{
  const u64 key = _tile_key(iZoom, iX, iY);
  if (const QPixmap* const pPixmap = mTileCache.object(key))
  {
    ioPainter.drawPixmap(iTarget, *pPixmap);
    return;
  }

  mWantedTiles.push_back(key);

  // fill the gap with an upscaled part of a coarser tile, if available
  for (i32 d = 1; d <= cMaxFallbackLevels && iZoom - d >= 0; ++d)
  {
    if (const QPixmap* const pPixmap = mTileCache.object(_tile_key(iZoom - d, iX >> d, iY >> d)))
    {
      const f64 sub = (f64)pPixmap->width() / (f64)(1 << d);
      const i32 mask = (1 << d) - 1;
      const QRectF source((iX & mask) * sub, (iY & mask) * sub, sub, sub);
      ioPainter.drawPixmap(QRectF(iTarget), *pPixmap, source);
      return;
    }
  }
}

void TileMapWidget::_draw_markers(QPainter& ioPainter)
{
  ioPainter.setRenderHint(QPainter::Antialiasing, true);

  if (mHomeValid)
  {
    const QPointF p = _geo_to_screen(mHome);
    ioPainter.setPen(QPen(QColor(0xE0, 0x20, 0x20), 3));
    ioPainter.setBrush(Qt::NoBrush);
    ioPainter.drawEllipse(p, 7.0, 7.0);
  }

  QFont font = this->font();
  font.setBold(true);
  ioPainter.setFont(font);
  const QFontMetrics fm(font);

  mMarkerRects.clear();
  mMarkerRects.reserve(mMarkers.size());

  for (const SMarker& m : mMarkers)
  {
    const QPointF p = _geo_to_screen(m.pos);
    const i32 w = fm.horizontalAdvance(m.label) + 6;
    const i32 h = fm.height() + 2;
    const QRect r((i32)std::lround(p.x() - w / 2.0), (i32)std::lround(p.y() - h / 2.0), w, h);
    mMarkerRects.push_back(r);

    ioPainter.setPen(QPen(QColor(0, 0, 0, 160), 1));
    ioPainter.setBrush(m.color);
    ioPainter.drawRect(r);
    ioPainter.setPen(Qt::black);
    ioPainter.drawText(r, Qt::AlignCenter, m.label);
  }
}

void TileMapWidget::_draw_attribution(QPainter& ioPainter) const
{
  QFont font = this->font();
  font.setPointSizeF(font.pointSizeF() * 0.85);
  ioPainter.setFont(font);
  const QFontMetrics fm(font);
  const QString text = cAttribution;
  const i32 w = fm.horizontalAdvance(text) + 8;
  const i32 h = fm.height() + 2;
  const QRect r(width() - w, height() - h, w, h);
  ioPainter.setPen(Qt::NoPen);
  ioPainter.setBrush(QColor(255, 255, 255, 190));
  ioPainter.drawRect(r);
  ioPainter.setPen(QColor(0x33, 0x33, 0x33));
  ioPainter.drawText(r, Qt::AlignCenter, text);
}

void TileMapWidget::_request_tiles()
{
  const i64 now = QDateTime::currentMSecsSinceEpoch();

  for (const u64 key : mWantedTiles)
  {
    if (mTilesInFlight.size() >= cMaxParallelRequests)
    {
      break;
    }
    if (mTilesInFlight.contains(key))
    {
      continue;
    }
    if (const auto it = mFailedTiles.constFind(key); it != mFailedTiles.cend() && now - it.value() < cRetryFailedTileMs)
    {
      continue;
    }

    const i32 zoom = (i32)(key >> 48);
    const i32 x = (i32)((key >> 24) & 0xFFFFFF);
    const i32 y = (i32)(key & 0xFFFFFF);

    QNetworkRequest request(QUrl(QString(cTileUrlTemplate).arg(zoom).arg(x).arg(y)));
    request.setHeader(QNetworkRequest::UserAgentHeader, cUserAgent);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    QNetworkReply* const pReply = mpNetMgr->get(request);
    pReply->setProperty(cTileKeyProperty, QVariant::fromValue<qulonglong>(key));
    mTilesInFlight.insert(key);
  }
}

void TileMapWidget::_slot_tile_received(QNetworkReply* const ipReply)
{
  ipReply->deleteLater();
  const u64 key = ipReply->property(cTileKeyProperty).toULongLong();
  mTilesInFlight.remove(key);

  bool ok = false;
  if (ipReply->error() == QNetworkReply::NoError)
  {
    auto* const pPixmap = new QPixmap();
    if (pPixmap->loadFromData(ipReply->readAll()))
    {
      mTileCache.insert(key, pPixmap);
      mFailedTiles.remove(key);
      ok = true;
    }
    else
    {
      delete pPixmap;
    }
  }

  if (!ok)
  {
    qWarning() << "Map tile request failed:" << ipReply->url().toString() << ipReply->errorString();
    mFailedTiles.insert(key, QDateTime::currentMSecsSinceEpoch());
  }

  update(); // also triggers further requests of still missing tiles
}

i32 TileMapWidget::_marker_at(const QPoint& iPos) const
{
  for (i32 i = (i32)mMarkerRects.size() - 1; i >= 0; --i) // topmost marker is painted last
  {
    if (mMarkerRects[i].contains(iPos))
    {
      return i;
    }
  }
  return -1;
}

void TileMapWidget::mousePressEvent(QMouseEvent* const ipEvent)
{
  if (ipEvent->button() == Qt::LeftButton)
  {
    mLeftPressed = true;
    mDragging = false;
    mPressPos = ipEvent->position().toPoint();
    mPressCenter = mCenter;
  }
  QWidget::mousePressEvent(ipEvent);
}

void TileMapWidget::mouseMoveEvent(QMouseEvent* const ipEvent)
{
  const QPoint pos = ipEvent->position().toPoint();

  if (mLeftPressed)
  {
    const QPoint delta = pos - mPressPos;
    if (!mDragging && delta.manhattanLength() >= cDragThresholdPx)
    {
      mDragging = true;
      setCursor(Qt::ClosedHandCursor);
    }
    if (mDragging)
    {
      mCenter = { mPressCenter.x - delta.x(), mPressCenter.y - delta.y() };
      _normalize_center();
      update();
      emit signal_view_changed_by_user();
    }
  }
  else
  {
    setCursor(_marker_at(pos) >= 0 ? Qt::PointingHandCursor : Qt::OpenHandCursor);
  }
  QWidget::mouseMoveEvent(ipEvent);
}

void TileMapWidget::mouseReleaseEvent(QMouseEvent* const ipEvent)
{
  if (ipEvent->button() == Qt::LeftButton && mLeftPressed)
  {
    mLeftPressed = false;
    if (!mDragging)
    {
      const i32 idx = _marker_at(ipEvent->position().toPoint());
      if (idx >= 0 && idx < (i32)mMarkers.size())
      {
        emit signal_marker_clicked(idx);
      }
    }
    mDragging = false;
    setCursor(Qt::OpenHandCursor);
  }
  QWidget::mouseReleaseEvent(ipEvent);
}

void TileMapWidget::mouseDoubleClickEvent(QMouseEvent* const ipEvent)
{
  if (ipEvent->button() == Qt::LeftButton)
  {
    _zoom_at(mZoom + 1, ipEvent->position());
    emit signal_view_changed_by_user();
  }
  else if (ipEvent->button() == Qt::RightButton)
  {
    _zoom_at(mZoom - 1, ipEvent->position());
    emit signal_view_changed_by_user();
  }
}

void TileMapWidget::wheelEvent(QWheelEvent* const ipEvent)
{
  // accumulate small steps of touchpads, one zoom level per wheel notch (120)
  mWheelAccu += ipEvent->angleDelta().y();
  const i32 steps = mWheelAccu / 120;
  if (steps != 0)
  {
    mWheelAccu -= steps * 120;
    _zoom_at(mZoom + steps, ipEvent->position());
    emit signal_view_changed_by_user();
  }
  ipEvent->accept();
}

bool TileMapWidget::event(QEvent* const ipEvent)
{
  if (ipEvent->type() == QEvent::ToolTip)
  {
    const auto* const pHelpEvent = static_cast<QHelpEvent*>(ipEvent);
    const i32 idx = _marker_at(pHelpEvent->pos());
    if (idx >= 0 && idx < (i32)mMarkers.size())
    {
      QToolTip::showText(pHelpEvent->globalPos(), mMarkers[idx].toolTip, this, mMarkerRects[idx]);
    }
    else
    {
      QToolTip::hideText();
      ipEvent->ignore();
    }
    return true;
  }
  return QWidget::event(ipEvent);
}
