/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2020
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
 *	Use the fdk-aac library.
 */
#ifdef  __WITH_FDK_AAC__

#ifndef  FDK_AAC_H
#define  FDK_AAC_H

#include  <QObject>
#include  <stdint.h>
#include  <aacdecoder_lib.h>
#include  "ringbuffer.h"


typedef struct
{
  int rfa;
  int dacRate;
  int sbrFlag;
  int psFlag;
  int aacChannelMode;
  int mpegSurround;
  int CoreChConfig;
  int CoreSrIndex;
  int ExtensionSrIndex;
} stream_parms;

class RadioInterface;

// fdkAAC is an interface to the fdk-aac library, using the LOAS protocol
class FdkAAC : public QObject
{
  Q_OBJECT
public:
  FdkAAC(RadioInterface * mr, RingBuffer<int16_t> * iipBuffer);
  ~FdkAAC() override;

  int16_t convert_mp4_to_pcm(const stream_parms * iSP, const uint8_t * ipBuffer, int16_t iPacketLength);

private:
  RingBuffer<int16_t> * const mpAudioBuffer;
  bool mIsWorking = false;
  HANDLE_AACDECODER handle;

signals:
  void signal_new_audio(int, unsigned int, unsigned int);
};

#endif
#endif
