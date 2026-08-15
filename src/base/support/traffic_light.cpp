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
#include <algorithm>

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

TrafficLight::TrafficLight(const QString & iText, const i32 iStage, QWidget * const parent)
  : QWidget(parent)
{
  _init_ui(iText);
  set_stage(iStage);
}

TrafficLight::TrafficLight(const i32 iStage, QWidget * const parent)
  : QWidget(parent)
{
  _init_ui(QString());
  set_stage(iStage);
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

const QPixmap & TrafficLight::_get_cached_pixmap(const i32 iStage)
{
  static const QPixmap sPixmaps[cStageCount] = {
    QPixmap{":/res/icons/traffic_light_h_off.svg"},          // 0
    QPixmap{":/res/icons/traffic_light_h_red.svg"},          // 1
    QPixmap{":/res/icons/traffic_light_h_red_yellow.svg"},   // 2
    QPixmap{":/res/icons/traffic_light_h_yellow.svg"},       // 3
    QPixmap{":/res/icons/traffic_light_h_yellow_green.svg"}, // 4
    QPixmap{":/res/icons/traffic_light_h_green.svg"}         // 5
  };

  if (iStage < 0 || iStage >= cStageCount)
  {
    return sPixmaps[0];
  }
  return sPixmaps[iStage];
}

void TrafficLight::_update_display()
{
  if (mpIconLabel == nullptr)
  {
    return;
  }

  const i32 stageToDisplay = isEnabled() ? mStage : static_cast<i32>(EStage::Off);
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

void TrafficLight::set_stage(const i32 iStage)
{
  const i32 clampedStage = std::clamp(iStage, 0, cStageCount - 1);
  if (mStage != clampedStage)
  {
    mStage = clampedStage;
    _update_display();
    emit signal_stage_changed(mStage);
  }
}

void TrafficLight::set_stage(const EStage iStage)
{
  set_stage(static_cast<i32>(iStage));
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
