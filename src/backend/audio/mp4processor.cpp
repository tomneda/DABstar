/*
 *    Copyright (C) 2013
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
 ************************************************************************
 *	may 15 2015. A real improvement on the code
 *	is the addition from Stefan Poeschel to create a
 *	header for the aac that matches, really a big help!!!!
 *
 *	(2019:)Furthermore, the code in the "build_aacFile" function is
 *	his as well. Chapeau!
 ************************************************************************
 */
#include  "mp4processor.h"
#include  "dabradio.h"
#include  <cstring>
#include  "charsets.h"
#include  "pad-handler.h"
#include  "bitWriter.h"
#include  "data_manip_and_checks.h"

// #define SHOW_ERROR_STATISTICS

/**
  *	\class mp4Processor is the main handler for the aac frames
  *	the class proper processes input and extracts the aac frames
  *	that are processed by the "faadDecoder" class
  */
Mp4Processor::Mp4Processor(DabRadio * iRI, const i16 iBitRate, RingBuffer<i16> * const iopAudioBuffer, RingBuffer<u8> * const iopFrameBuffer, FILE * const ipDumpFile)
  : mpRadioInterface(iRI)
  , mPadhandler(iRI)
  , mpDumpFile(ipDumpFile)
  , mBitRate(iBitRate)
  , mpFrameBuffer(iopFrameBuffer)  // input rate
  , mRsDims(iBitRate / 8)
  , mRsDecoder(8, 0435, 0, 1, 10)
{
  connect(this, &Mp4Processor::signal_show_frame_errors, iRI, &DabRadio::slot_show_frame_errors);
  connect(this, &Mp4Processor::signal_show_rs_errors, iRI, &DabRadio::slot_show_rs_errors);
  connect(this, &Mp4Processor::signal_show_aac_errors, iRI, &DabRadio::slot_show_aac_errors);
  connect(this, &Mp4Processor::signal_is_stereo, iRI, &DabRadio::slot_set_stereo);
  connect(this, &Mp4Processor::signal_new_frame, iRI, &DabRadio::slot_new_frame);
  connect(this, &Mp4Processor::signal_show_rs_corrections, iRI, &DabRadio::slot_show_rs_corrections);

#ifdef  __WITH_FDK_AAC__
  aacDecoder = new FdkAAC(iRI, iopAudioBuffer);
#else
  aacDecoder = new faadDecoder(iRI, iopAudioBuffer);
#endif

  mSuperFrameSize = 110 * (iBitRate / 8);
  mFrameByteVec.resize(mRsDims * 120);  // input
  mOutVec.resize(mRsDims * 110);
}

Mp4Processor::~Mp4Processor()
{
  delete aacDecoder;
}

/**
  *	\brief addtoFrame
  *
  *	a DAB+ superframe consists of 5 consecutive DAB frames
  *	we add vector for vector to the superframe. Once we have
  *	5 lengths of "old" frames, we check.
  *	Note that the packing in the entry vector is still one bit
  *	per Byte, nbits is the number of Bits (i.e. containing bytes)
  *	the function adds nbits bits, packed in bytes, to the frame.
  */
