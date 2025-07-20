/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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
#include  "dabradio.h"
#include  "protTables.h"
#include  "data_manip_and_checks.h"
#include  <cassert>

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

FicHandler::FicHandler(DabRadio * const iMr)
  : FibDecoder(iMr)
{
  std::array<std::byte, 9> shiftRegister;
  std::fill(shiftRegister.begin(), shiftRegister.end(), static_cast<std::byte>(1));

  for (i16 i = 0; i < 768; i++)
  {
    mPRBS[i] = shiftRegister[8] ^ shiftRegister[4];

    for (i16 j = 8; j > 0; j--)
    {
      shiftRegister[j] = shiftRegister[j - 1];
    }

    shiftRegister[0] = mPRBS[i];
  }

  //	Since the depuncturing is the same throughout all calls
  //	(even through all instances, so we could create a static
  //	table), we make an punctureTable that contains the indices of
  //	the ofdmInput table
  i32 local = 0;

  for (i16 i = 0; i < 21; i++)
  {
    for (i16 k = 0; k < 32 * 4; k++)
    {
      if (get_PCodes(16 - 1)[k % 32] != 0)
      {
        mPunctureTable[local] = true;
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
  for (i16 i = 0; i < 3; i++)
  {
    for (i16 k = 0; k < 32 * 4; k++)
    {
      if (get_PCodes(15 - 1)[k % 32] != 0)
      {
        mPunctureTable[local] = true;
      }
      local++;
    }
  }

  /**
    *	we have a final block of 24 bits  with puncturing according to PI_X
    *	This block constitues the 6 * 4 bits of the register itself.
    */
  for (i16 k = 0; k < 24; k++)
  {
    if (get_PCodes(8 - 1)[k] != 0)
    {
      mPunctureTable[local] = true;
    }
    local++;
  }

  connect(this, &FicHandler::show_fic_success, iMr, &DabRadio::slot_show_fic_success);
  connect(this, &FicHandler::show_fic_BER, iMr, &DabRadio::slot_show_fic_ber);
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
void FicHandler::process_block(const std::vector<i16> & iData, const i32 iBlkNo)
{
  if (iBlkNo == 1)
  {
    mIndex = 0;
    mFicNo = 0;
  }

  if (1 <= iBlkNo && iBlkNo <= 3)
  {
    for (i32 i = 0; i < 2 * cK; i++)
    {
      mOfdmInput[mIndex] = iData[i];
      ++mIndex;

      if (mIndex >= cFicSize)
      {
        _process_fic_input(mFicNo, mFicValid[mFicNo]);
        mIndex = 0;
        mFicNo++;
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
void FicHandler::_process_fic_input(const i16 iFicNo, bool & oValid)
{
  assert(iFicNo >= 0 && iFicNo < 4 && "Invalid FIC number");

  std::array<i16, 3072 + 24> viterbiBlock;
  i16 inputCount = 0;

  if (!mIsRunning.load())
  {
    return;
  }

  for (i16 i = 0; i < 3072 + 24; i++)
  {
    if (mPunctureTable[i])
    {
      viterbiBlock[i] = mOfdmInput[inputCount];
      ++inputCount;
    }
    else
    {
      viterbiBlock[i] = 0;
    }
  }
  /**
    *	Now we have the full word ready for deconvolution
    *	deconvolution is according to DAB standard section 11.2
    */
  mViterbi.deconvolve(viterbiBlock.data(), reinterpret_cast<u8 *>(mBitBufferOut.data()));
  mViterbi.calculate_BER(viterbiBlock.data(), mPunctureTable.data(), reinterpret_cast<u8 *>(mBitBufferOut.data()), mFicBits, mFicErrors);

  mFicBlock++;

  if(mFicBlock == 40) // 4 blocks per frame, 10 frames per sec
  {
    emit show_fic_BER((f32)mFicErrors / (f32)mFicBits);
    //printf("framebits = %d, bits = %d, errors = %d, %e\n", 3072, fic_bits, fic_errors, (f32)fic_errors/fic_bits);
    mFicBlock = 0;
    mFicErrors /= 2;
    mFicBits /= 2;
  }

  /**
    *	if everything worked as planned, we now have a
    *	768 bit vector containing three FIB's
    *
    *	first step: energy dispersal according to the DAB standard
    *	We use a predefined vector PRBS
    */
  for (i16 i = 0; i < 768; i++)
  {
    mBitBufferOut[i] ^= mPRBS[i];
  }

  for (i16 i = 0; i < 768; i++)
  {
    mFibBits[iFicNo * 768 + i] = mBitBufferOut[i];
  }
  /**
    *	each of the fib blocks is protected by a crc
    *	(we know that there are three fib blocks each time we are here)
    *	we keep track of the successrate
    *	and show that per 100 fic blocks
    *	One issue is what to do when we really believe the synchronization
    *	was lost.
    */

  oValid = true;

  for (i16 i = iFicNo * 3; i < iFicNo * 3 + 3; i++)
  {
    const std::byte * const p = &mBitBufferOut[(i % 3) * 256];

    if (!check_CRC_bits(reinterpret_cast<const u8 *>(p), 256))
    {
      oValid = false;
      emit show_fic_success(false);

      if (mFicDecodeSuccessRatio > 0)
      {
        mFicDecodeSuccessRatio--;
      }
      continue;
    }

    for (i32 j = 0; j < 32; j++)
    {
      mFicBuffer[j] = static_cast<std::byte>(0);

      for (i32 k = 0; k < 8; k++)
      {
        mFicBuffer[j] <<= 1;
        mFicBuffer[j] &= static_cast<std::byte>(0xFE);
        mFicBuffer[j] |= p[8 * j + k];
      }
    }

    mFicMutex.lock();
    if (mpFicDump != nullptr)
    {
      fwrite(mFicBuffer.data(), 1, 32, mpFicDump);
    }
    mFicMutex.unlock();

    emit show_fic_success(true);

    FibDecoder::process_FIB(reinterpret_cast<const u8 *>(p), iFicNo);

    if (mFicDecodeSuccessRatio < 10)
    {
      mFicDecodeSuccessRatio++;
    }
  }
}

void FicHandler::stop()
{
  disconnect_channel();
  //	clearEnsemble	();
  mIsRunning.store(false);
}

void FicHandler::restart()
{
  //	clearEnsemble	();
  mFicDecodeSuccessRatio = 0;
  connect_channel();
  mIsRunning.store(true);
}

void FicHandler::start_fic_dump(FILE * f)
{
  if (mpFicDump != nullptr)
  {
    return;
  }
  mpFicDump = f;
}

void FicHandler::stop_fic_dump()
{
  mFicMutex.lock();
  mpFicDump = nullptr;
  mFicMutex.unlock();
}


void FicHandler::get_fib_bits(u8 * v, bool * b)
{
  for (i32 i = 0; i < 4 * 768; i++)
  {
    v[i] = static_cast<u8>(mFibBits[i]);
  }

  for (i32 i = 0; i < 4; i++)
  {
    b[i] = mFicValid[i];
  }
}

i32 FicHandler::get_fic_decode_ratio_percent()
{
    return mFicDecodeSuccessRatio * 10;
}

