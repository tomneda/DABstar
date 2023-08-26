/*
 *    Copyright (C) 2016
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
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

#include  "fic-handler.h"
#include  "radio.h"
#include  "protTables.h"
#include  "data_manip_and_checks.h"

//	The 3072 bits of the serial motherword shall be split into
//	24 blocks of 128 bits each.
//	The first 21 blocks shall be subjected to
//	puncturing (per 32 bits) according to PI_16
//	The next three blocks shall be subjected to 
//	puncturing (per 32 bits) according to PI_15
//	The last 24 bits shall be subjected to puncturing
//	according to the table 8

/**
  *	\class FicHandler
  * 	We get in - through process_ficBlock - the FIC data
  * 	in units of 768 bits.
  * 	We follow the standard and apply convolution decoding and
  * 	puncturing.
  *	The data is sent through to the fib processor
  */

FicHandler::FicHandler(RadioInterface * const iMr, const uint8_t iDabMode) :
  FibDecoder(iMr),
  params(iDabMode)
{
  std::array<std::byte, 9> shiftRegister;
  std::fill(shiftRegister.begin(), shiftRegister.end(), static_cast<std::byte>(1));

  BitsperBlock = 2 * params.get_K();

  for (int16_t i = 0; i < 768; i++)
  {
    PRBS[i] = shiftRegister[8] ^ shiftRegister[4];

    for (int16_t j = 8; j > 0; j--)
    {
      shiftRegister[j] = shiftRegister[j - 1];
    }

    shiftRegister[0] = PRBS[i];
  }

  //	Since the depuncturing is the same throughout all calls
  //	(even through all instances, so we could create a static
  //	table), we make an punctureTable that contains the indices of
  //	the ofdmInput table
  int local = 0;

  for (int16_t i = 0; i < 21; i++)
  {
    for (int16_t k = 0; k < 32 * 4; k++)
    {
      if (get_PCodes(16 - 1)[k % 32] != 0)
      {
        punctureTable[local] = true;
      }
      local++;
    }
  }
  /**
    *	In the second step
    *	we have 3 blocks with puncturing according to PI_15
    *	each 128 bit block contains 4 subblocks of 32 bits
    *	on which the given puncturing is applied
    */
  for (int16_t i = 0; i < 3; i++)
  {
    for (int16_t k = 0; k < 32 * 4; k++)
    {
      if (get_PCodes(15 - 1)[k % 32] != 0)
      {
        punctureTable[local] = true;
      }
      local++;
    }
  }

  /**
    *	we have a final block of 24 bits  with puncturing according to PI_X
    *	This block constitues the 6 * 4 bits of the register itself.
    */
  for (int16_t k = 0; k < 24; k++)
  {
    if (get_PCodes(8 - 1)[k] != 0)
    {
      punctureTable[local] = true;
    }
    local++;
  }

  connect(this, SIGNAL (show_ficSuccess(bool)), iMr, SLOT (show_ficSuccess(bool)));
}

/**
  *	\brief process_ficBlock
  *	The number of bits to be processed per incoming block
  *	is 2 * p -> K, which still depends on the Mode.
  *	for Mode I it is 2 * 1536, for Mode II, it is 2 * 384,
  *	for Mode III it is 192, Mode IV gives 2 * 768.
  *	for Mode II we will get the 2304 bits after having read
  *	the 3 FIC blocks, each with 768 bits.
  *	for Mode IV we will get 3 * 2 * 768 = 4608, i.e. two resulting blocks
  *	Note that Mode III is NOT supported
  *	
  *	The function is called with a blkno. This should be 1, 2 or 3
  *	for each time 2304 bits are in, we call process_ficInput
  */
void FicHandler::process_ficBlock(const std::vector<int16_t> & iData, const int32_t iBlkNo)
{
  if (iBlkNo == 1)
  {
    index = 0;
    ficno = 0;
  }
  //
  if ((1 <= iBlkNo) && (iBlkNo <= 3))
  {
    for (int32_t i = 0; i < BitsperBlock; i++)
    {
      ofdm_input[index] = iData[i];
      ++index;

      if (index >= 2304)
      {
        process_ficInput(ficno, &ficValid[ficno]);
        index = 0;
        ficno++;
      }
    }
  }
  else
  {
    fprintf(stderr, "You should not call ficBlock here\n");
  }
  //	we are pretty sure now that after block 4, we end up
  //	with index = 0
}

