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

class DabRadio;

class Mp4Processor : public QObject, public FrameProcessor
{
Q_OBJECT
public:
  Mp4Processor(DabRadio *, i16, RingBuffer<i16> *, RingBuffer<u8> *, FILE *);
  ~Mp4Processor() override;

  void add_to_frame(const std::vector<u8> &) override;
  
private:
  DabRadio * const mpRadioInterface;
  PadHandler mPadhandler;
  FILE * const mpDumpFile;
  i16 const mBitRate;
  RingBuffer<u8> * const mpFrameBuffer;
  const i16 mRsDims;
  ReedSolomon mRsDecoder;

  i16 mSuperFrameSize;
  i16 mBlockFillIndex = 0;
  i16 mBlocksInBuffer = 0;
  i16 mFrameCount = 0;
  i16 mFrameErrors = 0;
  i16 mRsErrors = 0;
  i16 mCrcErrors = 0;
  i16 mAacErrors = 0;
  i16 mAacFrames = 0;
  i16 mSuccessFrames = 0;
  i32 mSuperFrameSync = 0;
  i32 mGoodFrames = 0;
  i32 mTotalCorrections = 0;
  i32 mSumFrameCount = 0;
  i32 mSumFrameErrors = 0;
  i32 mSumRsErrors = 0;
  i32 mSumCorrections = 0;
  i32 mSumCrcErrors = 0;
  std::vector<u8> mFrameByteVec;
  std::vector<u8> mOutVec;
  std::array<i16, 10> mAuStartArr;
  FirecodeChecker fc;
#ifdef  __WITH_FDK_AAC__
  FdkAAC		*aacDecoder;
#else
  faadDecoder * aacDecoder;
#endif

  bool _process_super_frame(u8 [], i16);
  int _build_aac_file(i16 aac_frame_len, stream_parms * sp, u8 * data, std::vector<u8> & fileBuffer);

signals:
  void signal_show_frame_errors(int);
  void signal_show_rs_errors(int);
  void signal_show_aac_errors(int);
  void signal_is_stereo(bool);
  void signal_new_frame(int);
  void signal_show_rs_corrections(int, int);
};

#endif


