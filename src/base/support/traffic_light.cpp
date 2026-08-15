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

#include "traffic_light.h"
#include <QEvent>
#include <QTimer>

TrafficLight::TrafficLight(QWidget * const parent)
  : QWidget(parent)
{
  _init_ui(QString());
}

TrafficLight::TrafficLight(const QString & iText, QWidget * const parent)
  : QWidget(parent)
{
  _init_ui(iText);
}

TrafficLight::TrafficLight(const QString & iText, const EStage iStage, QWidget * const parent)
  : QWidget(parent)
{
  _init_ui(iText);
  set_stage(iStage);
}

TrafficLight::TrafficLight(const EStage iStage, QWidget * const parent)
  : QWidget(parent)
{
  _init_ui(QString());
  set_stage(iStage);
}

TrafficLight::TrafficLight(const QString & iText, const EStage iStage, const i32 iTimeoutMs, const EStage iTimeoutStage, QWidget * const parent)
  : QWidget(parent)
{
  _init_ui(iText);
  set_stage(iStage);
  set_timeout(iTimeoutMs, iTimeoutStage);
}

TrafficLight::TrafficLight(const EStage iStage, const i32 iTimeoutMs, const EStage iTimeoutStage, QWidget * const parent)
  : QWidget(parent)
{
  _init_ui(QString());
  set_stage(iStage);
  set_timeout(iTimeoutMs, iTimeoutStage);
}

void TrafficLight::_init_ui(const QString & iText)
{
  mText = iText;

  mpLayout = new QHBoxLayout(this);
  mpLayout->setContentsMargins(0, 0, 0, 0);
  mpLayout->setSpacing(4);
  mpLayout->setAlignment(Qt::AlignCenter);

  mpLabel = new QLabel(this);
  mpLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  mpLabel->setText(mText);
  mpLabel->setVisible(!mText.isEmpty());

  mpIconLabel = new QLabel(this);
  mpIconLabel->setAlignment(Qt::AlignCenter);
  mpIconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  mpLayout->addWidget(mpLabel);
  mpLayout->addWidget(mpIconLabel);

  _update_display();
}

const QPixmap & TrafficLight::_get_cached_pixmap(const EStage iStage)
{
  static const QPixmap sPixmaps[cStageCount] = {
    QPixmap{":/res/icons/traffic_light_h_off.svg"},          // 0
    QPixmap{":/res/icons/traffic_light_h_red.svg"},          // 1
    QPixmap{":/res/icons/traffic_light_h_red_yellow.svg"},   // 2
    QPixmap{":/res/icons/traffic_light_h_yellow.svg"},       // 3
    QPixmap{":/res/icons/traffic_light_h_yellow_green.svg"}, // 4
    QPixmap{":/res/icons/traffic_light_h_green.svg"}         // 5
  };

  const auto stageIdx = static_cast<size_t>(iStage);
  if (stageIdx >= cStageCount)
  {
    return sPixmaps[0];
  }
  return sPixmaps[stageIdx];
}

