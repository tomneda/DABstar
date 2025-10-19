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
 *
 */

#include  "ip-datahandler.h"
#include  "dabradio.h"
#include  "bit-extractors.h"
#include  "crc.h"

IpDataHandler::IpDataHandler(DabRadio * mr, RingBuffer<u8> * dataBuffer)
{
  this->dataBuffer = dataBuffer;
  this->handledPackets = 0;
  connect(this, &IpDataHandler::signal_write_datagramm, mr,  &DabRadio::slot_send_datagram);
}

void IpDataHandler::add_mscDatagroup(const std::vector<u8> & msc)
{
  u8 * data = (u8 *)(msc.data());
  bool extensionFlag = getBits_1(data, 0) != 0;
  bool crcFlag = getBits_1(data, 1) != 0;
  bool segmentFlag = getBits_1(data, 2) != 0;
  bool userAccessFlag = getBits_1(data, 3) != 0;
  i16 next = 16;    // bits
  bool lastSegment = false;
  u16 segmentNumber = 0;
  bool transportIdFlag = false;
  u16 transportId = 0;
  u8 lengthInd;
  i16 i;

  if (crcFlag && !check_CRC_bits(data, msc.size()))
  {
    return;
  }

  if (extensionFlag)
  {
    next += 16;
  }

  if (segmentFlag)
  {
    lastSegment = getBits_1(data, next) != 0;
    segmentNumber = getBits(data, next + 1, 15);
    next += 16;
  }

  (void)lastSegment;
  (void)segmentNumber;
  if (userAccessFlag)
  {
    transportIdFlag = getBits_1(data, next + 3);
    lengthInd = getBits_4(data, next + 4);
    next += 8;
    if (transportIdFlag)
    {
      transportId = getBits(data, next, 16);
    }
    next += lengthInd * 8;
  }
  (void)transportId;
  u16 ipLength = 0;
  i16 sizeinBits = msc.size() - next - (crcFlag != 0 ? 16 : 0);

  (void)sizeinBits;
  ipLength = getBits(data, next + 16, 16);
  if (ipLength < msc.size() / 8)
  {  // just to be sure
    std::vector<u8> ipVector;
    ipVector.resize(ipLength);
    for (i = 0; i < ipLength; i++)
    {
      ipVector[i] = getBits_8(data, next + 8 * i);
    }
    if ((ipVector[0] >> 4) != 4)
    {
      return;
    }  // should be version 4
    process_ipVector(ipVector);
  }
}

void IpDataHandler::process_ipVector(const std::vector<u8> & v)
{
  u8 * data = (u8 *)(v.data());
  i16 headerSize = data[0] & 0x0F;  // in 32 bits words
  i16 ipSize = (data[2] << 8) | data[3];
  u8 protocol = data[9];

  u32 checkSum = 0;
  i16 i;

  for (i = 0; i < 2 * headerSize; i++)
  {
    checkSum += ((data[2 * i] << 8) | data[2 * i + 1]);
  }
  checkSum = (checkSum >> 16) + (checkSum & 0xFFFF);
  if ((~checkSum & 0xFFFF) != 0)
  {
    return;
  }

  switch (protocol)
  {
  case 17:      // UDP protocol
    process_udpVector(&data[4 * headerSize], ipSize - 4 * headerSize);
    return;
  default: return;
  }
}

//
//	We keep it simple now, just hand over the data from the
//	udp packet to port 8888
void IpDataHandler::process_udpVector(const u8 * data, i16 length)
{
  char * message = (char *)(&(data[8]));
  dataBuffer->put_data_into_ring_buffer((u8 *)message, length - 8);
  signal_write_datagramm(length - 8);
}