void Mp4Processor::add_to_frame(const std::vector<u8> & iV)
{
  const i16 numBits = 24 * mBitRate;
  const i16 numBytes = numBits / 8;
  i32 displayedErrors = mSumCorrections;

  // convert input bit-stream in iV to a byte-stream in mFrameByteVec
  for (i16 i = 0; i < numBytes; i++)
  {
    u8 temp = 0;
    for (i16 j = 0; j < 8; j++)
    {
      temp = (temp << 1) | (iV[i * 8 + j] & 0x1);
    }
    mFrameByteVec[mBlockFillIndex * numBytes + i] = temp;
  }

  mBlocksInBuffer++;
  mBlockFillIndex = (mBlockFillIndex + 1) % 5;
  mSumFrameCount++;

  // we take the last five blocks to look at
  if (mBlocksInBuffer >= 5)
  {
    // first, we show the "successrate"
    if (++mFrameCount >= 25)
    {
      mFrameCount = 0;
      emit signal_show_frame_errors(mFrameErrors);
      mFrameErrors = 0;
    }
    /**
      *	starting for real: check the fire code
      *	if the firecode is OK, we handle the frame
      *	and adjust the buffer here for the next round
      */
    if (mSuperFrameSync == 0)
    {
      if (fc.check(&mFrameByteVec[mBlockFillIndex * numBytes]))
      {
        mSuperFrameSync = 4;
      }
      else
      {
        mBlocksInBuffer = 4;
      }
    }
    // since we processed a full cycle of 5 blocks, we just start a
    // new sequence, beginning with block blockFillIndex
    if (mSuperFrameSync)
    {
      mBlocksInBuffer = 0;

      if (_process_super_frame(mFrameByteVec.data(), mBlockFillIndex * numBytes))
      {
        mSuperFrameSync = 4;

        if (++mSuccessFrames > 25)
        {
          emit signal_show_rs_errors(mRsErrors);
          mSuccessFrames = 0;
          mRsErrors = 0;
        }
      }
      else
      {
        mSuperFrameSync--;
        if (mSuperFrameSync == 0)
        {
          // we were wrong, virtual shift to left in block sizes
          mBlocksInBuffer = 4;
          mFrameErrors++;
          mSumFrameErrors++;
        }
      }
    }
  }

  if (mSumCorrections > displayedErrors)
  {
#ifdef SHOW_ERROR_STATISTICS
    fprintf(stderr, "Frames=%d, Frame Errors=%d, RS Errors=%d, RS Corrections=%d, CRC Errors=%d\n",
            mSumFrameCount * mRsDims / 5, mSumFrameErrors, mSumRsErrors, mSumCorrections, mSumCrcErrors);
#endif
    displayedErrors = mSumCorrections;
  }
}

bool Mp4Processor::_process_reed_solomon_frame(const u8 * const ipFrameBytes, const i16 iBase)
{
  /**
    *	apply reed-solomon error repair
    *	OK, what we now have is a vector with RSDims * 120 u8's
    *	the superframe, containing parity bytes for error repair
    *	take into account the interleaving that is applied.
    */
  for (i16 j = 0; j < mRsDims; j++)
  {
    std::array<u8, 120> rsIn;

    for (i16 k = 0; k < 120; k++)
    {
      rsIn[k] = ipFrameBytes[(iBase + j + k * mRsDims) % (mRsDims * 120)];
    }

    std::array<u8, 110> rsOut;

    const i16 ler = mRsDecoder.dec(rsIn.data(), rsOut.data(), 135);  // i7-6700K: ~28us

    if (ler < 0)
    {
      mRsErrors++;
      mSumRsErrors ++;
      //fprintf (stderr, "RS failure %d\n", mRsErrors);
      if (!fc.check(&ipFrameBytes[iBase]) && j == 0)
      {
        //fprintf (stderr, "Firecode after RS failure\n");
        return false;
      }
    }
    else
    {
      mTotalCorrections += ler;
      mSumCorrections += ler;
      mGoodFrames++;
      if (mGoodFrames >= 100)
      {
        emit signal_show_rs_corrections(mTotalCorrections, mCrcErrors);
        mTotalCorrections = 0;
        mGoodFrames = 0;
        mCrcErrors = 0;
      }
    }

    for (i16 k = 0; k < 110; k++)
    {
      mOutVec[j + k * mRsDims] = rsOut[k];
    }
  }
  return true;
}

/**
  *	\brief processSuperframe
  *
  *	First, we know the firecode checker gave green light
  *	We correct the errors using RS
  */
