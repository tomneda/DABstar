#include "src/base/support/map_projection.h"

#include <cmath>
#include <cstdio>

using namespace MapProjection;

namespace
{
bool nearly_equal(const char* const iName, const f64 iActual, const f64 iExpected, const f64 iTolerance)
{
  if (std::abs(iActual - iExpected) <= iTolerance)
  {
    return true;
  }
  std::fprintf(stderr, "%s: got %.9f, expected %.9f (tolerance %.3g)\n", iName, iActual, iExpected, iTolerance);
  return false;
}
}

int main()
{
  bool ok = true;

  // world size
  ok &= nearly_equal("world_size(0)", world_size(0), 256.0, 0.0);
  ok &= nearly_equal("world_size(10)", world_size(10), 256.0 * 1024.0, 0.0);

  // reference points
  {
    const SWorldPos w = geo_to_world({ 0.0, 0.0 }, 1);
    ok &= nearly_equal("origin x", w.x, 256.0, 1e-9);
    ok &= nearly_equal("origin y", w.y, 256.0, 1e-9);
  }
  {
    const SWorldPos w = geo_to_world({ cMaxLatitude, -180.0 }, 3);
    ok &= nearly_equal("top left x", w.x, 0.0, 1e-9);
    ok &= nearly_equal("top left y", w.y, 0.0, 1e-6);
  }
  {
    // Munich (48.1372N, 11.5756E) is in OSM tile 8/136/88 at zoom 8
    const SWorldPos w = geo_to_world({ 48.1372, 11.5756 }, 8);
    ok &= nearly_equal("munich tile x", std::floor(w.x / cTileSize), 136.0, 0.0);
    ok &= nearly_equal("munich tile y", std::floor(w.y / cTileSize), 88.0, 0.0);
  }

  // round trip
  for (i32 zoom = 0; zoom <= cMaxZoom; zoom += 3)
  {
    for (f64 lat = -80.0; lat <= 80.0; lat += 20.0)
    {
      for (f64 lon = -170.0; lon <= 170.0; lon += 34.0)
      {
        const SGeoPos g = world_to_geo(geo_to_world({ lat, lon }, zoom), zoom);
        ok &= nearly_equal("round trip lat", g.lat, lat, 1e-9);
        ok &= nearly_equal("round trip lon", g.lon, lon, 1e-9);
      }
    }
  }

  // fit zoom: a single point is shown with the maximum zoom
  ok &= nearly_equal("fit single", fit_zoom({ 48.0, 11.0 }, { 48.0, 11.0 }, 800, 600, 20, 13), 13.0, 0.0);

  // fit zoom: the found zoom fits, the next one does not
  {
    const SGeoPos minPos{ 47.0, 9.0 };
    const SGeoPos maxPos{ 49.0, 13.0 };
    const i32 zoom = fit_zoom(minPos, maxPos, 800, 600, 20, cMaxZoom);
    const auto span_x = [&](const i32 iZoom) { return geo_to_world({ 0, maxPos.lon }, iZoom).x - geo_to_world({ 0, minPos.lon }, iZoom).x; };
    const auto span_y = [&](const i32 iZoom) { return geo_to_world({ minPos.lat, 0 }, iZoom).y - geo_to_world({ maxPos.lat, 0 }, iZoom).y; };
    ok &= (span_x(zoom) <= 760.0 && span_y(zoom) <= 560.0);
    ok &= (span_x(zoom + 1) > 760.0 || span_y(zoom + 1) > 560.0);
    ok &= nearly_equal("fit area zoom", zoom, 8.0, 0.0);
  }

  // fit zoom: never below the minimum zoom
  ok &= nearly_equal("fit world", fit_zoom({ -80.0, -179.0 }, { 80.0, 179.0 }, 100, 100, 20, cMaxZoom), (f64)cMinZoom, 0.0);

  if (!ok)
  {
    std::fprintf(stderr, "map_projection_test FAILED\n");
    return 1;
  }
  std::printf("map_projection_test passed\n");
  return 0;
}
