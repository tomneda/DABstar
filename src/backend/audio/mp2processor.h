/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/******************************************************************************
** kjmp2 -- a minimal MPEG-1 Audio Layer II decoder library                  **
*******************************************************************************
** Copyright (C) 2006 Martin J. Fiedler <martin.fiedler@gmx.net>             **
**                                                                           **
** This software is provided 'as-is', without any express or implied         **
** warranty. In no event will the authors be held liable for any damages     **
** arising from the use of this software.                                    **
**                                                                           **
** Permission is granted to anyone to use this software for any purpose,     **
** including commercial applications, and to alter it and redistribute it    **
** freely, subject to the following restrictions:                            **
**   1. The origin of this software must not be misrepresented; you must not **
**      claim that you wrote the original software. If you use this software **
**      in a product, an acknowledgment in the product documentation would   **
**      be appreciated but is not required.                                  **
**   2. Altered source versions must be plainly marked as such, and must not **
**      be misrepresented as being the original software.                    **
**   3. This notice may not be removed or altered from any source            **
**      distribution.                                                        **
******************************************************************************/
//
//	This software is a rewrite of the original kjmp2 software,
//	Rewriting in the form of a class
//	for use in the sdr-j DAB/DAB+ receiver
//	all rights remain where they belong

#ifndef MP2PROCESSOR_H
#define MP2PROCESSOR_H

#include  <utility>
#include  <cstdio>
#include  <cstdint>
#include  <cmath>
#include  <QObject>
#include  <cstdio>
#include  "frame-processor.h"
#include  "ringbuffer.h"
#include  "pad-handler.h"

#define KJMP2_MAX_FRAME_SIZE    1440  // the maximum size of a frame
#define KJMP2_SAMPLES_PER_FRAME 1152  // the number of samples per frame

// quantizer specification structure
struct SQuantizerSpec
{
  i32 nlevels;
  u8 grouping;
  u8 cw_bits;
};

class DabRadio;

class Mp2Processor : public QObject, public FrameProcessor
{
Q_OBJECT
public:
  Mp2Processor(DabRadio *, i16, RingBuffer<i16> *, RingBuffer<u8> *);
  ~Mp2Processor() override;
  void add_to_frame(const std::vector<u8> &) override;

private:
  DabRadio * myRadioInterface;
  i16 bitRate;
  PadHandler my_padhandler;
  RingBuffer<i16> * const audioBuffer;
  RingBuffer<u8> * const frameBuffer;
  i32 sampleRate;
  i16 V[2][1024];
  i16 Voffs;
  i16 N[64][32];
  SQuantizerSpec * allocation[2][32];
  i32 scfsi[2][32];
  i32 scalefactor[2][32][3];
  i32 sample[2][32][3];
  i32 U[512];

  i32 frame_count = 0;
  i32 bit_window;
  i32 bits_in_window;
  const u8 * frame_pos;
  std::vector<u8> MP2frame;
  enum class ESyncState { SearchingForSync, GetSampleRate, GetData };

  ESyncState MP2SyncState = ESyncState::SearchingForSync;
  i16 MP2framesize;
  i16 MP2headerCount;
  i16 MP2bitCount;
  i16 numberofFrames;
  i16 errorFrames;

  i32 _get_mp2_sample_rate(const std::vector<u8> & iFrame);
  i32 _mp2_decode_frame(const std::vector<u8> & iFrame, i16 *);
  void _set_sample_rate(i32);
  SQuantizerSpec * _read_allocation(i32, i32);
  void _read_samples(SQuantizerSpec *, i32 iScalefactor, i32 * opSamples);
  i32 _get_bits(i32);
  void _add_bit_to_mp2(std::vector<u8> &, u8, i16);
  void _process_pad_data(const std::vector<u8> & iBits);

signals:
  void signal_show_frameErrors(i32);
  void signal_new_audio(i32, u32, u32);
  void signal_new_frame(i32);
  void signal_is_stereo(bool);
};

#endif