void TrafficLight::_update_display()
{
  if (mpIconLabel == nullptr)
  {
    return;
  }

  const EStage stageToDisplay = isEnabled() ? mStage : EStage::Off;
  const QPixmap & basePixmap = _get_cached_pixmap(stageToDisplay);

  if (!basePixmap.isNull())
  {
    if (mIconSize == basePixmap.size() || mIconSize.isEmpty())
    {
      mpIconLabel->setPixmap(basePixmap);
      mpIconLabel->setFixedSize(basePixmap.size());
    }
    else
    {
      mpIconLabel->setPixmap(basePixmap.scaled(mIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
      mpIconLabel->setFixedSize(mIconSize);
    }
  }
}

void TrafficLight::set_stage(const EStage iStage)
{
  if (mStage != iStage)
  {
    mStage = iStage;
    _update_display();
    emit signal_stage_changed(mStage);
  }

  _restart_timer_if_needed();
}

void TrafficLight::set_timeout(const i32 iTimeoutMs, const EStage iTimeoutStage)
{
  mTimeoutMs = iTimeoutMs;
  mTimeoutStage = iTimeoutStage;

  if (mTimeoutMs > 0)
  {
    _get_or_create_timer()->setInterval(mTimeoutMs);
    if (mStage != mTimeoutStage)
    {
      mpTimer->start(mTimeoutMs);
    }
    else if (mpTimer != nullptr)
    {
      mpTimer->stop();
    }
  }
  else if (mpTimer != nullptr)
  {
    mpTimer->stop();
  }
}

bool TrafficLight::is_timer_active() const
{
  return mpTimer != nullptr && mpTimer->isActive();
}

void TrafficLight::restart_timeout_timer()
{
  if (mTimeoutMs > 0 && mStage != mTimeoutStage)
  {
    _get_or_create_timer()->start(mTimeoutMs);
  }
}

void TrafficLight::stop_timeout_timer()
{
  if (mpTimer != nullptr)
  {
    mpTimer->stop();
  }
}

void TrafficLight::_restart_timer_if_needed()
{
  if (mTimeoutMs > 0)
  {
    if (mStage != mTimeoutStage)
    {
      _get_or_create_timer()->start(mTimeoutMs);
    }
    else if (mpTimer != nullptr)
    {
      mpTimer->stop();
    }
  }
}

QTimer * TrafficLight::_get_or_create_timer()
{
  if (mpTimer == nullptr)
  {
    mpTimer = new QTimer(this);
    mpTimer->setSingleShot(true);
    connect(mpTimer, &QTimer::timeout, this, &TrafficLight::_slot_timeout);
  }
  return mpTimer;
}

void TrafficLight::_slot_timeout()
{
  if (mStage != mTimeoutStage)
  {
    set_stage(mTimeoutStage);
    emit signal_timeout();
  }
}

void TrafficLight::set_text(const QString & iText)
{
  if (mText != iText)
  {
    mText = iText;
    if (mpLabel != nullptr)
    {
      mpLabel->setText(mText);
      mpLabel->setVisible(!mText.isEmpty());
    }
    updateGeometry();
  }
}

void TrafficLight::set_alignment(const Qt::Alignment iAlignment)
{
  if (mpLayout != nullptr)
  {
    mpLayout->setAlignment(iAlignment);
  }
}

Qt::Alignment TrafficLight::get_alignment() const
{
  if (mpLayout != nullptr)
  {
    return mpLayout->alignment();
  }
  return Qt::AlignCenter;
}

void TrafficLight::set_label_alignment(const Qt::Alignment iAlignment)
{
  if (mpLabel != nullptr)
  {
    mpLabel->setAlignment(iAlignment);
  }
}

Qt::Alignment TrafficLight::get_label_alignment() const
{
  if (mpLabel != nullptr)
  {
    return mpLabel->alignment();
  }
  return Qt::AlignRight | Qt::AlignVCenter;
}

void TrafficLight::set_icon_size(const QSize & iSize)
{
  if (mIconSize != iSize && !iSize.isEmpty())
  {
    mIconSize = iSize;
    _update_display();
    updateGeometry();
  }
}

void TrafficLight::changeEvent(QEvent * const event)
{
  QWidget::changeEvent(event);
  if (event != nullptr && event->type() == QEvent::EnabledChange)
  {
    _update_display();
  }
}

QSize TrafficLight::sizeHint() const
{
  if (mpLayout != nullptr)
  {
    return mpLayout->sizeHint();
  }
  return QSize(85, cDefaultIconHeight);
}

QSize TrafficLight::minimumSizeHint() const
{
  if (mpLayout != nullptr)
  {
    return mpLayout->minimumSize();
  }
  return QSize(cDefaultIconWidth, cDefaultIconHeight);
}
