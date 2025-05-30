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
#include  "dab-constants.h"
#include  "dabradio.h"
#include  "data-processor.h"
#include  "virtual-datahandler.h"
#include  "ip-datahandler.h"
#include  "mot-handler.h"
#include  "journaline-datahandler.h"
#include  "tdc-datahandler.h"
#include  "data_manip_and_checks.h"

//	\class DataProcessor
//	The main function of this class is to assemble the 
//	MSCdatagroups and dispatch to the appropriate handler
//
//	fragmentsize == Length * CUSize
DataProcessor::DataProcessor(DabRadio * mr, const Packetdata * pd, RingBuffer<u8> * dataBuffer)
{
  this->myRadioInterface = mr;
  this->bitRate = pd->bitRate;
  this->DSCTy = pd->DSCTy;
  this->appType = pd->appType;
  this->packetAddress = pd->packetAddress;
  this->DGflag = pd->DGflag;
  this->FEC_scheme = pd->FEC_scheme;
  this->dataBuffer = dataBuffer;
  this->expectedIndex = 0;
  switch (DSCTy)
  {
  default: fprintf(stderr, "DSCTy %d not supported\n", DSCTy);
    my_dataHandler = new virtual_dataHandler();
    break;

  case 5: my_dataHandler = new tdc_dataHandler(mr, dataBuffer, appType);
    break;

  case 44: my_dataHandler = new journaline_dataHandler();
    break;

  case 59: my_dataHandler = new ip_dataHandler(mr, dataBuffer);
    break;

  case 60: my_dataHandler = new MotHandler(mr);
    break;

  }

  packetState = 0;
}

DataProcessor::~DataProcessor()
{
  delete my_dataHandler;
}


void DataProcessor::add_to_frame(const std::vector<u8> & outV)
{
  //	There is - obviously - some exception, that is
  //	when the DG flag is on and there are no datagroups for DSCTy5

  if ((this->DSCTy == 5) && (this->DGflag))
  {  // no datagroups
    handleTDCAsyncstream(outV.data(), 24 * bitRate);
  }
  else
  {
    handlePackets(outV.data(), 24 * bitRate);
  }
}

//
//	While for a full mix data and audio there will be a single packet in a
//	data compartment, for an empty mix, there may be many more
void DataProcessor::handlePackets(const u8 * data, i32 length)
{
  while (true)
  {
    i32 pLength = (getBits_2(data, 0) + 1) * 24 * 8;
    if (length < pLength)
    {  // be on the safe side
      return;
    }
    handlePacket(data);
    length -= pLength;
    if (length < 2)
    {
      return;
    }
    data = &(data[pLength]);
  }
}

//
//	Handle a single DAB packet:
//	Note, although not yet encountered, the standard says that
//	there may be multiple streams, to be identified by
//	the address. For the time being we only handle a single
//	stream!!!!
void DataProcessor::handlePacket(const u8 * data)
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

  if (continuityIndex != expectedIndex)
  {
    expectedIndex = 0;
    return;
  }

  expectedIndex = (expectedIndex + 1) % 4;
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
  if (packetAddress != address)
  {  // sorry
    return;
  }

  //	assemble the full MSC datagroup

  if (packetState == 0)
  {  // waiting for a start
    if (firstLast == 02)
    {  // first packet
      packetState = 1;
      series.resize(usefulLength * 8);
      for (u16 i = 0; i < series.size(); i++)
      {
        series[i] = data[24 + i];
      }
    }
    else if (firstLast == 03)
    {  // single packet, mostly padding
      series.resize(usefulLength * 8);
      for (u16 i = 0; i < series.size(); i++)
      {
        series[i] = data[24 + i];
      }
      my_dataHandler->add_mscDatagroup(series);
    }
    else
    {
      series.resize(0);
    }  // packetState remains 0
  }
  else if (packetState == 01)
  {  // within a series
    if (firstLast == 0)
    {  // intermediate packet
      i32 currentLength = series.size();
      series.resize(currentLength + 8 * usefulLength);
      for (u16 i = 0; i < 8 * usefulLength; i++)
      {
        series[currentLength + i] = data[24 + i];
      }
    }
    else if (firstLast == 01)
    {  // last packet
      i32 currentLength = series.size();
      series.resize(currentLength + 8 * usefulLength);
      for (u16 i = 0; i < 8 * usefulLength; i++)
      {
        series[currentLength + i] = data[24 + i];
      }

      my_dataHandler->add_mscDatagroup(series);
      packetState = 0;
    }
    else if (firstLast == 02)
    {  // first packet, previous one erroneous
      packetState = 1;
      series.resize(usefulLength * 8);
      for (u16 i = 0; i < series.size(); i++)
      {
        series[i] = data[24 + i];
      }
    }
    else
    {
      packetState = 0;
      series.resize(0);
    }
  }
}

//	Really no idea what to do here
void DataProcessor::handleTDCAsyncstream(const u8 * data, i32 length)
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
