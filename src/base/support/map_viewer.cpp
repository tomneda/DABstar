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
#include "map_viewer.h"
#include "tile_map_widget.h"
#include "dabradio.h"
#include "setting_helper.h"
#include "tii_codes.h"
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBrowser>
#include <algorithm>
#include <limits>

using namespace MapProjection;

MapViewer::MapViewer(DabRadio* const ipDabRadio, const cf32& iHomeLocation)
  : QWidget(nullptr)
  , mHomeLocation(iHomeLocation)
{
  setWindowTitle(PRJ_NAME " - TII map");
  setAttribute(Qt::WA_QuitOnClose, false);

  mpMap = new TileMapWidget(this);
  mpSideBar = new QTextBrowser(this);
  mpSideBar->setOpenLinks(false);
  mpSideBar->setMinimumWidth(150);

  mpSplitter = new QSplitter(Qt::Horizontal, this);
  mpSplitter->addWidget(mpMap);
  mpSplitter->addWidget(mpSideBar);
  mpSplitter->setStretchFactor(0, 1); // only the map grows when the window is resized
  mpSplitter->setStretchFactor(1, 0);
  mpSplitter->setChildrenCollapsible(false);

  auto* const pLayout = new QHBoxLayout(this);
  pLayout->setContentsMargins(0, 0, 0, 0);
  pLayout->addWidget(mpSplitter);

  resize(1000, 650);
  Settings::MapViewer::posAndSize.read_widget_geometry(this);

  if (!mpSplitter->restoreState(Settings::MapViewer::varSplitterState.read().toByteArray()))
  {
    // default: side bar just wide enough for the longest typical text line of a transmitter entry
    const i32 sideBarWidth = mpSideBar->fontMetrics().horizontalAdvance("Latitude 00.0000°, Longitude 000.0000°") + 40;
    mpSplitter->setSizes({ std::max(200, width() - sideBarWidth), sideBarWidth });
  }

  const SGeoPos home{ std::real(mHomeLocation), std::imag(mHomeLocation) };
  mpMap->set_home(home);
  mpMap->set_view(home, 8);

  connect(mpMap, &TileMapWidget::signal_marker_clicked, this, &MapViewer::_slot_marker_clicked);
  connect(mpMap, &TileMapWidget::signal_view_changed_by_user, this, [this]() { mSuppressAutoFrame = true; });
  // queued, as the slot deletes this object
  connect(this, &MapViewer::signal_terminating, ipDabRadio, &DabRadio::slot_map_terminate, Qt::QueuedConnection);
  connect(&mUpdateTimer, &QTimer::timeout, this, &MapViewer::_slot_update_timeout);

  _update_side_bar();
  mUpdateTimer.start(cUpdateIntervalMs);
  show();
}

MapViewer::~MapViewer()
{
  mUpdateTimer.stop();
  Settings::MapViewer::posAndSize.write_widget_geometry(this);
  Settings::MapViewer::varSplitterState.write(mpSplitter->saveState());
}

void MapViewer::closeEvent(QCloseEvent* const ipEvent)
{
  mUpdateTimer.stop();
  if (!mTerminating)
  {
    mTerminating = true;
    emit signal_terminating();
  }
  ipEvent->accept();
}

void MapViewer::reset()
{
  mPendingTransmitters.clear();
  mShownTransmitters.clear();
  mSelectedText.clear();
  mNrTransmittersFramed = 0;
  mSuppressAutoFrame = false;
  mpMap->set_markers({});
  _update_side_bar();
}

void MapViewer::add_transmitter(const STiiDataEntry& iTiiDataEntry, const f32 iStrength, const f32 iDistance, const f32 iAzimuth, const bool iNonEtsi)
{
  // check if entry was already added to the transmitter list
  const bool alreadyAdded = std::any_of(mPendingTransmitters.cbegin(), mPendingTransmitters.cend(), [&iTiiDataEntry](const STransmitter& iTr)
  {
    return iTr.latitude == iTiiDataEntry.latitude && iTr.longitude == iTiiDataEntry.longitude && iTr.transmitterName == iTiiDataEntry.transmitterName;
  });
  if (alreadyAdded)
  {
    return;
  }

  STransmitter tr;
  tr.latitude = iTiiDataEntry.latitude;
  tr.longitude = iTiiDataEntry.longitude;
  tr.transmitterName = iTiiDataEntry.transmitterName;
  tr.channelName = iTiiDataEntry.channel;
  tr.polarization = iTiiDataEntry.polarization;
  tr.direction = (iTiiDataEntry.direction.isEmpty() ? "undefined" : iTiiDataEntry.direction);
  tr.mainId = iTiiDataEntry.mainId;
  tr.subId = iTiiDataEntry.subId;
  tr.strength = iStrength;
  tr.distance = iDistance;
  tr.azimuth = iAzimuth;
  tr.power = iTiiDataEntry.power;
  tr.altitude = iTiiDataEntry.altitude;
  tr.height = iTiiDataEntry.height;
  tr.nonEtsi = iNonEtsi;
  mPendingTransmitters.emplace_back(tr);
}

