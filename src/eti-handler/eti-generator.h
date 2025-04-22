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

class IDabRadio;

class parameter;

//	to build a simple cache for the protection handlers
typedef struct
{
  bool uepFlag;
  int bitRate;
  int protLevel;
  uint8_t * dispersionVector;
  Protection * theDeconvolver;
} protDesc;

class etiGenerator
{
public:
  etiGenerator(FicHandler * my_ficHandler);
  ~etiGenerator();

  void newFrame();
  void process_block(const std::vector<int16_t> & fbits, int blkno);
  void reset();
  bool start_eti_generator(const QString &);
  void stop_eti_generator();

private:
  FicHandler * my_ficHandler;
  FILE * etiFile;
  bool running;
  int16_t index_Out;
  int Minor;
  int16_t CIFCount_hi;
  int16_t CIFCount_lo;
  std::atomic<int16_t> amount;
  int16_t BitsperBlock;
  int16_t numberofblocksperCIF;
  uint8_t fibBits[4 * 768];

  int32_t _init_eti(uint8_t *, int16_t, int16_t, int16_t);
  int32_t _process_cif(const int16_t *, uint8_t *, int32_t);
  void _process_sub_channel(int, parameter *, Protection * prot, uint8_t *);

  void postProcess(const uint8_t *, int32_t);
};

#endif


