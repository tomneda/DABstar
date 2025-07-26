
#include	<iostream>
#include	"dab-constants.h"
#include	"eti-generator.h"
#include	"eep-protection.h"
#include	"uep-protection.h"
#include	<memory>

static u16 const crctab_1021[256] = {
  0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
  0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
  0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
  0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
  0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
  0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
  0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
  0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
  0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
  0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
  0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
  0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
  0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
  0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
  0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
  0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
  0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
  0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
  0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
  0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
  0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
  0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
  0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
  0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
  0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
  0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
  0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
  0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
  0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
  0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
  0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
  0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

static u16 calc_crc(u8 * data, i32 length, u16 const * crctab, u32 crc)
{
  i32 count;
  u32 temp;

  for (count = 0; count < length; ++count)
  {
    temp = (*data++ ^ (crc >> 8)) & 0xff;
    crc = crctab[temp] ^ (crc << 8);
  }

  return crc & 0xffff;
}

i16 cif_In[55296];
i16 cifVector[16][55296];
u8 fibVector[16][96];
bool fibValid[16];

#define  CUSize  (4 * 16)
//
//	For each subchannel we create a
//	deconvoluter and a descramble table up front
Protection * protTable[64] = { nullptr };
u8 * descrambler[64] = { nullptr };

i16 temp[55296];
const i16 interleaveMap[] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };
u8 theVector[6144];

//
//	fibvector contains the processed fics, i.e ready for addition
//	to the eti frame.
//	Since there are 4 fibs, one for each CIF, the index to be used for
//	filling - and processing - indicates at the end of the frame
//	again the last fib.
//	In between there are more fib fields filled than CIFS
//
//	Since the odpprocessor (together with the decoder) takes quite an
//	amount of cycles, the eti-generation is done in a different thread
//	Note CIF counts from 0 .. 3
//
EtiGenerator::EtiGenerator(FicHandler * my_ficHandler)
{
  this->my_ficHandler = my_ficHandler;

  index_Out = 0;
  BitsperBlock = c2K;
  numberofblocksperCIF = 18;  // mode I
  amount = 0;
  CIFCount_hi = -1;
  CIFCount_lo = -1;
  etiFile = nullptr;
  Minor = 0;
}

EtiGenerator::~EtiGenerator()
{
  reset();
  if (etiFile != nullptr)
  {
    fclose(etiFile);
  }
}

//
//	we probably need "reset" when handling a change in configuration
//
void EtiGenerator::reset()
{
  std::lock_guard lock(mMutex);
  for (i32 i = 0; i < 64; i++)
  {
    if (descrambler[i] != nullptr)
    {
      delete descrambler[i];
    }
    if (protTable[i] != nullptr)
    {
      delete protTable[i];
    }

    protTable[i] = nullptr;
    descrambler[i] = nullptr;
  }
  index_Out = 0;
  BitsperBlock = c2K;
  numberofblocksperCIF = 18;  // mode I
  amount = 0;
  CIFCount_hi = -1;
  CIFCount_lo = -1;
  Minor = 0;
  running = false;
}

