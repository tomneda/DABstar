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

#ifndef DABSTAR_CIRCLEPUSHBUTTON_H
#define DABSTAR_CIRCLEPUSHBUTTON_H

#include <QPushButton>

class CirclePushButton : public QPushButton
{
  Q_OBJECT
public:
  explicit CirclePushButton(QWidget * parent = nullptr);

  void init(const QString & iImagePath, const float iRadiusPixels, const float iSecPerRound);
  void start_animation();
  void stop_animation();

protected:
  void paintEvent(QPaintEvent * event) override;

private slots:
  void _slot_update_position();

private:
  static constexpr float sDegPerStep = 5.0f;
  bool mAnimationActive = false;
  QString mImagePath;
  QTimer * mTimer = nullptr;
  float mCurAngle = 0;
  float mRadius = 0;
  int32_t mTimerMs = 50;
};

#endif // DABSTAR_CIRCLEPUSHBUTTON_H
