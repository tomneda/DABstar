/*
 * Copyright (c) 2023 by Thomas Neder (https://github.com/tomneda)
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

#include "circlepushbutton.h"
#include "dab-constants.h"

#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <cmath>


CirclePushButton::CirclePushButton(QWidget * parent /*= nullptr*/) :
  QPushButton(parent),
  mTimer(new QTimer(this))
{
  connect(mTimer, &QTimer::timeout, this, &CirclePushButton::_slot_update_position);
}

void CirclePushButton::init(const QString & iImagePath, const f32 iRadiusPixels, const f32 iSecPerRound)
{
  mAnimationActive = false;
  mTimer->stop();
  mImagePath = iImagePath;
  mRadius = iRadiusPixels;
  mTimerMs = i32(1000.0f * sDegPerStep / 360.0f * iSecPerRound);
  setIcon(QIcon(mImagePath));
  update();
}

void CirclePushButton::start_animation()
{
  mCurAngle = 0;
  mTimer->stop();
  QPixmap image;
  QPainter painter(this);
  painter.drawPixmap(0, 0, image);
  setIcon(QIcon());
  mAnimationActive = true;
  mTimer->start(mTimerMs);
}

void CirclePushButton::stop_animation()
{
  mAnimationActive = false;
  mTimer->stop();
  setIcon(QIcon(mImagePath));
  update();
}

void CirclePushButton::paintEvent(QPaintEvent * event)
{
  QPushButton::paintEvent(event);

  if (mAnimationActive)
  {
    QSize buttonSize = size();
    QPixmap image(mImagePath);

    const i32 x = buttonSize.width()  / 2 + fixround<i32>(mRadius * std::cos(mCurAngle));
    const i32 y = buttonSize.height() / 2 + fixround<i32>(mRadius * std::sin(mCurAngle));

    QPainter painter(this);
    painter.drawPixmap(x - image.width() / 2, y - image.height() / 2, image);
  }
}

void CirclePushButton::_slot_update_position()
{
  mCurAngle += F_RAD_PER_DEG * sDegPerStep;

  if (mCurAngle > F_2_M_PI)
  {
    mCurAngle -= F_2_M_PI;
  }

  update();
}
