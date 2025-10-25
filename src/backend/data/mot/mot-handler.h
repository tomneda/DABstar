/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2015 .. 2017
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
 */
#ifndef  MOT_HANDLER_H
#define  MOT_HANDLER_H

#include  "dab-constants.h"
#include  "virtual-datahandler.h"
#include  <vector>

class DabRadio;
class MotObject;
class MotDirectory;

class MotHandler : public VirtualDataHandler
{
public:
  explicit MotHandler(DabRadio *);
  ~MotHandler() override;

  void add_MSC_data_group(const std::vector<u8> &);
private:
  // we "cache" the most recent single motSlides (not those in a directory)
  struct SMotTable
  {
    u16 transportId;
    i32 orderNumber;
    MotObject * motSlide;
  };

  MotDirectory * mpDirectory = nullptr;
  DabRadio * const mpRadioInterface;
  int mOrderNumber = 0;
  std::array<SMotTable, 15> mMotTable; // TODO: are 55 or 15 the better number?

  void setHandle(MotObject *, u16);
  MotObject * getHandle(u16);
};

#endif

