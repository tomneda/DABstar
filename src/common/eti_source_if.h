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
#include <array>

// Size of one ETI(NI) frame (24 ms), see ETSI EN 300 799
inline constexpr i32 cEtiFrameSize = 6144;
using TEtiFrame = std::array<u8, cEtiFrameSize>;

// A device delivering ETI frames instead of IQ samples (e.g. an ETI file reader).
// The frames are already synchronized (start with ERR and FSYNC) and are delivered in real time pace.
class IEtiSource
{
public:
  virtual ~IEtiSource() = default;

  // Blocks until a frame is available or iTimeout_ms has elapsed. Returns false on timeout.
  virtual bool get_eti_frame(TEtiFrame& oFrame, i32 iTimeout_ms) = 0;
};
