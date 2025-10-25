/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2015 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
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
#include "dab-constants.h"
#include "dabradio.h"
#include "data-processor.h"
#include "virtual-datahandler.h"
#include "ip-datahandler.h"
#include "mot-handler.h"
#include "journaline-datahandler.h"
#include "tdc-datahandler.h"
#include "bit-extractors.h"
#include "crc.h"

// The main function of this class is to assemble the MSCdatagroups and dispatch to the appropriate handler
DataProcessor::DataProcessor(DabRadio * mr, const SPacketData * const ipPD, RingBuffer<u8> * const ipDataBuffer)
  : mpDabRadio(mr)
  , mBitRate(ipPD->bitRate)
  , mDSCTy(ipPD->DSCTy)
  , mAppType(ipPD->appTypeVec[0]) // TODO: only first element
  , mPacketAddress(ipPD->PacketAddress)
  , mDGflag(ipPD->DGflag)
  , mFEC_scheme(ipPD->FEC_scheme)
  , mpDataBuffer(ipDataBuffer)
{
  switch (mDSCTy)
  {
  case 5:
    if (mAppType == 0x44a)
    {
      qInfo() << "Create Journaline (DSCTy 5, AppType 0x44a) data handler";
      mpDataHandler.reset(new JournalineDataHandler);
      break;
    }
    else if (mAppType == 1500)
    {
      qInfo() << "Create ADV (DSCTy 5, AppType 1500) data handler (not implemented)";
      //my_dataHandler = new adv_dataHandler(mr, dataBuffer, appType);
      mpDataHandler.reset(new VirtualDataHandler);
      break;
    }
    else if (mAppType == 4)
    {
      qInfo() << "Create TDC (DSCTy 5, AppType 4) data handler";
      mpDataHandler.reset(new tdc_dataHandler(mr, mpDataBuffer, mAppType));
      break;
    }
    else
    {
      qWarning() << "DSCTy 5 with AppType" << mAppType << "not supported";
      mpDataHandler.reset(new VirtualDataHandler);
    }
    break;

  case 44:
    qInfo() << "Create Journaline (DSCTy 44) data handler";
    mpDataHandler.reset(new JournalineDataHandler);
    break;

  case 59:
    qInfo() << "Create IP (DSCTy 59) data handler";
    mpDataHandler.reset(new IpDataHandler(mr, mpDataBuffer));
    break;

  case 60:
    qInfo() << "Create MOT (DSCTy 60) data handler";
    mpDataHandler.reset(new MotHandler(mr));
    break;

  default:
    qWarning() << "DSCTy " << mDSCTy << " not supported";
    mpDataHandler.reset(new VirtualDataHandler);
    break;
  }

  mPacketState = 0;
}

void DataProcessor::add_to_frame(const std::vector<u8> & outV)
{
  //	There is - obviously - some exception, that is
  //	when the DG flag is on and there are no datagroups for DSCTy5

  if ((this->mDSCTy == 5) && (this->mDGflag))
  {  // no datagroups
    _handle_TDC_async_stream(outV.data(), 24 * mBitRate);
  }
  else
  {
    _handle_packets(outV.data(), 24 * mBitRate);
  }
}

// While for a full mix data and audio there will be a single packet in a
// data compartment, for an empty mix, there may be many more
void DataProcessor::_handle_packets(const u8 * data, i32 length)
{
  while (true)
  {
    i32 pLength = (getBits_2(data, 0) + 1) * 24 * 8;
    if (length < pLength)
    {  // be on the safe side
      return;
    }
    _handle_packet(data);
    length -= pLength;
    if (length < 2)
    {
      return;
    }
    data = &(data[pLength]);
  }
}

// Handle a single DAB packet:
// Note, although not yet encountered, the standard says that there may be multiple streams, to be identified by the address.
// For the time being we only handle a single stream!!!!
void DataProcessor::_handle_packet(const u8 * data)
{
  i32 packetLength = (getBits_2(data, 0) + 1) * 24;
  i16 continuityIndex = getBits_2(data, 2);
  i16 firstLast = getBits_2(data, 4);
  i16 address = getBits(data, 6, 10);
  u16 command = getBits_1(data, 16);
  i32 usefulLength = getBits_7(data, 17);
  //	if (usefulLength > 0)
  //	   fprintf (stdout, "CI = %d, address = %d, usefulLength = %d\n",
  //	                       continuityIndex, address, usefulLength);

  if (continuityIndex != mExpectedIndex)
  {
    mExpectedIndex = 0;
    return;
  }

  mExpectedIndex = (mExpectedIndex + 1) % 4;
  (void)command;

  if (!check_CRC_bits(data, packetLength * 8))
  {
    return;
  }

  if (address == 0)
  {
    return;
  }    // padding packet
  //
  if (mPacketAddress != address)
  {  // sorry
    return;
  }

  //	assemble the full MSC datagroup

  if (mPacketState == 0)
  {  // waiting for a start
    if (firstLast == 02)
    {  // first packet
      mPacketState = 1;
      mSeriesVec.resize(usefulLength * 8);
      for (u16 i = 0; i < mSeriesVec.size(); i++)
      {
        mSeriesVec[i] = data[24 + i];
      }
    }
    else if (firstLast == 03)
    {  // single packet, mostly padding
      mSeriesVec.resize(usefulLength * 8);
      for (u16 i = 0; i < mSeriesVec.size(); i++)
      {
        mSeriesVec[i] = data[24 + i];
      }
      mpDataHandler->add_MSC_data_group(mSeriesVec);
    }
    else
    {
      mSeriesVec.resize(0);
    }  // packetState remains 0
  }
  else if (mPacketState == 01)
  {  // within a series
    if (firstLast == 0)
    {  // intermediate packet
      i32 currentLength = mSeriesVec.size();
      mSeriesVec.resize(currentLength + 8 * usefulLength);
      for (u16 i = 0; i < 8 * usefulLength; i++)
      {
        mSeriesVec[currentLength + i] = data[24 + i];
      }
    }
    else if (firstLast == 01)
    {  // last packet
      i32 currentLength = mSeriesVec.size();
      mSeriesVec.resize(currentLength + 8 * usefulLength);
      for (u16 i = 0; i < 8 * usefulLength; i++)
      {
        mSeriesVec[currentLength + i] = data[24 + i];
      }

      mpDataHandler->add_MSC_data_group(mSeriesVec);
      mPacketState = 0;
    }
    else if (firstLast == 02)
    {  // first packet, previous one erroneous
      mPacketState = 1;
      mSeriesVec.resize(usefulLength * 8);
      for (u16 i = 0; i < mSeriesVec.size(); i++)
      {
        mSeriesVec[i] = data[24 + i];
      }
    }
    else
    {
      mPacketState = 0;
      mSeriesVec.resize(0);
    }
  }
}

//	Really no idea what to do here
void DataProcessor::_handle_TDC_async_stream(const u8 * data, i32 length)
{
  i16 packetLength = (getBits_2(data, 0) + 1) * 24;
  i16 continuityIndex = getBits_2(data, 2);
  i16 firstLast = getBits_2(data, 4);
  i16 address = getBits(data, 6, 10);
  u16 command = getBits_1(data, 16);
  i16 usefulLength = getBits_7(data, 17);

  (void)length;
  (void)packetLength;
  (void)continuityIndex;
  (void)firstLast;
  (void)address;
  (void)command;
  (void)usefulLength;

  if (!check_CRC_bits(data, packetLength * 8))
  {
    return;
  }
}

//
