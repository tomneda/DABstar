/*
 * Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
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

// Central place for Qt version dependent substitutions to avoid #if QT_VERSION ... spread over the code base.

#pragma once

#include <QtGlobal>

/*
 * Qt 6.7 introduced QCheckBox::checkStateChanged(Qt::CheckState) and deprecated QCheckBox::stateChanged(int).
 *
 * Write
 *   connect(cb, &QCheckBox::stateChangedSubst, this, &MyClass::_slot_handle_cb);
 * instead of an #if/#else block around both signal names.
 *
 * The slot may take an i32/int (Qt::CheckState converts implicitly) or nothing at all, so it stays
 * source compatible with both Qt versions. Do not declare the slot as taking a Qt::CheckState.
 */
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  #define stateChangedSubst checkStateChanged
#else
  #define stateChangedSubst stateChanged
#endif
