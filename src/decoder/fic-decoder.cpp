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

#include  "fic-decoder.h"
#include  "dabradio.h"
#include  "protTables.h"
#include  "crc.h"
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
  *	\class FicDecoder
  * 	We get in - through process_ficBlock - the FIC data
  * 	in units of 768 bits.
  * 	We follow the standard and apply convolution decoding and
  * 	puncturing.
  *	The data is sent through to the fib processor
  */

FicDecoder::FicDecoder(DabRadio * const iMr)
  : mpFibDecoder(FibDecoderFactory::create(iMr))
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

  connect(this, &FicDecoder::signal_fic_status, iMr, &DabRadio::slot_show_fic_status);
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
void FicDecoder::process_block(const std::vector<i16> & iOfdmSoftBits, const i32 iOfdmSymbIdx)
{
  assert(iOfdmSoftBits.size() == c2K);
  assert(1 <= iOfdmSymbIdx && iOfdmSymbIdx <= 3); // iSymbIdx begins with zero

  if (iOfdmSymbIdx == 1) // next symbol after sync symbol
  {
    mIndex = 0;
    mFicIdx = 0;
  }

  for (i32 i = 0; i < c2K; i++)
  {
    mFicViterbiSoftInput[mIndex] = iOfdmSoftBits[i];
    ++mIndex;

    if (mIndex >= cFicSizeVitIn) // collect 1 FIC or 3 FIBs (3/4 of an OFDM symbol bits)
    {
      _process_fic_input(mFicIdx, mFicValid[mFicIdx]); // for each 2304 bits

      mIndex = 0;
      mFicIdx++;
    }
  }
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
void FicDecoder::_process_fic_input(const i16 iFicIdx, bool & oFicValid)
{
  assert(iFicIdx >= 0 && iFicIdx < 4);

  std::array<i16, cViterbiBlockSize> viterbiBlock; // Viterbi input buffer with punctuation
  i16 ficReadIdx = 0;

  if (!mIsRunning.load())
  {
    return;
  }

  for (i16 i = 0; i < cViterbiBlockSize; i++)
  {
    if (mPunctureTable[i])
    {
      assert(ficReadIdx < cFicSizeVitIn);
      viterbiBlock[i] = mFicViterbiSoftInput[ficReadIdx];
      ++ficReadIdx;
    }
    else
    {
      viterbiBlock[i] = 0;
    }
  }

  // Now we have the full word ready for deconvolution. Deconvolution is according to DAB standard section 11.2.
  auto & fibBitsOf3Fibs = *reinterpret_cast<std::array<std::byte, cFicSizeVitOut> *>(&mFibBitsEntireFrame[iFicIdx * cFicSizeVitOut]); // 3 FIBs or 1 FIC

  mViterbi.deconvolve(viterbiBlock.data(), reinterpret_cast<u8 *>(fibBitsOf3Fibs.data()));
  mViterbi.calculate_BER(viterbiBlock.data(), mPunctureTable.data(), reinterpret_cast<u8 *>(fibBitsOf3Fibs.data()), mFicBits, mFicErrors);

  mFicBlock++;

  if (mFicBlock == 40) // 4 blocks per frame, 10 frames per sec
  {
    emit signal_fic_status(mFicDecodeSuccessRatio * 10, (f32)mFicErrors / (f32)mFicBits);
    // printf("framebits = %d, bits = %d, errors = %d, %e\n", 3072, mFicBits, mFicErrors, (f32)mFicErrors/mFicBits);
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
  for (i16 i = 0; i < cFicSizeVitOut; i++)
  {
    fibBitsOf3Fibs[i] ^= mPRBS[i];
  }

  /**
    *	each of the fib blocks is protected by a crc
    *	(we know that there are three fib blocks each time we are here)
    *	we keep track of the success rate
    *	and show that per 100 fic blocks
    *	One issue is what to do when we really believe the synchronization
    *	was lost.
    */
  oFicValid = true;

  for (i16 fibIdx = 0; fibIdx < cFibPerFic; fibIdx++)
  {
    const auto & oneFib = *reinterpret_cast<std::array<std::byte, cFibSizeVitOut> *>(&fibBitsOf3Fibs[fibIdx * cFibSizeVitOut]);

    if (check_CRC_bits(reinterpret_cast<const u8 *>(oneFib.data()), cFibSizeVitOut))
    {
      if (mpFicDump != nullptr)
      {
        _dump_fib_to_file(oneFib.data());
      }

      mpFibDecoder->process_FIB(oneFib, iFicIdx);

      if (mFicDecodeSuccessRatio < 10)
      {
        mFicDecodeSuccessRatio++; // this is increased with each correct FIB
      }
    }
    else
    {
      oFicValid = false;

      if (mFicDecodeSuccessRatio > 0)
      {
        mFicDecodeSuccessRatio--;
      }
    }
  }
}

void FicDecoder::stop()
{
  mpFibDecoder->disconnect_channel();
  mIsRunning.store(false);
}

void FicDecoder::restart()
{
  mFicDecodeSuccessRatio = 0;
  mpFibDecoder->connect_channel();
  mIsRunning.store(true);
}

void FicDecoder::start_fic_dump(FILE * f)
{
  if (mpFicDump != nullptr)
  {
    return;
  }
  mpFicDump = f;
}

void FicDecoder::stop_fic_dump()
{
  mpFicDump = nullptr;
}

void FicDecoder::_dump_fib_to_file(const std::byte * const ipOneFibBits) const
{
  std::array<std::byte, cFibSizeVitOut / 8> fibByteBuffer; // FIB bits in a byte vector

  for (i32 j = 0; j < (i32)fibByteBuffer.size(); j++)
  {
    fibByteBuffer[j] = static_cast<std::byte>(0);

    for (i32 k = 0; k < 8; k++)
    {
      fibByteBuffer[j] <<= 1;
      fibByteBuffer[j] &= static_cast<std::byte>(0xFE);
      fibByteBuffer[j] |= ipOneFibBits[8 * j + k];
    }
  }

  fwrite(fibByteBuffer.data(), 1, fibByteBuffer.size(), mpFicDump);
}

void FicDecoder::get_fib_bits(u8 * v, bool * b)
{
  for (i32 i = 0; i < (i32)mFibBitsEntireFrame.size(); i++)
  {
    v[i] = static_cast<u8>(mFibBitsEntireFrame[i]);
  }

  for (i32 i = 0; i < (i32)mFicValid.size(); i++)
  {
    b[i] = mFicValid[i];
  }
}

i32 FicDecoder::get_fic_decode_ratio_percent() const
{
  return mFicDecodeSuccessRatio * 10;
}
