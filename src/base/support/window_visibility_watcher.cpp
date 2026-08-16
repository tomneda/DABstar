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

#include "window_visibility_watcher.h"
#include "indicator_button.h"
#include <QEvent>

WindowVisibilityWatcher::WindowVisibilityWatcher(QObject * parent)
  : QObject(parent)
{
}

WindowVisibilityWatcher::WindowVisibilityWatcher(QWidget * ipTarget, QObject * parent)
  : QObject(parent)
{
  attach(ipTarget);
}

WindowVisibilityWatcher::~WindowVisibilityWatcher()
{
  detach();
}

void WindowVisibilityWatcher::attach(QWidget * ipTarget)
{
  if (mpTarget.data() == ipTarget)
  {
    if (mpTarget != nullptr)
    {
      _update_visibility(mpTarget->isVisible());
    }
    return;
  }

  if (mpTarget != nullptr)
  {
    mpTarget->removeEventFilter(this);
    disconnect(mpTarget.data(), &QObject::destroyed, this, nullptr);
  }

  mpTarget = ipTarget;

  if (mpTarget != nullptr)
  {
    mpTarget->installEventFilter(this);
    connect(mpTarget.data(), &QObject::destroyed, this, [this]() {
      mpTarget = nullptr;
      _update_visibility(false);
    });
    _update_visibility(mpTarget->isVisible());
  }
  else
  {
    _update_visibility(false);
  }
}

void WindowVisibilityWatcher::detach()
{
  if (mpTarget != nullptr)
  {
    mpTarget->removeEventFilter(this);
    disconnect(mpTarget.data(), &QObject::destroyed, this, nullptr);
    mpTarget = nullptr;
  }
  _update_visibility(false);
}

bool WindowVisibilityWatcher::is_target_visible() const
{
  return mpTarget != nullptr && mpTarget->isVisible();
}

bool WindowVisibilityWatcher::eventFilter(QObject * watched, QEvent * event)
{
  if (watched == mpTarget.data())
  {
    switch (event->type())
    {
    case QEvent::Show:
      _update_visibility(true);
      break;
    case QEvent::Hide:
    case QEvent::Close:
      _update_visibility(false);
      break;
    default:
      break;
    }
  }
  return QObject::eventFilter(watched, event);
}

void WindowVisibilityWatcher::_update_visibility(const bool iVisible)
{
  if (mIsVisible != iVisible)
  {
    mIsVisible = iVisible;
    emit signal_visibility_changed(mIsVisible);
  }
}

WindowVisibilityWatcher * WindowVisibilityWatcher::bind(QWidget * ipTarget, IndicatorButton * ipButton)
{
  if (ipButton == nullptr)
  {
    return nullptr;
  }

  auto * watcher = new WindowVisibilityWatcher(ipTarget, ipButton);
  connect(watcher, &WindowVisibilityWatcher::signal_visibility_changed, ipButton, &IndicatorButton::set_indicator_active);
  if (ipTarget != nullptr)
  {
    ipButton->set_indicator_active(ipTarget->isVisible());
  }
  return watcher;
}
