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

#include  "tdc-datahandler.h"
#include  "dabradio.h"
#include  "bit-extractors.h"
#include  "crc.h"

tdc_dataHandler::tdc_dataHandler(DabRadio * mr, RingBuffer<u8> * dataBuffer, i16 /*appType*/)
{
  myRadioInterface = mr;
  this->dataBuffer = dataBuffer;
  //	for the moment we assume appType 4
  connect(this, &tdc_dataHandler::signal_bytes_out, myRadioInterface,  &DabRadio::slot_handle_tdc_data);
}

#define  swap(a)  (((a) << 8) | ((a) >> 8))

//---------------------------------------------------------------------------
u16 usCalculCRC(u8 * buf, int lg)
{
  u16 crc;
  int count;
  crc = 0xFFFF;
  for (count = 0; count < lg; count++)
  {
    crc = (u16)(swap(crc) ^ (u16)buf[count]);
    crc ^= ((u8)crc) >> 4;
    crc = (u16)(crc ^ (swap((u8)(crc)) << 4) ^ ((u8)(crc) << 5));
  }
  return ((u16)(crc ^ 0xFFFF));
}

void tdc_dataHandler::add_mscDatagroup(const std::vector<u8> & m)
{
  i32 offset = 0;
  u8 * data = (u8 *)(m.data());
  i32 size = m.size();
  i16 i;

  //	we maintain offsets in bits, the "m" array has one bit per byte
  while (offset < size)
  {
    while (offset + 16 < size)
    {
      if (getBits(data, offset, 16) == 0xFF0F)
      {
        break;
      }
      else
      {
        offset += 8;
      }
    }
    if (offset + 16 >= size)
    {
      return;
    }

    //	we have a syncword
    //	   u16 syncword	= getBits (data, offset,      16);
    i16 length = getBits(data, offset + 16, 16);
    u16 crc = getBits(data, offset + 32, 16);

    (void)crc;
    u8 frametypeIndicator = getBits(data, offset + 48, 8);
    if ((length < 0) || (length >= (size - offset) / 8))
    {
      return;
    }    // garbage
    //
    //	OK, prepare to check the crc
    u8 checkVector[18];
    //
    //	first the syncword and the length
    for (i = 0; i < 4; i++)
    {
      checkVector[i] = getBits(data, offset + i * 8, 8);
    }
    //
    //	we skip the crc in the incoming data and take the frametype
    checkVector[4] = getBits(data, offset + 6 * 8, 8);

    int size = length < 11 ? length : 11;
    for (i = 0; i < size; i++)
    {
      checkVector[5 + i] = getBits(data, offset + 7 * 8 + i * 8, 8);
    }
    checkVector[5 + size] = getBits(data, offset + 4 * 8, 8);
    checkVector[5 + size + 1] = getBits(data, offset + 5 * 8, 8);
    if (!check_crc_bytes(checkVector, 5 + size))
    {
      qWarning("CRC failed");
      return;
    }

    if (frametypeIndicator == 0)
    {
      offset = handleFrame_type_0(data, offset + 7 * 8, length);
    }
    else if (frametypeIndicator == 1)
    {
      offset = handleFrame_type_1(data, offset + 7 * 8, length);
    }
    else
    {
      return;
    }  // failure
  }
}

i32 tdc_dataHandler::handleFrame_type_0(u8 * data, i32 offset, i32 length)
{
  i16 i;
  //i16 noS	= getBits (data, offset, 8);
  auto * const buffer = make_vla(u8, length);

  for (i = 0; i < length; i++)
  {
    buffer[i] = getBits(data, offset + i * 8, 8);
  }
  if (!check_crc_bytes(buffer, length - 2))
  {
    fprintf(stdout, "crc check failed\n");
  }
#if 0
  fprintf (stdout, "nrServices %d, SID-A %d SID-B %d SID-C %d\n",
                    buffer [0], buffer [1], buffer [2], buffer [3]);
#endif
  dataBuffer->put_data_into_ring_buffer(buffer, length);
  emit signal_bytes_out(0, length);
  return offset + length * 8;
}

i32 tdc_dataHandler::handleFrame_type_1(u8 * data, i32 offset, i32 length)
{
  i16 i;
  auto * const buffer = make_vla(u8, length);
  int lOffset;
  int llengths = length - 4;
#if 0
  fprintf (stdout, " frametype 1  (length %d) met %d %d %d\n", length,
                               getBits (data, offset,      8),
                               getBits (data, offset + 8,  8),
                               getBits (data, offset + 16, 8));
  fprintf (stdout, "encryption %d\n", getBits (data, offset + 24, 8));
#endif
  for (i = 0; i < length; i++)
  {
    buffer[i] = getBits(data, offset + i * 8, 8);
  }
  dataBuffer->put_data_into_ring_buffer(buffer, length);
  if (getBits(data, offset + 24, 8) == 0)
  {  // no encryption
    lOffset = offset + 4 * 8;
    do
    {
      //	      int compInd	= getBits (data, lOffset, 8);
      int flength = getBits(data, lOffset + 8, 16);
      //	      int crc		= getBits (data, lOffset + 3 * 8, 8);
#if 0
      fprintf (stdout, "segment %d, length %d\n",
                                 compInd, flength);
      for (i = 5; i < flength; i ++)
         fprintf (stdout, "%c", buffer [i]);
      fprintf (stderr, "\n");
#endif
      lOffset += (flength + 5) * 8;
      llengths -= flength + 5;
    }
    while (llengths > 10);
  }
  emit signal_bytes_out(1, length);
  return offset + length * 8;
}

//	The component header CRC is two bytes long,
//	and based on the ITU-T polynomial x^16 + x*12 + x^5 + 1.
//	The component header CRC is calculated from the service component
//	identifier, the field length and the first 13 bytes of the
//	component data. In the case of component data shorter
//	than 13 bytes, the component identifier, the field
//	length and all component data shall be taken into account.
bool tdc_dataHandler::serviceComponentFrameheaderCRC(const u8 * data, i16 offset, i16 /*maxL*/)
{
  u8 testVector[18];
  i16 i;
  i16 length = getBits(data, offset + 8, 16);
  i16 size = length < 13 ? length : 13;
  u16 crc;

  if (length < 0)
  {
    return false;
  }    // assumed garbage
  crc = getBits(data, offset + 24, 16);  // the crc
  testVector[0] = getBits(data, offset + 0, 8);
  testVector[1] = getBits(data, offset + 8, 8);
  testVector[2] = getBits(data, offset + 16, 8);
  for (i = 0; i < size; i++)
  {
    testVector[3 + i] = getBits(data, offset + 40 + i * 8, 8);
  }

  return usCalculCRC(testVector, 3 + size) == crc;
}


