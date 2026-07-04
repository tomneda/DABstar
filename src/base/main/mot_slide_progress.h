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
#include "gap_progress_bar.h"
#include <QObject>
#include <QTimer>

class MotSlideProgress : public QObject
{
  Q_OBJECT
public:
  explicit MotSlideProgress(GapProgressBar * const pGapProgBarMot);
  ~MotSlideProgress() override = default;

  void do_progress(const i32 iPercent);
  void reset();

private:
  GapProgressBar * const mpGapProgBarMot;
  QTimer mTimerHide;

private slots:
  void _slot_timeout_hide_bar() const;
};


