/*
* This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C)  2016 .. 2023
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB.
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

#pragma once

#include "glob_defs.h"
#include "wav_writer.h"
#include <mutex>
#include <xml-filewriter.h>
#include "newconverter.h"

class converter_48000
{
public:
  converter_48000();
  ~converter_48000() = default;

  int convert(const cmplx16 *, int32_t, int,std::vector<float> &);

  void start_audioDump(const QString &);
  void stop_audioDump();

private:
  newConverter mapper_16{16000, 48000, 2 * 1600};
  newConverter mapper_24{24000, 48000, 2 * 2400};
  newConverter mapper_32{32000, 48000, 2 * 3200};
  std::mutex locker;
  wavWriter mWavWriter;

  int convert_16000(const cmplx16 *, int, std::vector<float> &);
  int convert_24000(const cmplx16 *, int, std::vector<float> &);
  int convert_32000(const cmplx16 *, int, std::vector<float> &);
  int convert_48000(const cmplx16 *, int, std::vector<float> &);

  void dump(const cmplx *, int);
  void dump(const cmplx16 *, int);
};
