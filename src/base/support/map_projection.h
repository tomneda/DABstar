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

// Web Mercator (EPSG:3857) helpers as used by OpenStreetMap slippy map tiles.
// Kept free of Qt so it can be unit tested standalone.

#include "glob_data_types.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace MapProjection
{
constexpr i32 cTileSize = 256;
constexpr i32 cMinZoom  = 2;
constexpr i32 cMaxZoom  = 18;
// latitude limit of Web Mercator (~85.0511°), calculated as a literal would lose precision with -fsingle-precision-constant
inline const f64 cMaxLatitude = std::atan(std::sinh(std::numbers::pi)) * 180.0 / std::numbers::pi;

struct SWorldPos  // pixel position in the world bitmap at a certain zoom level
{
  f64 x;
  f64 y;
};

struct SGeoPos
{
  f64 lat;
  f64 lon;
};

inline f64 world_size(const i32 iZoom)
{
  return (f64)cTileSize * std::ldexp(1.0, iZoom);
}

inline SWorldPos geo_to_world(const SGeoPos& iGeo, const i32 iZoom)
{
  const f64 size = world_size(iZoom);
  const f64 lat = std::clamp(iGeo.lat, -cMaxLatitude, cMaxLatitude) * std::numbers::pi / 180.0;
  const f64 x = (iGeo.lon + 180.0) / 360.0 * size;
  const f64 y = (1.0 - std::asinh(std::tan(lat)) / std::numbers::pi) / 2.0 * size;
  return { x, y };
}

inline SGeoPos world_to_geo(const SWorldPos& iWorld, const i32 iZoom)
{
  const f64 size = world_size(iZoom);
  const f64 lon = iWorld.x / size * 360.0 - 180.0;
  const f64 n = std::numbers::pi * (1.0 - 2.0 * iWorld.y / size);
  const f64 lat = std::atan(std::sinh(n)) * 180.0 / std::numbers::pi;
  return { lat, lon };
}

// Returns the highest zoom level (limited to iMaxZoom) at which the bounding box spanned by iMin/iMax
// fits into a viewport of iViewWidth x iViewHeight pixels, keeping iPadding pixels free at each side.
inline i32 fit_zoom(const SGeoPos& iMin, const SGeoPos& iMax, const i32 iViewWidth, const i32 iViewHeight, const i32 iPadding, const i32 iMaxZoom)
{
  const f64 availW = std::max(1, iViewWidth  - 2 * iPadding);
  const f64 availH = std::max(1, iViewHeight - 2 * iPadding);

  for (i32 zoom = iMaxZoom; zoom > cMinZoom; --zoom)
  {
    const SWorldPos p1 = geo_to_world({ iMax.lat, iMin.lon }, zoom); // top left
    const SWorldPos p2 = geo_to_world({ iMin.lat, iMax.lon }, zoom); // bottom right
    if (std::abs(p2.x - p1.x) <= availW && std::abs(p2.y - p1.y) <= availH)
    {
      return zoom;
    }
  }
  return cMinZoom;
}
} // namespace MapProjection
