
#include	<iostream>
#include	"eti-generator.h"
#include	"eep-protection.h"
#include	"uep-protection.h"
#include	"crc.h"
#include	<memory>


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
    u16 crc = calc_crc(&(theVector[base]), offset - base);
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
  u16 HCRC = calc_crc(&oEti[4], fillPointer - 4);
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

