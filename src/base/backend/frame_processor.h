/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2010 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
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
#pragma once

#include "glob_data_types.h"
#include <vector>
#include <cstdio>

// Decodes one of the last logical frames of a subchannel again, with
// known bytes (iterative DAB+ decoding: the bytes of the correct
// Reed-Solomon codewords help the Viterbi decoder with the others)
class FrameRedecoder
{
  public:
    virtual ~FrameRedecoder() = default;
    // age: 0 = the last frame given to addtoFrame(), 1 = the one before.
    // known: per byte of the frame 1 if bytes[i] is known.
    // bytes: in: the known bytes, out: the frame decoded again.
    // false if the frame is not available any more.
    virtual bool redecodeFrame(i32 age, const u8 *known, u8 *bytes) = 0;
};

// Virtual class, just for providing a common base for the real decoder classes
class FrameProcessor
{
public:
  FrameProcessor() = default;
  virtual ~FrameProcessor() = default;

  virtual void add_to_frame(const std::vector<u8>&)
  {
    fprintf(stderr, "missing overridden method in FrameProcessor\n");
  }

  // For decoders that can use it (DAB+)
  virtual void setFrameRedecoder(FrameRedecoder *) {}
};
