/*
 * Copyright (c) 2023 by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
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

// Keep this Qt-free as also needed for Qt-free libs

#ifndef QWT_DEFS_H
#define QWT_DEFS_H

#include <qwt.h>

#ifdef QWT_VERSION
  #if (QWT_VERSION >> 8) < 0x0602
    using TQwtData = double; // the older QWT version provides no (assuming faster) sample stream with float but only double typed
  #else
    using TQwtData = float;
  #endif
#else
  #error "QWT_VERSION not defined"
#endif
#endif // QWT_DEFS_H
