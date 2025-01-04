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
#ifndef  MP4PROCESSOR_H
#define  MP4PROCESSOR_H
/*
 * 	Handling superframes for DAB+ and delivering
 * 	frames into the ffmpeg or faad decoding library
 */

#include  "dab-constants.h"
#include  <cstdio>
#include  <cstdint>
#include  <vector>
#include  "audio-base.h"
#include  "frame-processor.h"
#include  "firecode-checker.h"
#include  "reed-solomon.h"
#include  <QObject>
#include  "pad-handler.h"

#ifdef  __WITH_FDK_AAC__
#include	"fdk-aac.h"
#else

#include  "faad-decoder.h"

#endif

class RadioInterface;

class Mp4Processor : public QObject, public FrameProcessor
{
Q_OBJECT
public:
  Mp4Processor(RadioInterface *, int16_t, RingBuffer<int16_t> *, RingBuffer<uint8_t> *, FILE *);
  ~Mp4Processor() override;

  void addtoFrame(const std::vector<uint8_t> &) override;
  
private:
  RadioInterface * myRadioInterface;
  PadHandler mPadhandler;
  bool processSuperframe(uint8_t [], int16_t);
  int build_aacFile(int16_t aac_frame_len, stream_parms * sp, uint8_t * data, std::vector<uint8_t> & fileBuffer);

  FILE * mpDumpFile = nullptr;
  int16_t mSuperFrameSize;
  int16_t mBlockFillIndex = 0;
  int16_t mBlocksInBuffer = 0;
  int16_t mFrameCount = 0;
  int16_t mFrameErrors = 0;
  int16_t mRsErrors = 0;
  int16_t mCrcErrors = 0;
  int16_t mAacErrors = 0;
  int16_t mAacFrames = 0;
  int16_t mSuccessFrames = 0;
  int32_t mSuperFrameSync = 0;
  int32_t mGoodFrames = 0;
  int32_t mTotalCorrections = 0;
  int16_t bitRate;
  RingBuffer<uint8_t> * mpFrameBuffer = nullptr;
  std::vector<uint8_t> mFrameByteVec;
  std::vector<uint8_t> mOutVec;
  const int16_t mRsDims;
  std::array<int16_t, 10> mAuStartArr;
  FirecodeChecker fc;
  ReedSolomon mRsDecoder;
#ifdef  __WITH_FDK_AAC__
  FdkAAC		*aacDecoder;
#else
  faadDecoder * aacDecoder;
#endif

signals:
  void signal_show_frame_errors(int);
  void signal_show_rs_errors(int);
  void signal_show_aac_errors(int);
  void signal_is_stereo(bool);
  void signal_new_frame(int);
  void signal_show_rs_corrections(int, int);
};

#endif


