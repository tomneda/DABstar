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

#include "mot_slide_progress.h"

static constexpr i32 cTimehoutHideBar = 5000;

MotSlideProgress::MotSlideProgress(GapProgressBar * const pGapProgBarMot)
  : mpGapProgBarMot(pGapProgBarMot)
{
  mpGapProgBarMot->setMaximum(100);
  mpGapProgBarMot->setFillColor(0x1A5FB4);

  mTimerHide.setSingleShot(true);
  connect(&mTimerHide, &QTimer::timeout, this, &MotSlideProgress::_slot_timeout_hide_bar);
}

void MotSlideProgress::do_progress(const i32 iPercent)
{
  mpGapProgBarMot->setValue(iPercent);
  mTimerHide.start(iPercent < 100 ? 2 * cTimehoutHideBar : cTimehoutHideBar); // (re-)start hide-timer (wait longer while not finished for slow transmissions)
}

void MotSlideProgress::reset()
{
  mTimerHide.stop();
  mpGapProgBarMot->setValue(0);
}

void MotSlideProgress::_slot_timeout_hide_bar() const
{
  mpGapProgBarMot->setValue(0);
}
