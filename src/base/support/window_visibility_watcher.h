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
#include <QObject>
#include <QPointer>
#include <QWidget>

class IndicatorButton;

class WindowVisibilityWatcher : public QObject
{
  Q_OBJECT

public:
  explicit WindowVisibilityWatcher(QObject * parent = nullptr);
  explicit WindowVisibilityWatcher(QWidget * ipTarget, QObject * parent = nullptr);
  ~WindowVisibilityWatcher() override;

  void attach(QWidget * ipTarget);
  void detach();

  [[nodiscard]] bool is_target_visible() const;
  [[nodiscard]] QWidget * get_target() const { return mpTarget.data(); }

  // Convenience helper to bind a target window to an IndicatorButton
  static WindowVisibilityWatcher * bind(QWidget * ipTarget, IndicatorButton * ipButton);

signals:
  void signal_visibility_changed(bool iVisible);

protected:
  bool eventFilter(QObject * watched, QEvent * event) override;

private:
  QPointer<QWidget> mpTarget;
  bool mIsVisible = false;

  void _update_visibility(bool iVisible);
};
