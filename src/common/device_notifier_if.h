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

#include <QObject>

// IDeviceHandler is a plain interface and cannot carry signals, so device handlers which want to notify
// DabRadio about asynchronous events derive from this QObject in addition. Only the handlers which really
// emit one of the signals below need to do so, DabRadio detects them with a dynamic_cast. As Qt allows only
// one QObject base class, this must be the first base class of the handler.
class IDeviceNotifier : public QObject
{
  Q_OBJECT
public:
  ~IDeviceNotifier() override = default;

signals:
  // A file reader wrapped around to the begin of the file.
  void signal_file_looped();

  // A network device established its server connection. This can happen long after the device was created,
  // so the channel has to be (re)started to make it deliver samples.
  void signal_device_connected();
};
