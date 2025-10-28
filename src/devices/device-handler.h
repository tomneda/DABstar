/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *	We have to create a simple virtual class here, since we
 *	want the interface with different devices (including  filehandling)
 *	to be transparent
 */
#ifndef  DEVICE_HANDLER_H
#define  DEVICE_HANDLER_H

#include "glob_defs.h"
#include <cstdint>
#include <QString>

class IDeviceHandler
{
public:
  virtual ~IDeviceHandler() = default;

  virtual bool restartReader(i32 freq) = 0;
  virtual void stopReader() = 0;
  virtual void setVFOFrequency(i32) = 0;
  virtual i32 getVFOFrequency() = 0;
  virtual i32 getSamples(cf32 *, i32) = 0;
  virtual i32 Samples() = 0;
  virtual void resetBuffer() = 0;
  virtual i16 bitDepth() = 0;
  virtual void hide() = 0;
  virtual void show() = 0;
  virtual bool isHidden() = 0;
  virtual QString deviceName() = 0;
  virtual bool isFileInput() = 0;
};

#endif
