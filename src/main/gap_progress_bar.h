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

#include <QProgressBar>
#include <QPainter>

class GapProgressBar : public QProgressBar
{
  Q_OBJECT
  // Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor)
  // Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor)
  // Q_PROPERTY(int frameWidth READ frameWidth WRITE setFrameWidth)

public:
  explicit GapProgressBar(QWidget * parent = nullptr)
    : QProgressBar(parent)
  {
    setTextVisible(false);
    setAttribute(Qt::WA_TranslucentBackground);
  }

  void setOrigin(int iOrigin)
  {
    mOrigin = iOrigin;
    update();
  }

  int origin() const { return mOrigin; }

  void setFillColor(const QColor & iC)
  {
    mFillBrush = iC;
    update();
  }

  QColor fillColor() const { return mFillBrush.color(); }

  void setFrameColor(const QColor & iC)
  {
    mFrameColor = iC;
    update();
  }

  QColor frameColor() const { return mFrameColor; }

  void setFrameWidth(int iW)
  {
    mFrameWidth = qMax(0, iW);
    update();
  }

  int frameWidth() const { return mFrameWidth; }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);

    const QRect outerRect = rect();
    const QRect innerRect = outerRect.adjusted(mFrameWidth, mFrameWidth, -mFrameWidth, -mFrameWidth);

    // Frame
    if (mFrameWidth > 0)
    {
      p.setPen(QPen(mFrameColor, mFrameWidth));
      p.setBrush(Qt::NoBrush);
      const int h = mFrameWidth / 2;
      p.drawRect(outerRect.adjusted(h, h, -h, -h));
    }

    // Background
    p.fillRect(innerRect, Qt::transparent);

    const int span = maximum() - minimum();
    if (span <= 0 || innerRect.isEmpty()) return;

    const int v = qBound(minimum(), value(), maximum());
    const int o = qBound(minimum(), mOrigin, maximum());

    auto toPixel = [&](int val) -> int
    {
      return innerRect.left() + (val - minimum()) * innerRect.width() / span;
    };

    p.setPen(Qt::NoPen);
    p.setBrush(mFillBrush);

    const int pxOrigin = toPixel(o);
    const int pxValue = toPixel(v);

    if (v >= o)
    {
      // Normal: fill from origin to value
      if (pxValue > pxOrigin)
      {
        p.drawRect(QRect(pxOrigin, innerRect.top(), pxValue - pxOrigin, innerRect.height()));
      }
    }
    else
    {
      // Gap mode: fill [min..value] and [origin..max], leave gap in between
      if (pxValue > innerRect.left())
      {
        p.drawRect(QRect(innerRect.left(), innerRect.top(), pxValue - innerRect.left(), innerRect.height()));
      }

      if (pxOrigin < innerRect.right())
      {
        p.drawRect(QRect(pxOrigin, innerRect.top(), innerRect.right() - pxOrigin + 1, innerRect.height()));
      }
    }
  }

private:
  int mOrigin = 0;
  QBrush mFillBrush = QColor(26, 95, 180);
  QColor mFrameColor = QColor(80, 80, 80);
  int mFrameWidth = 0;
};