/**
  *	\brief process_ficInput
  *	we have a vector of 2304 (0 .. 2303) soft bits that has
  *	to be de-punctured and de-conv-ed into a block of 768 bits
  *	In this approach we first create the full 3072 block (i.e.
  *	we first depuncture, and then we apply the deconvolution
  *	In the next coding step, we will combine this function with the
  *	one above
  */
void FicHandler::process_ficInput(const int16_t iFicNo, bool * oValid)
{
  std::array<int16_t, 3072 + 24> viterbiBlock;
  int16_t inputCount = 0;

  if (!running.load())
  {
    return;
  }
  //	memset (viterbiBlock, 0, (3072 + 24) * sizeof (int16_t));

  for (int16_t i = 0; i < 3072 + 24; i++)
  {
    if (punctureTable[i])
    {
      viterbiBlock[i] = ofdm_input[inputCount];
      ++inputCount;
    }
    else
    {
      viterbiBlock[i] = 0; // TODO: this was missing, is it not necessary to set this to zero?
    }
  }
  /**
    *	Now we have the full word ready for deconvolution
    *	deconvolution is according to DAB standard section 11.2
    */
  myViterbi.deconvolve(viterbiBlock.data(), reinterpret_cast<uint8_t *>(bitBuffer_out.data()));
  /**
    *	if everything worked as planned, we now have a
    *	768 bit vector containing three FIB's
    *
    *	first step: energy dispersal according to the DAB standard
    *	We use a predefined vector PRBS
    */
  for (int16_t i = 0; i < 768; i++)
  {
    bitBuffer_out[i] ^= PRBS[i];
  }

  for (int16_t i = 0; i < 768; i++)
  {
    fibBits[iFicNo * 768 + i] = bitBuffer_out[i];
  }
  /**
    *	each of the fib blocks is protected by a crc
    *	(we know that there are three fib blocks each time we are here)
    *	we keep track of the successrate
    *	and show that per 100 fic blocks
    *	One issue is what to do when we really believe the synchronization
    *	was lost.
    */

  *oValid = true;
  for (int16_t i = iFicNo * 3; i < iFicNo * 3 + 3; i++)
  {
    const std::byte * const p = &bitBuffer_out[(i % 3) * 256];
    
    if (!check_CRC_bits(reinterpret_cast<const uint8_t *>(p), 256))
    {
      *oValid = false;
      emit show_ficSuccess(false);
      continue;
    }

    for (int j = 0; j < 32; j++)
    {
      ficBuffer[j] = static_cast<std::byte>(0);
      for (int k = 0; k < 8; k++)
      {
        ficBuffer[j] <<= 1;
        ficBuffer[j] &= static_cast<std::byte>(0xFE);
        ficBuffer[j] |= p[8 * j + k];
      }
    }

    ficLocker.lock();
    if (ficDumpPointer != nullptr)
    {
      fwrite(ficBuffer.data(), 1, 32, ficDumpPointer);
    }
    ficLocker.unlock();

    emit show_ficSuccess(true);
    FibDecoder::process_FIB(reinterpret_cast<const uint8_t *>(p), iFicNo);
  }
}

void FicHandler::stop()
{
  disconnect_channel();
  //	clearEnsemble	();
  running.store(false);
}

void FicHandler::restart()
{
  //	clearEnsemble	();
  connect_channel();
  running.store(true);
}

void FicHandler::start_ficDump(FILE * f)
{
  if (ficDumpPointer != nullptr)
  {
    return;
  }
  ficDumpPointer = f;
}

void FicHandler::stop_ficDump()
{
  ficLocker.lock();
  ficDumpPointer = nullptr;
  ficLocker.unlock();
}


void FicHandler::get_fibBits(uint8_t * v, bool * b)
{
  for (int i = 0; i < 4 * 768; i++)
  {
    v[i] = static_cast<uint8_t>(fibBits[i]);
  }

  for (int i = 0; i < 4; i++)
  {
    b[i] = ficValid[i];
  }
}
