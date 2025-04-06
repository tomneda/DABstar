/*
 * Copyright (c) 2024 by Thomas Neder (https://github.com/tomneda)
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

#include "compass_direction.h"
#include <vector>
#include <cmath>

// Function to find the compass direction for a given angle
/*static*/ std::string CompassDirection::get_compass_direction(double iAngleInDegree)
{
  struct SAngleDirection
  {
    const double angle;
    std::string direction;
  };

  // Define the angles and their corresponding compass directions
  const std::vector<SAngleDirection> compassPoints =
  {
    {  0.0, "N"}, { 22.5, "NNE"}, { 45.0, "NE"}, { 67.5, "ENE"},
    { 90.0, "E"}, {112.5, "ESE"}, {135.0, "SE"}, {157.5, "SSE"},
    {180.0, "S"}, {202.5, "SSW"}, {225.0, "SW"}, {247.5, "WSW"},
    {270.0, "W"}, {292.5, "WNW"}, {315.0, "NW"}, {337.5, "NNW"},
    {360.0, "N"}
  };

  // Normalize the angle to be within [0, 360] degrees
  iAngleInDegree = fmod(iAngleInDegree, 360.0);
  if (iAngleInDegree < 0)
  {
    iAngleInDegree += 360.0;
  }

  // Determine the closest direction
  std::string pResult = "N";
  double minDiff = 360.0;

  for (const auto & point : compassPoints)
  {
    if (double diff = std::abs(iAngleInDegree - point.angle);
        diff < minDiff)
    {
      minDiff = diff;
      pResult = point.direction;
    }
  }

  return pResult;
}
