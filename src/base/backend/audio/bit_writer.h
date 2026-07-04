/*
    DABlin - capital DAB experience
    Copyright (C) 2015-2019 Stefan Pöschel

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// --- BitWriter ------------------------------------------------------------
#pragma once

#include "glob_data_types.h"
#include <cstddef>
#include <vector>

class BitWriter
{
public:
  explicit BitWriter(std::vector<u8> & oData)
    : data(oData)
  {
    Reset();
  }

  void Reset();
  void AddBits(i32 data_new, size_t count);
  void AddBytes(const u8 * data, size_t len);

  void WriteAudioMuxLengthBytes();        // needed for LATM

private:
  std::vector<u8> & data; // use a reference to avoid copying outside of this class
  size_t byte_bits;
};
