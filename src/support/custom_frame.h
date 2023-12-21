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

#ifndef DABSTAR_CUSTOM_FRAME_H
#define DABSTAR_CUSTOM_FRAME_H

#include <QObject>
#include <QCloseEvent>
#include <QFrame>

class CustomFrame : public QFrame
{
  using QFrame::QFrame;
  Q_OBJECT

protected:
  void closeEvent(QCloseEvent * event) override
  {
    emit signal_frame_closed();
    QFrame::closeEvent(event);
  }

  signals:
    void signal_frame_closed();
};

#endif //DABSTAR_CUSTOM_FRAME_H
