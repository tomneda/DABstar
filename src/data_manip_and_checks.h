//
// Created on 26/08/23.
//

#ifndef DATA_MANIP_AND_CHECKS_H
#define DATA_MANIP_AND_CHECKS_H

#include <cinttypes>

//	generic, up to 16 bits
static inline uint16_t getBits(const uint8_t * d, int32_t offset, int16_t size)
{
  int16_t i;
  uint16_t res = 0;

  for (i = 0; i < size; i++)
  {
    res <<= 1;
    res |= (d[offset + i]) & 01;
  }
  return res;
}

static inline uint16_t getBits_1(const uint8_t * d, int32_t offset)
{
  return (d[offset] & 0x01);
}

static inline uint16_t getBits_2(const uint8_t * d, int32_t offset)
{
  uint16_t res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  return res;
}

static inline uint16_t getBits_3(const uint8_t * d, int32_t offset)
{
  uint16_t res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  res <<= 1;
  res |= d[offset + 2];
  return res;
}

static inline uint16_t getBits_4(const uint8_t * d, int32_t offset)
{
  uint16_t res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  res <<= 1;
  res |= d[offset + 2];
  res <<= 1;
  res |= d[offset + 3];
  return res;
}

static inline uint16_t getBits_5(const uint8_t * d, int32_t offset)
{
  uint16_t res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  res <<= 1;
  res |= d[offset + 2];
  res <<= 1;
  res |= d[offset + 3];
  res <<= 1;
  res |= d[offset + 4];
  return res;
}

static inline uint16_t getBits_6(const uint8_t * d, int32_t offset)
{
  uint16_t res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  res <<= 1;
  res |= d[offset + 2];
  res <<= 1;
  res |= d[offset + 3];
  res <<= 1;
  res |= d[offset + 4];
  res <<= 1;
  res |= d[offset + 5];
  return res;
}

static inline uint16_t getBits_7(const uint8_t * d, int32_t offset)
{
  uint16_t res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  res <<= 1;
  res |= d[offset + 2];
  res <<= 1;
  res |= d[offset + 3];
  res <<= 1;
  res |= d[offset + 4];
  res <<= 1;
  res |= d[offset + 5];
  res <<= 1;
  res |= d[offset + 6];
  return res;
}

static inline uint16_t getBits_8(const uint8_t * d, int32_t offset)
{
  uint16_t res = d[offset];
  res <<= 1;
  res |= d[offset + 1];
  res <<= 1;
  res |= d[offset + 2];
  res <<= 1;
  res |= d[offset + 3];
  res <<= 1;
  res |= d[offset + 4];
  res <<= 1;
  res |= d[offset + 5];
  res <<= 1;
  res |= d[offset + 6];
  res <<= 1;
  res |= d[offset + 7];
  return res;
}


static inline uint32_t getLBits(const uint8_t * d, int32_t offset, int16_t amount)
{
  uint32_t res = 0;
  int16_t i;

  for (i = 0; i < amount; i++)
  {
    res <<= 1;
    res |= (d[offset + i] & 01);
  }
  return res;
}

static inline bool check_CRC_bits(const uint8_t * const iIn, const int32_t iSize)
{
  static const uint8_t crcPolynome[] = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 };  // MSB .. LSB
  int32_t f;
  uint8_t b[16];
  int16_t Sum = 0;

  memset(b, 1, 16);

  for (int32_t i = 0; i < iSize; i++)
  {
    const uint8_t invBit = (i >= iSize - 16 ? 1 : 0);

    if ((b[0] ^ (iIn[i] ^ invBit)) == 1)
    {
      for (f = 0; f < 15; f++)
      {
        b[f] = crcPolynome[f] ^ b[f + 1];
      }
      b[15] = 1;
    }
    else
    {
      memmove(&b[0], &b[1], sizeof(uint8_t) * 15); // Shift
      b[15] = 0;
    }
  }

  for (int32_t i = 0; i < 16; i++)
  {
    Sum += b[i];
  }

  return Sum == 0;
}

static inline bool check_crc_bytes(const uint8_t * const msg, const int32_t len)
{
  int i, j;
  uint16_t accumulator = 0xFFFF;
  uint16_t crc;
  uint16_t genpoly = 0x1021;

  for (i = 0; i < len; i++)
  {
    int16_t data = msg[i] << 8;
    for (j = 8; j > 0; j--)
    {
      if ((data ^ accumulator) & 0x8000)
      {
        accumulator = ((accumulator << 1) ^ genpoly) & 0xFFFF;
      }
      else
      {
        accumulator = (accumulator << 1) & 0xFFFF;
      }
      data = (data << 1) & 0xFFFF;
    }
  }
  //
  //	ok, now check with the crc that is contained
  //	in the au
  crc = ~((msg[len] << 8) | msg[len + 1]) & 0xFFFF;
  return (crc ^ accumulator) == 0;
}

#endif // DATA_MANIP_AND_CHECKS_H