void MapViewer::_slot_update_timeout()
{
  // show the transmitters collected since the last update
  mShownTransmitters = std::move(mPendingTransmitters);
  mPendingTransmitters.clear();

  std::vector<TileMapWidget::SMarker> markers;
  markers.reserve(mShownTransmitters.size());
  for (const STransmitter& tr : mShownTransmitters)
  {
    TileMapWidget::SMarker m;
    m.pos = { tr.latitude, tr.longitude };
    m.label = QString("%1-%2").arg(tr.mainId).arg(tr.subId);
    m.toolTip = QString("%1 %2km").arg(tr.transmitterName).arg(tr.distance, 0, 'f', 1);
    m.color = _get_marker_color(tr.strength);
    markers.emplace_back(m);
  }
  mpMap->set_markers(markers);

  _update_side_bar();
  _auto_frame();
}

void MapViewer::_auto_frame()
{
  if (mSuppressAutoFrame || mShownTransmitters.empty() || mNrTransmittersFramed >= mShownTransmitters.size())
  {
    return;
  }

  std::vector<SGeoPos> positions;
  positions.reserve(mShownTransmitters.size() + 1);
  f32 minDistKm = std::numeric_limits<f32>::max();
  for (const STransmitter& tr : mShownTransmitters)
  {
    positions.push_back({ tr.latitude, tr.longitude });
    minDistKm = std::min(minDistKm, tr.distance);
  }

  if (minDistKm < cHomeInFrameMaxDistKm) // add home to zoom range if a transmitter is near enough
  {
    positions.push_back({ std::real(mHomeLocation), std::imag(mHomeLocation) });
  }

  mNrTransmittersFramed = mShownTransmitters.size();
  mpMap->fit_to(positions, cFramePaddingPx, cFrameMaxZoom);
}

void MapViewer::_slot_marker_clicked(const i32 iIdx)
{
  if (iIdx < 0 || iIdx >= (i32)mShownTransmitters.size())
  {
    return;
  }
  mSelectedText = _get_transmitter_text(mShownTransmitters[iIdx]);
  _update_side_bar();
}

QColor MapViewer::_get_marker_color(const f32 iStrength)
{
  // linear interpolation: alpha = 1.0 at -6dB, 0.2 at -30dB
  const f32 alpha = std::clamp(1.0f - (iStrength + 6.0f) * 0.80f / -24.0f, 0.2f, 1.0f);
  QColor color(Qt::red);
  color.setAlphaF(alpha);
  return color;
}

QString MapViewer::_get_transmitter_text(const STransmitter& iTr)
{
  return QString("<b>%1</b> (%2 kW)<br>"
                 "Altitude %3m, Height %4m<br>"
                 "Latitude %5&deg;, Longitude %6&deg;<br>"
                 "Polarization %7, Direction %8<br>"
                 "TII %9-%10%11, Level %12dB<br>"
                 "Distance from home %13km %14&deg;")
    .arg(iTr.transmitterName.toHtmlEscaped())
    .arg(iTr.power, 0, 'f', 2)
    .arg(iTr.altitude)
    .arg(iTr.height)
    .arg(iTr.latitude, 0, 'f', 4)
    .arg(iTr.longitude, 0, 'f', 4)
    .arg(iTr.polarization.toUpper().toHtmlEscaped())
    .arg(iTr.direction.toHtmlEscaped())
    .arg(iTr.mainId)
    .arg(iTr.subId)
    .arg(iTr.nonEtsi ? ", non-ETSI phases" : "")
    .arg(iTr.strength, 0, 'f', 1)
    .arg(iTr.distance, 0, 'f', 1)
    .arg(iTr.azimuth, 0, 'f', 1);
}

void MapViewer::_update_side_bar() const
{
  QString html;

  if (!mSelectedText.isEmpty())
  {
    html += "<table border='1' cellpadding='6' cellspacing='0' width='100%'><tr><td>Selected:<br>" + mSelectedText + "</td></tr></table><br>";
  }

  html += QString("<b>Number of transmitters: %1</b><br>").arg(mShownTransmitters.size());
  if (!mShownTransmitters.empty())
  {
    html += "<b>Channel: " + mShownTransmitters.front().channelName.toHtmlEscaped() + "</b><br>";
  }
  html += "<br>";

  for (const STransmitter& tr : mShownTransmitters)
  {
    html += _get_transmitter_text(tr) + "<br><br>";
  }

  // keep the scroll position while the content is refreshed
  const i32 scrollPos = mpSideBar->verticalScrollBar()->value();
  mpSideBar->setHtml(html);
  mpSideBar->verticalScrollBar()->setValue(scrollPos);
}
