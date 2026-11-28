/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 * and improved by old-dab (https://github.com/old-dab)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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
 *  may 15 2015. A real improvement on the code
 *  is the addition from Stefan Poeschel to create a
 *  header for the aac that matches, really a big help!!!!
 *
 *  (2019:)Furthermore, the code in the "build_aacFile" function is
 *  his as well. Chapeau!
 ************************************************************************
 */
#include "mp4processor.h"
#include "dabradio.h"
#include "audio_pipeline.h"
#include "pad_handler.h"
#include "crc.h"
#include "bit_writer.h"
#include "setting_helper.h"
#include <cstring>

// #define SHOW_ERROR_STATISTICS

// Iterative decoding: passes of Viterbi and RS decoding per superframe
static const i32 max_redecode_passes = 3;
// Superframes without a correct fire code until the alignment is lost
static const i32 max_superframes_without_sync = 10;

/**
  * \class mp4Processor is the main handler for the aac frames
  * the class proper processes input and extracts the aac frames
  * that are processed by the "faadDecoder" class
  */
Mp4Processor::Mp4Processor(DabRadio* iRI, const i16 iBitRate, RingBuffer<i16>* const iopAudioBuffer, RingBuffer<u8>* const iopFrameBuffer)
  : mpRadioInterface(iRI)
  , mPadhandler(iRI)
  , mBitRate(iBitRate)
  , mpFrameBuffer(iopFrameBuffer)  // input rate
  , mRsDims(iBitRate / 8)
  , mRsDecoder(8, 0435, 0, 1, 10)
{
  connect(this, &Mp4Processor::signal_show_frame_errors, iRI, &DabRadio::slot_show_frame_errors);
  connect(this, &Mp4Processor::signal_show_aac_errors, iRI, &DabRadio::slot_show_aac_errors);
  connect(this, &Mp4Processor::signal_is_stereo, iRI, &DabRadio::slot_set_stereo);
  if (iRI != nullptr && iRI->get_audio_pipeline() != nullptr)
  {
    connect(this, &Mp4Processor::signal_new_aac_frame, iRI->get_audio_pipeline(), &AudioPipeline::slot_new_aac_mp2_frame);
  }
  connect(this, &Mp4Processor::signal_show_rs_corrections, iRI, &DabRadio::slot_show_rs_corrections);

#ifdef  __WITH_FDK_AAC__
  mpAacDecoder = std::make_unique<FdkAAC>(iRI, iopAudioBuffer);
#else
  mpAacDecoder = std::make_unique<FaadDecoder>(iRI, iopAudioBuffer);
#endif

  mFrameByteVec.resize(mRsDims * 120);  // input
  mOutVec.resize(mRsDims * 120);
  codeword_ok.resize(mRsDims);
}

/**
  * \brief addtoFrame
  *
  * a DAB+ superframe consists of 5 consecutive DAB frames
  * we add vector for vector to the superframe. Once we have
  * 5 lengths of "old" frames, we check.
  * Note that the packing in the entry vector is still one bit
  * per Byte, nbits is the number of Bits (i.e. containing bytes)
  * the function adds nbits bits, packed in bytes, to the frame.
  */
void Mp4Processor::add_to_frame(const std::vector<u8>& iV)
{
  const i16 numBits = 24 * mBitRate;
  const i16 numBytes = numBits / 8;
  i32 corrected_errors = 0;
  i32 uncorr_errors;

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

  if(frames_since_sync >= 0)
    frames_since_sync++;

  // we take the last five blocks to look at
  if (mBlocksInBuffer < 5)
    return;

  // first, we show the "successrate"
  if (++mFrameCount >= 25)
  {
    mFrameCount = 0;
    emit signal_show_frame_errors(mFrameErrors);
    mFrameErrors = 0;
  }

  _process_reed_solomon_frame(mFrameByteVec.data(), mBlockFillIndex * numBytes, corrected_errors, uncorr_errors, &codeword_ok);

  // A superframe starts 5 frames after the last correct one (or a
  // multiple of 5: a broken one is followed by 4 shifted attempts)
  const bool aligned = frames_since_sync > 0 && frames_since_sync % 5 == 0;
  if(aligned && uncorr_errors && redecoder)
    Redecode(corrected_errors, uncorr_errors);

  if (!_check_reed_solomon_frame(aligned))
  {
    mBlocksInBuffer = 4;
    if(frames_since_sync >= 5 * max_superframes_without_sync)
    {
      frames_since_sync = -1;
      mSumFrameErrors++;
    }
    else
      if (frames_since_sync > 0 && frames_since_sync % 5 == 0)
      {
        mSumFcErrors++;
        mSumCorrections += corrected_errors;
        mSumRsErrors += uncorr_errors;
      }

    if(sync_frames == 0)
    {
      fprintf(stderr, "SuperframeFilter: Superframe sync started...\n");
    }
    sync_frames++;
    return;
  }

  if(sync_frames)
  {
    fprintf(stderr, "SuperframeFilter: Superframe sync succeeded after %d frame(s)\n", sync_frames);
    sync_frames = 0;
  }

  frames_since_sync = 0;
  mBlocksInBuffer = 0;

  if (++mSuccessFrames > 25)
  {
    emit signal_show_rs_errors(mRsErrors);
    mSuccessFrames = 0;
    mRsErrors = 0;
  }

  mSumCorrections += corrected_errors;
  mSumRsErrors += uncorr_errors;
  if (corrected_errors || uncorr_errors)
  {
#ifdef SHOW_ERROR_STATISTICS
    const i32 SumRsFrames = mSumFrameCount * mRsDims / 5;
    const i32 SumCrcBlocks = mSumFrameCount * mNumAUs / 5;
    fprintf(stderr, "Frames=%d, Resyncs=%d, FC Errors=%d, FC Corr=%d, RS Errors=%d, RS Corr=%d, CRC Blocks=%d, CRC Errors=%d, CRC ok=%d\n",
            SumRsFrames, mSumFrameErrors, mSumFcErrors, mSumFcCorrections, mSumRsErrors, mSumCorrections, SumCrcBlocks, mSumCrcErrors, mSum_good_crcs);
#endif
  }
  _process_super_frame();
}

void Mp4Processor::_process_reed_solomon_frame(const u8* const ipFrameBytes, const i16 iBase, i32& corrected_errors, i32& uncorr_errors, std::vector<bool> *codeword_ok)
{
  /**
    *   apply reed-solomon error repair
    *   OK, what we now have is a vector with RSDims * 120 u8's
    *   the superframe, containing parity bytes for error repair
    *   take into account the interleaving that is applied.
    */
  uncorr_errors = 0;
  if(codeword_ok)
    codeword_ok->assign(mRsDims, false);

  for (i16 j = 0; j < mRsDims; j++)
  {
    std::array<u8, 120> rsIn;

    for (i16 k = 0; k < 120; k++)
    {
      rsIn[k] = ipFrameBytes[(iBase + j + k * mRsDims) % (mRsDims * 120)];
    }

    std::array<u8, 120> rsOut;

    const i16 ler = mRsDecoder.dec(rsIn.data(), rsOut.data(), 135);  // i7-6700K: ~28us

    if (ler < 0)
    {
      uncorr_errors++;
    }
    else
    {
      corrected_errors += ler;
      mTotalCorrections += ler;
      mGoodFrames++;
      if (mGoodFrames >= 100)
      {
        emit signal_show_rs_corrections(mTotalCorrections, mCrcErrors);
        mTotalCorrections = 0;
        mGoodFrames = 0;
        mCrcErrors = 0;
      }
    }
    if(codeword_ok && ler != -1)
      (*codeword_ok)[j] = true;

    for (i16 k = 0; k < 120; k++)
    {
      mOutVec[j + k * mRsDims] = rsOut[k];
    }
  }
}

bool Mp4Processor::_check_reed_solomon_frame(bool aligned)
{
  // When RS/firecode fails the entire superframe is unrecoverable: numAUs and sbrFlag are
  // unknown so we cannot call conceal_lost_frame() with the right sizes.  The decoder's
  // previous-frame decay will naturally apply on the next CRC-fail path once sync resumes.

  // abort, if mAuStartArr is kind of zero (prevent sync on complete zero array)
  if(mOutVec[3] == 0x00 && mOutVec[4] == 0x00)
    return false;

  if (mFireCode.check(mOutVec.data()))
    return true;

  if (aligned && mFireCode.check_and_correct_6bits(mOutVec.data()))
  {
    mSumFcCorrections++;
    return true;
  }

  return false;
}

/**
  * \brief processSuperframe
  *
  * First, we know the firecode checker gave green light
  */
void Mp4Processor::_process_super_frame()
{
  // bits 0 .. 15 is firecode
  // bit 16 is unused
  SStreamParms streamParameters;
  streamParameters.dacRate        = (mOutVec[2] >> 6) & 01;  // bit 17
  streamParameters.sbrFlag        = (mOutVec[2] >> 5) & 01;  // bit 18
  streamParameters.aacChannelMode = (mOutVec[2] >> 4) & 01;  // bit 19, 0:mono, 1:stereo
  streamParameters.psFlag         = (mOutVec[2] >> 3) & 01;  // bit 20
  streamParameters.mpegSurround   = (mOutVec[2] >> 0) & 07;  // bits 21 .. 23

  // added for the aac file writer
  streamParameters.CoreSrIndex    = streamParameters.dacRate ? (streamParameters.sbrFlag ? 6 : 3) : (streamParameters.sbrFlag ? 8 : 5);
  streamParameters.CoreChConfig   = streamParameters.aacChannelMode ? 2 : 1;

  streamParameters.ExtensionSrIndex = streamParameters.dacRate ? 3 : 5;

  u8 numAUs;
  mAuStartArr[1] = mOutVec[3] * 16 + (mOutVec[4] >> 4);

  switch (2 * streamParameters.dacRate + streamParameters.sbrFlag)
  {
  case 0: numAUs = 4;
    mAuStartArr[0] = 8;
    mAuStartArr[2] = (mOutVec[4] & 0xf) * 256 + mOutVec[5];
    mAuStartArr[3] = mOutVec[6] * 16 + (mOutVec[7] >> 4);
    break;
  //
  case 1: numAUs = 2;
    mAuStartArr[0] = 5;
    break;
  //
  case 2: numAUs = 6;
    mAuStartArr[0] = 11;
    mAuStartArr[2] = (mOutVec[4] & 0xf) * 256 + mOutVec[5];
    mAuStartArr[3] = mOutVec[6] * 16 + (mOutVec[7] >> 4);
    mAuStartArr[4] = (mOutVec[7] & 0xf) * 256 + mOutVec[8];
    mAuStartArr[5] = mOutVec[9] * 16 + (mOutVec[10] >> 4);
    break;
  //
  case 3: numAUs = 3;
    mAuStartArr[0] = 6;
    mAuStartArr[2] = (mOutVec[4] & 0xf) * 256 + mOutVec[5];
    break;
  default: assert(false);
  }
  mAuStartArr[numAUs] = 110 * (mBitRate / 8);
  mNumAUs = numAUs;

  /**
    * OK, the result is N * 110 * 8 bits (still single bit per byte!!!)
    * extract the AU's, and prepare a buffer,  with the sufficient
    * lengthy for conversion to PCM samples
    */
  // DAB+ uses a 960-sample core AAC frame; SBR doubles the output; output is always stereo.
  constexpr i32 cAacCoreFrameBytes = 960;
  const i32 numConcealSamples = cAacCoreFrameBytes * (streamParameters.sbrFlag ? 2 : 1) * 2;

  // When concealment is switched off in the configuration, lost frames are left as gaps (the
  // audio buffer is not filled at all) - the former behaviour before packet-loss concealment.
  const bool concealDropouts = Settings::Config::cbAudioConcealment.read().toBool();

  for (i16 auIdx = 0; auIdx < numAUs; ++auIdx)
  {
    const i32 aacFrameLen = mAuStartArr[auIdx + 1] - mAuStartArr[auIdx] - 2;

    // Just a sanity check
    if (aacFrameLen > cAacCoreFrameBytes || aacFrameLen < 0)
    {
      qWarning() << "Invalid aacFrameLen =" << aacFrameLen;
      mAacErrors++;
      // Conceal this AU and continue rather than abandoning all remaining AUs.
      if (concealDropouts) mpAacDecoder->conceal_lost_frame(numConcealSamples);
    }
    //  but first the crc check
    else if (const std::span<const u8> auWithCrc = std::span(mOutVec).subspan(mAuStartArr[auIdx], aacFrameLen + 2);
             check_crc_bytes(auWithCrc))
    {
      //    first prepare dumping
      std::vector<u8> aacStreamBuffer;
      const i32 segmentSize = _build_aac_stream(&streamParameters, auWithCrc.first(aacFrameLen), aacStreamBuffer);

      mSum_good_crcs++;

      mpFrameBuffer->put_data_into_ring_buffer(aacStreamBuffer.data(), segmentSize); // This is used by the "ACC dump button" in the Audio Service Details widget
      emit signal_new_aac_frame();

      // first handle the pad data if any
      if (((mOutVec[mAuStartArr[auIdx + 0]] >> 5) & 07) == 4)
      {
        const i16 count = mOutVec[mAuStartArr[auIdx] + 1];
        auto* const buffer = make_vla(u8, count);
        memcpy(buffer, &mOutVec[mAuStartArr[auIdx] + 2], count);
        const u8 L0 = buffer[count - 1];
        const u8 L1 = buffer[count - 2];
        mPadhandler.process_PAD(buffer, count - 3, L1, L0);
      }

      //    then handle the audio
#ifdef  __WITH_FDK_AAC__
      const i32 aacErr = mpAacDecoder->convert_mp4_to_pcm(&streamParameters, aacStreamBuffer);
#else
      std::array<u8, cAacCoreFrameBytes + 10> theAudioUnit;  // max size is ensured above but for what are the +10?
      assert(aacFrameLen <= cAacCoreFrameBytes);
      memcpy(theAudioUnit.data(), &mOutVec[mAuStartArr[auIdx]], aacFrameLen);
      memset(&theAudioUnit[aacFrameLen], 0, 10);

      const i32 aacErr = mpAacDecoder->convert_mp4_to_pcm(&streamParameters, std::span(theAudioUnit).first(aacFrameLen));
#endif

      emit signal_is_stereo(streamParameters.aacChannelMode == 1 || streamParameters.psFlag == 1);

      if (aacErr <= 0)
      {
        qWarning() << "AAC decoding error: " << aacErr;
        mAacErrors++;
        // The AAC decoder produced no PCM; conceal the gap to avoid a buffer hole.
        if (concealDropouts) mpAacDecoder->conceal_lost_frame(numConcealSamples);
      }
    }
    else
    {
      mCrcErrors++;
      mSumCrcErrors++;
      if (concealDropouts) mpAacDecoder->conceal_lost_frame(numConcealSamples);
    }

    if (++mAacFrames > 25)
    {
      emit signal_show_aac_errors(mAacErrors);
      mAacErrors = 0;
      mAacFrames = 0;
    }
  }
}

i32 Mp4Processor::_build_aac_stream(const SStreamParms* const iSp, const std::span<const u8>& iAacFrame, std::vector<u8>& oFileBuffer) const
{
  const i16 iAacFrameLen = (i16)iAacFrame.size();
  BitWriter au_bw(oFileBuffer); // fileBuffer will filled up in BitWriter

  au_bw.AddBits(0x2B7, 11);  // syncword
  au_bw.AddBits(0, 13);  // audioMuxLengthBytes - written later
  //    AudioMuxElement(1)

  au_bw.AddBits(0, 1);  // useSameStreamMux
  //    StreamMuxConfig()

  au_bw.AddBits(0, 1);  // audioMuxVersion
  au_bw.AddBits(1, 1);  // allStreamsSameTimeFraming
  au_bw.AddBits(0, 6);  // numSubFrames
  au_bw.AddBits(0, 4);  // numProgram
  au_bw.AddBits(0, 3);  // numLayer

  if (iSp->sbrFlag)
  {
    au_bw.AddBits(0b00101, 5); // SBR
    au_bw.AddBits(iSp->CoreSrIndex, 4); // samplingFrequencyIndex
    au_bw.AddBits(iSp->CoreChConfig, 4); // channelConfiguration
    au_bw.AddBits(iSp->ExtensionSrIndex, 4);  // extensionSamplingFrequencyIndex
    au_bw.AddBits(0b00010, 5);    // AAC LC
    au_bw.AddBits(0b100, 3);              // GASpecificConfig() with 960 transform
  }
  else
  {
    au_bw.AddBits(0b00010, 5); // AAC LC
    au_bw.AddBits(iSp->CoreSrIndex, 4); // samplingFrequencyIndex
    au_bw.AddBits(iSp->CoreChConfig, 4); // channelConfiguration
    au_bw.AddBits(0b100, 3);              // GASpecificConfig() with 960 transform
  }

  au_bw.AddBits(0b000, 3);  // frameLengthType
  au_bw.AddBits(0xFF, 8);  // latmBufferFullness
  au_bw.AddBits(0, 1);  // otherDataPresent
  au_bw.AddBits(0, 1);  // crcCheckPresent

  //    PayloadLengthInfo()
  for (size_t i = 0; i < (size_t)(iAacFrameLen / 255); i++)
  {
    au_bw.AddBits(0xFF, 8);
  }
  au_bw.AddBits(iAacFrameLen % 255, 8);

  au_bw.AddBytes(iAacFrame);
  au_bw.WriteAudioMuxLengthBytes();

  return oFileBuffer.size();
}

// Decodes the 5 frames of the superframe again with the bytes of the correct
// RS codewords as known bits: they split the error bursts of the Viterbi
// decoder, so that more codewords become correctable, which gives more
// known bytes for the next pass
void Mp4Processor::Redecode(i32& corrected_errors, i32& uncorr_errors)
{
  const i32 codewords = mRsDims;
  const i32 sf_len = mRsDims * 120;
  const i32 frame_len = sf_len / 5;
  known_bytes.resize(frame_len);

  for(i32 pass = 0; pass < max_redecode_passes; pass++)
  {
    const i32 correct_before = std::count(codeword_ok.begin(), codeword_ok.end(), true);
    if(correct_before == 0)
       return;

    for(i32 f = 0; f < 5; f++)
    {
      for(i32 i = 0; i < frame_len; i++)
      {
        const i32 pos = f * frame_len + i;
        // byte pos of the superframe is in RS codeword pos % codewords
        known_bytes[i] = codeword_ok[pos % codewords];
      }
      if(!redecoder->redecodeFrame(4 - f, known_bytes.data(), &mOutVec[f * frame_len]))
        return;
    }
    memcpy(mFrameByteVec.data(), mOutVec.data(), sf_len);

    _process_reed_solomon_frame(mFrameByteVec.data(), 0, corrected_errors, uncorr_errors, &codeword_ok);
#ifdef SHOW_ERROR_STATISTICS
    fprintf(stderr, "Redecode: pass=%d, len=%d, RS Errors=%d, RS Corr=%d\n", pass, sf_len, uncorr_errors, corrected_errors);
#endif

    const i32 correct = std::count(codeword_ok.begin(), codeword_ok.end(), true);
    if(!uncorr_errors || correct <= correct_before)
      return;
  }
}
