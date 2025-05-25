/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 * ## Copyright
 *
 * dabtools is written by Dave Chapman <dave@dchapman.com> 
 *
 * Large parts of the code are copied verbatim (or with trivial
 * modifications) from David Crawley's OpenDAB and hence retain his
 * copyright.
 *
 *	Parts of this software are copied verbatim (or with trivial
 *	Modifications) from David Chapman's dabtools and hence retain
 *	his copyright. In particular, the crc, descramble and init_eti
 *	functions are - apart from naming - a verbatim copy. Thanks
 *	for the nice work
 *
 *    Copyright (C) 2016, 2023
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the eti library
 *    eti library is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    eti library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with eti library; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 	eti generator
 */

#ifndef  ETI_GENERATOR_H
#define  ETI_GENERATOR_H

#include  <cstdio>
#include  <stdint.h>
#include  <cstdio>
#include  <vector>
#include  <atomic>
#include  "dab-constants.h"
#include  "ringbuffer.h"
#include  "fic-handler.h"
#include  "protection.h"

class DabRadio;

class parameter;

//	to build a simple cache for the protection handlers
typedef struct
{
  bool uepFlag;
  int bitRate;
  int protLevel;
  u8 * dispersionVector;
  Protection * theDeconvolver;
} protDesc;

class etiGenerator
{
public:
  etiGenerator(FicHandler * my_ficHandler);
  ~etiGenerator();

  void newFrame();
  void process_block(const std::vector<i16> & fbits, int blkno);
  void reset();
  bool start_eti_generator(const QString &);
  void stop_eti_generator();

private:
  FicHandler * my_ficHandler;
  FILE * etiFile;
  bool running;
  i16 index_Out;
  int Minor;
  i16 CIFCount_hi;
  i16 CIFCount_lo;
  std::atomic<i16> amount;
  i16 BitsperBlock;
  i16 numberofblocksperCIF;
  u8 fibBits[4 * 768];

  i32 _init_eti(u8 *, i16, i16, i16);
  i32 _process_cif(const i16 *, u8 *, i32);
  void _process_sub_channel(int, parameter *, Protection * prot, u8 *);

  void postProcess(const u8 *, i32);
};

#endif


