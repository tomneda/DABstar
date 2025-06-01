/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
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
#ifndef FAAD_DECODER_H
#define FAAD_DECODER_H

#include        <QObject>
#include        "neaacdec.h"
#include        "ringbuffer.h"

class DabRadio;

struct SStreamParms
{
  i32 rfa;
  i32 dacRate;
  i32 sbrFlag;
  i32 psFlag;
  i32 aacChannelMode;
  i32 mpegSurround;
  i32 CoreChConfig;
  i32 CoreSrIndex;
  i32 ExtensionSrIndex;
};


class faadDecoder : public QObject
{
Q_OBJECT
public:
  faadDecoder(DabRadio * mr, RingBuffer<i16> * buffer);
  ~faadDecoder();

  i16 convert_mp4_to_pcm(const SStreamParms * iSP, const u8 * ipBuffer, i16 iBufferLength);

private:
  bool initialize(const SStreamParms *);

  bool processorOK;
  bool aacInitialized;
  u32 aacCap;
  NeAACDecHandle aacHandle;
  NeAACDecConfigurationPtr aacConf;
  NeAACDecFrameInfo hInfo;
  i32 baudRate;
  RingBuffer<i16> * audioBuffer;

signals:
  void signal_new_audio(i32, u32, u32);
};

#endif