bool Mp4Processor::_process_super_frame(u8 ipFrameBytes[], const i16 iBase)
{
  if (!_process_reed_solomon_frame(ipFrameBytes, iBase)) return false; // fills mOutVec

  // bits 0 .. 15 is firecode
  // bit 16 is unused
  stream_parms streamParameters;
  streamParameters.dacRate = (mOutVec[2] >> 6) & 01;  // bit 17
  streamParameters.sbrFlag = (mOutVec[2] >> 5) & 01;  // bit 18
  streamParameters.aacChannelMode = (mOutVec[2] >> 4) & 01;  // bit 19, 0:mono, 1:stereo
  streamParameters.psFlag = (mOutVec[2] >> 3) & 01;   // bit 20
  streamParameters.mpegSurround = (mOutVec[2] & 07);  // bits 21 .. 23

  // added for the aac file writer
  streamParameters.CoreSrIndex = streamParameters.dacRate ? (streamParameters.sbrFlag ? 6 : 3) : (streamParameters.sbrFlag ? 8 : 5);
  streamParameters.CoreChConfig = streamParameters.aacChannelMode ? 2 : 1;

  streamParameters.ExtensionSrIndex = streamParameters.dacRate ? 3 : 5;

  u8 numAUs;

  switch (2 * streamParameters.dacRate + streamParameters.sbrFlag)
  {
  case 0: numAUs = 4;
    mAuStartArr[0] = 8;
    mAuStartArr[1] = mOutVec[3] * 16 + (mOutVec[4] >> 4);
    mAuStartArr[2] = (mOutVec[4] & 0xf) * 256 + mOutVec[5];
    mAuStartArr[3] = mOutVec[6] * 16 + (mOutVec[7] >> 4);
    mAuStartArr[4] = 110 * (mBitRate / 8);
    break;
  //
  case 1: numAUs = 2;
    mAuStartArr[0] = 5;
    mAuStartArr[1] = mOutVec[3] * 16 + (mOutVec[4] >> 4);
    mAuStartArr[2] = 110 * (mBitRate / 8);
    break;
  //
  case 2: numAUs = 6;
    mAuStartArr[0] = 11;
    mAuStartArr[1] = mOutVec[3] * 16 + (mOutVec[4] >> 4);
    mAuStartArr[2] = (mOutVec[4] & 0xf) * 256 + mOutVec[5];
    mAuStartArr[3] = mOutVec[6] * 16 + (mOutVec[7] >> 4);
    mAuStartArr[4] = (mOutVec[7] & 0xf) * 256 + mOutVec[8];
    mAuStartArr[5] = mOutVec[9] * 16 + (mOutVec[10] >> 4);
    mAuStartArr[6] = 110 * (mBitRate / 8);
    break;
  //
  case 3: numAUs = 3;
    mAuStartArr[0] = 6;
    mAuStartArr[1] = mOutVec[3] * 16 + (mOutVec[4] >> 4);
    mAuStartArr[2] = (mOutVec[4] & 0xf) * 256 + mOutVec[5];
    mAuStartArr[3] = 110 * (mBitRate / 8);
    break;
  default: assert(false);
  }
  /**
    *	OK, the result is N * 110 * 8 bits (still single bit per byte!!!)
    *	extract the AU's, and prepare a buffer,  with the sufficient
    *	lengthy for conversion to PCM samples
    */
  for (i16 i = 0; i < numAUs; i++)
  {
    ///	sanity check 1
    if (mAuStartArr[i + 1] < mAuStartArr[i])
    {
      fprintf(stderr, "error: invalid AU address sequence: mAuStartArr[i] %d, mAuStartArr[i + 1] %d, i %d, numAUs %d\n", mAuStartArr[i], mAuStartArr[i + 1], i, numAUs);
      //	should not happen, all errors were corrected
      return false;
    }

    const i16 aac_frame_length = mAuStartArr[i + 1] - mAuStartArr[i] - 2;
    //	just a sanity check
    if ((aac_frame_length >= 960) || (aac_frame_length < 0))
    {
      fprintf(stderr, "error: invalid aac_frame_length = %d\n", aac_frame_length);
      return false;
    }

    //	but first the crc check
    if (check_crc_bytes(&mOutVec[mAuStartArr[i]], aac_frame_length))
    {
      //	first prepare dumping
      std::vector<u8> fileBuffer;
      const i32 segmentSize = _build_aac_file(aac_frame_length, &streamParameters, &(mOutVec[mAuStartArr[i]]), fileBuffer);

      if (mpDumpFile == nullptr)
      {
        mpFrameBuffer->put_data_into_ring_buffer(fileBuffer.data(), segmentSize);
        emit signal_new_frame(segmentSize);
      }
      else
      {
        fwrite(fileBuffer.data(), 1, segmentSize, mpDumpFile);
      }

      //	first handle the pad data if any
      if (mpDumpFile == nullptr)
      {
        if (((mOutVec[mAuStartArr[i + 0]] >> 5) & 07) == 4)
        {
          const i16 count = mOutVec[mAuStartArr[i] + 1];
          auto * const buffer = make_vla(u8, count);
          memcpy(buffer, &mOutVec[mAuStartArr[i] + 2], count);
          const u8 L0 = buffer[count - 1];
          const u8 L1 = buffer[count - 2];
          mPadhandler.process_PAD(buffer, count - 3, L1, L0);
        }

        //	then handle the audio
#ifdef  __WITH_FDK_AAC__
        const i32 tmp = aacDecoder->convert_mp4_to_pcm(&streamParameters, fileBuffer.data(), segmentSize);
#else
        u8 theAudioUnit[2 * 960 + 10];  // sure, large enough

        memcpy(theAudioUnit, &mOutVec[mAuStartArr[i]], aac_frame_length);
        memset(&theAudioUnit[aac_frame_length], 0, 10);

        const i32 tmp = aacDecoder->convert_mp4_to_pcm(&streamParameters, theAudioUnit, aac_frame_length);
#endif
        emit signal_is_stereo((streamParameters.aacChannelMode == 1) || (streamParameters.psFlag == 1));

        if (tmp <= 0)
        {
          fprintf(stderr, "MP4 decoding error: %d\n", tmp);
          mAacErrors++;
        }

        if (++mAacFrames > 25)
        {
          emit signal_show_aac_errors(mAacErrors);
          mAacErrors = 0;
          mAacFrames = 0;
        }
      }
    }
    else
    {
      mCrcErrors++;
      mSumCrcErrors++;
    }
    //
    //	what would happen if the errors were in the 10 parity bytes
    //	rather than in the 110 payload bytes?
  }
  return true;
}