void EtiGenerator::process_block(const std::vector<i16> & ibits, i32 iOfdmSymbIdx)
{
  std::lock_guard lock(mMutex);

  // we ensure that when starting, we start with a block 1
  if (!running && etiFile != nullptr && iOfdmSymbIdx == 1)
  {
    running = true;
  }

  if (!running)
  {
    return;
  }

  // wait until CIF/FIB data are collected "outside"
  if (iOfdmSymbIdx < 4)
  {
    return;
  }

  if (iOfdmSymbIdx == 4)
  {
    // import fibBits
    bool ficValid[4];
    my_ficHandler->get_fib_bits(fibBits, ficValid);

    for (i32 i = 0; i < 4; i++)
    {
      fibValid[index_Out + i] = ficValid[i];

      for (i32 j = 0; j < 96; j++)
      {
        fibVector[(index_Out + i) & 017][j] = 0;
        for (i32 k = 0; k < 8; k++)
        {
          fibVector[(index_Out + i) & 017][j] <<= 1;
          fibVector[(index_Out + i) & 017][j] |= (fibBits[i * 768 + 8 * j + k] & 01);
        }
      }
    }
    Minor = 0;
    my_ficHandler->get_cif_count(&CIFCount_hi, &CIFCount_lo);
  }

  // adding the MSC blocks. Blocks 5 .. 76 are "transformed" into the "soft" bits arrays
  i32 CIF_index = (iOfdmSymbIdx - 4) % numberofblocksperCIF;
  memcpy(&cif_In[CIF_index * BitsperBlock], ibits.data(), BitsperBlock * sizeof(i16));
  if (CIF_index == numberofblocksperCIF - 1)
  {
    for (i32 i = 0; i < 3072 * 18; i++)
    {
      i32 index = interleaveMap[i & 017];
      temp[i] = cifVector[(index_Out + index) & 017][i];
      cifVector[index_Out & 0xF][i] = cif_In[i];
    }
    //	we have to wait until the interleave matrix is filled
    if (amount < 15)
    {
      amount++;
      index_Out = (index_Out + 1) & 017;
      // Minor is introduced to inform the init_eti function about the CIF number in the dab frame, it runs from 0 .. 3
      Minor = 0;
      return;    // wait until next time
    }
    //
    //	Otherwise, it becomes serious
    if (CIFCount_hi < 0 || CIFCount_lo < 0)
    {
      return;
    }
    //
    //	3 steps, init the vector, add the fib and add the CIF content
    i32 offset = _init_eti(theVector, CIFCount_hi, CIFCount_lo, Minor);
    i32 base = offset;
    memcpy(&theVector[offset], fibVector[index_Out], 96);
    offset += 96;
    //
    //
    //	oef, here we go for handling the CIF
    offset = _process_cif(temp, theVector, offset);
    //
    //	EOF - CRC
    //	The "data bytes" are stored in the range base .. offset
    i32 crc = calc_crc(&(theVector[base]), offset - base, crctab_1021, 0xFFFF);
    crc = ~crc;
    theVector[offset++] = (crc & 0xFF00) >> 8;
    theVector[offset++] = crc & 0xFF;
    //
    //	EOF - RFU
    theVector[offset++] = 0xFF;
    theVector[offset++] = 0xFF;
    //
    //	TIST	- 0xFFFFFFFF means time stamp not used
    theVector[offset++] = 0xFF;
    theVector[offset++] = 0xFF;
    theVector[offset++] = 0xFF;
    theVector[offset++] = 0xFF;
    //
    //	Padding
    memset(&theVector[offset], 0x55, 6144 - offset);
    if (etiFile != nullptr)
    {
      fwrite(theVector, 1, 6144, etiFile);
    }
    //	at the end, go for a new eti vector
    index_Out = (index_Out + 1) & 017;
    Minor++;
  }
}

//	Copied  from dabtools:
i32 EtiGenerator::_init_eti(u8 * oEti, i16 CIFCount_hi, i16 CIFCount_lo, i16 minor)
{
  i32 fillPointer = 0;
  ChannelData data;

  CIFCount_lo += minor;
  if (CIFCount_lo >= 250)
  {
    CIFCount_lo = CIFCount_lo % 250;
    CIFCount_hi++;
  }
  if (CIFCount_hi >= 20)
  {
    CIFCount_hi = 20;
  }

  //	SYNC()
  //	ERR
  //	if (fibValid [index_Out + minor])
  oEti[fillPointer++] = 0xFF;    // error level 0
  //	else
  //	   eti [fillPointer ++] = 0x0F;		// error level 2, fib errors
  //	FSYNC
  if (CIFCount_lo & 1)
  {
    oEti[fillPointer++] = 0xf8;
    oEti[fillPointer++] = 0xc5;
    oEti[fillPointer++] = 0x49;
  }
  else
  {
    oEti[fillPointer++] = 0x07;
    oEti[fillPointer++] = 0x3a;
    oEti[fillPointer++] = 0xb6;
  }
  //	LIDATA ()
  //	FC()
  oEti[fillPointer++] = CIFCount_lo; // FCT from CIFCount_lo
  i32 FICF = 1;      // FIC present in MST
  i32 NST = 0;      // number of streams
  i32 FL = 0;      // Frame Length
  for (i32 j = 0; j < 64; j++)
  {
    my_ficHandler->get_channel_info(&data, j);
    if (data.in_use)
    {
      NST++;
      FL += (data.bitrate * 3) / 4;    // words remember
    }
  }
  //
  FL += NST + 1 + 24; // STC + EOH + MST (FIC data, Mode 1!)
  oEti[fillPointer++] = (FICF << 7) | NST;
  //
  //	The FP is computed as remainder of the total CIFCount,
  u8 FP = ((CIFCount_hi * 250) + CIFCount_lo) % 8;
  //
  i32 MID = 0x01; // We only support Mode 1
  oEti[fillPointer++] = (FP << 5) | (MID << 3) | ((FL & 0x700) >> 8);
  oEti[fillPointer++] = FL & 0xff;
  //	Now for each of the streams in the FIC we add information
  //	on how to get it
  //	STC ()
  for (i32 j = 0; j < 64; j++)
  {
    my_ficHandler->get_channel_info(&data, j);
    if (data.in_use)
    {
      i32 SCID = data.id;
      i32 SAD = data.start_cu;
      i32 TPL;
      if (data.uepFlag)
      {
        TPL = 0x10 | (data.protlev - 1);
      }
      else
      {
        TPL = 0x20 | data.protlev;
      }
      i32 STL = data.bitrate * 3 / 8;
      oEti[fillPointer++] = (SCID << 2) | ((SAD & 0x300) >> 8);
      oEti[fillPointer++] = SAD & 0xFF;
      oEti[fillPointer++] = (TPL << 2) | ((STL & 0x300) >> 8);
      oEti[fillPointer++] = STL & 0xFF;
    }
  }
  //	EOH ()
  //	MNSC
  oEti[fillPointer++] = 0xFF;
  oEti[fillPointer++] = 0xFF;
  //	HCRC
  i32 HCRC = calc_crc(&oEti[4], fillPointer - 4, crctab_1021, 0xffff);
  HCRC = ~HCRC;
  oEti[fillPointer++] = (HCRC & 0xff00) >> 8;
  oEti[fillPointer++] = HCRC & 0xff;

  return fillPointer;
}

