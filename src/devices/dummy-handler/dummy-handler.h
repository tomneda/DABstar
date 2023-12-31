/*
 * Copyright (c) 2023 by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
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

#ifndef  DUMMY_HANDLER_H
#define  DUMMY_HANDLER_H

#include "device-handler.h"

class DummyHandler final : public IDeviceHandler
{
public:
  DummyHandler() = default;
  ~DummyHandler() override = default;

  bool restartReader(int32_t freq) override { return true; }
  void stopReader() override {}
  void setVFOFrequency(int32_t) override {}
  int32_t getVFOFrequency() override { return 0; }
  int32_t getSamples(cmplx *, int32_t) override { return 0; }
  int32_t Samples() override { return 1024; }
  void resetBuffer() override {}
  int16_t bitDepth() override { return 10; }
  void hide() override {}
  void show() override {}
  bool isHidden() override { return false; }
  QString deviceName() override { return "Dummy"; }
  bool isFileInput() override { return false; }
};

#endif