i32 Mp4Processor::_build_aac_file(i16 aac_frame_len, stream_parms * sp, u8 * data, std::vector<u8> & fileBuffer)
{
  BitWriter au_bw(fileBuffer); // fileBuffer will filled up in BitWriter

  au_bw.AddBits(0x2B7, 11);  // syncword
  au_bw.AddBits(0, 13);  // audioMuxLengthBytes - written later
  //	AudioMuxElement(1)

  au_bw.AddBits(0, 1);  // useSameStreamMux
  //	StreamMuxConfig()

  au_bw.AddBits(0, 1);  // audioMuxVersion
  au_bw.AddBits(1, 1);  // allStreamsSameTimeFraming
  au_bw.AddBits(0, 6);  // numSubFrames
  au_bw.AddBits(0, 4);  // numProgram
  au_bw.AddBits(0, 3);  // numLayer

  if (sp->sbrFlag)
  {
    au_bw.AddBits(0b00101, 5); // SBR
    au_bw.AddBits(sp->CoreSrIndex, 4); // samplingFrequencyIndex
    au_bw.AddBits(sp->CoreChConfig, 4); // channelConfiguration
    au_bw.AddBits(sp->ExtensionSrIndex, 4);  // extensionSamplingFrequencyIndex
    au_bw.AddBits(0b00010, 5);    // AAC LC
    au_bw.AddBits(0b100, 3);              // GASpecificConfig() with 960 transform
  }
  else
  {
    au_bw.AddBits(0b00010, 5); // AAC LC
    au_bw.AddBits(sp->CoreSrIndex, 4); // samplingFrequencyIndex
    au_bw.AddBits(sp->CoreChConfig, 4); // channelConfiguration
    au_bw.AddBits(0b100, 3);              // GASpecificConfig() with 960 transform
  }

  au_bw.AddBits(0b000, 3);  // frameLengthType
  au_bw.AddBits(0xFF, 8);  // latmBufferFullness
  au_bw.AddBits(0, 1);  // otherDataPresent
  au_bw.AddBits(0, 1);  // crcCheckPresent

  //	PayloadLengthInfo()
  for (size_t i = 0; i < (size_t)(aac_frame_len / 255); i++)
  {
    au_bw.AddBits(0xFF, 8);
  }
  au_bw.AddBits(aac_frame_len % 255, 8);

  au_bw.AddBytes(data, aac_frame_len);
  au_bw.WriteAudioMuxLengthBytes();

  return fileBuffer.size();
}