//	In process_CIF we iterate over the data in the CIF and map that
//	upon a segment in the eti vector
//
//	Since from the subchannel data we know the location in
//	the input vector, the output vector and the
//	parameters for deconvolution, we can do
//	the processing in parallel. So, for each subchannel
//	we just launch a task
class parameter
{
public:
  const i16 * input;
  bool uepFlag;
  i32 bitRate;
  i32 protLevel;
  i32 start_cu;
  i32 size;
  u8 * output;
};

i32 EtiGenerator::_process_cif(const i16 * input, u8 * output, i32 offset)
{
  u8 shiftRegister[9];
  std::vector<parameter *> theParameters;

  for (i32 i = 0; i < 64; i++)
  {
    ChannelData data;
    my_ficHandler->get_channel_info(&data, i);
    if (data.in_use)
    {
      parameter * t = new parameter;
      t->input = input;
      t->uepFlag = data.uepFlag;
      t->bitRate = data.bitrate;
      t->protLevel = data.protlev;
      t->start_cu = data.start_cu;
      t->size = data.size;
      t->output = &output[offset];
      offset += data.bitrate * 24 / 8;

      if (protTable[i] == nullptr)
      {
        if (t->uepFlag)
        {
          protTable[i] = new UepProtection(t->bitRate, t->protLevel);
        }
        else
        {
          protTable[i] = new EepProtection(t->bitRate, t->protLevel);
        }

        memset(shiftRegister, 1, 9);
        descrambler[i] = new u8[24 * t->bitRate];

        for (i32 j = 0; j < 24 * t->bitRate; j++)
        {
          u8 b = shiftRegister[8] ^ shiftRegister[4];
          for (i32 k = 8; k > 0; k--)
          {
            shiftRegister[k] = shiftRegister[k - 1];
          }
          shiftRegister[0] = b;
          descrambler[i][j] = b;
        }
      }
      //	we need to save a reference to the parameters
      //	since we have to delete the instance later on
      _process_sub_channel(i, t, protTable[i], descrambler[i]);
    }
  }
  return offset;
}

void EtiGenerator::_process_sub_channel(i32 /*nr*/, parameter * p, Protection * prot, u8 * desc)
{
  std::unique_ptr<u8[]> outVector{ new u8[24 * p->bitRate] };
  if (!outVector)
  {
    std::cerr << "process_subCh - alloc fail";
    return;
  }

  memset(outVector.get(), 0, sizeof(u8) * 24 * p->bitRate);

  prot->deconvolve(&p->input[p->start_cu * CUSize], p->size * CUSize, outVector.get());
  //
  for (i32 j = 0; j < 24 * p->bitRate; j++)
  {
    outVector[j] ^= desc[j];
  }
  //
  //	and the storage:
  for (i32 j = 0; j < 24 * p->bitRate / 8; j++)
  {
    i32 temp = 0;
    for (i32 k = 0; k < 8; k++)
    {
      temp = (temp << 1) | (outVector[j * 8 + k] & 01);
    }
    p->output[j] = temp;
  }

}

bool EtiGenerator::start_eti_generator(const QString & f)
{
  reset();
  std::lock_guard lock(mMutex);
  etiFile = fopen(f.toUtf8().data(), "wb");
  return etiFile != nullptr;
}

void EtiGenerator::stop_eti_generator()
{
  std::lock_guard lock(mMutex);
  if (etiFile != nullptr)
  {
    fclose(etiFile);
  }
  etiFile = nullptr;
  running = false;
}

