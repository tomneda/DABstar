/*
 * This file is adapted by old-dab and Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */


#include "tii-detector.h"

// TII pattern for transmission modes I, II and IV
static const uint8_t cTable[] = {
  0x0f,	// 0 0 0 0 1 1 1 1		0
  0x17,	// 0 0 0 1 0 1 1 1		1
  0x1b,	// 0 0 0 1 1 0 1 1		2
  0x1d,	// 0 0 0 1 1 1 0 1		3
  0x1e,	// 0 0 0 1 1 1 1 0		4
  0x27,	// 0 0 1 0 0 1 1 1		5
  0x2b,	// 0 0 1 0 1 0 1 1		6
  0x2d,	// 0 0 1 0 1 1 0 1		7
  0x2e,	// 0 0 1 0 1 1 1 0		8
  0x33,	// 0 0 1 1 0 0 1 1		9
  0x35,	// 0 0 1 1 0 1 0 1		10
  0x36,	// 0 0 1 1 0 1 1 0		11
  0x39,	// 0 0 1 1 1 0 0 1		12
  0x3a,	// 0 0 1 1 1 0 1 0		13
  0x3c,	// 0 0 1 1 1 1 0 0		14
  0x47,	// 0 1 0 0 0 1 1 1		15
  0x4b,	// 0 1 0 0 1 0 1 1		16
  0x4d,	// 0 1 0 0 1 1 0 1		17
  0x4e,	// 0 1 0 0 1 1 1 0		18
  0x53,	// 0 1 0 1 0 0 1 1		19
  0x55,	// 0 1 0 1 0 1 0 1		20
  0x56,	// 0 1 0 1 0 1 1 0		21
  0x59,	// 0 1 0 1 1 0 0 1		22
  0x5a,	// 0 1 0 1 1 0 1 0		23
  0x5c,	// 0 1 0 1 1 1 0 0		24
  0x63,	// 0 1 1 0 0 0 1 1		25
  0x65,	// 0 1 1 0 0 1 0 1		26
  0x66,	// 0 1 1 0 0 1 1 0		27
  0x69,	// 0 1 1 0 1 0 0 1		28
  0x6a,	// 0 1 1 0 1 0 1 0		29
  0x6c,	// 0 1 1 0 1 1 0 0		30
  0x71,	// 0 1 1 1 0 0 0 1		31
  0x72,	// 0 1 1 1 0 0 1 0		32
  0x74,	// 0 1 1 1 0 1 0 0		33
  0x78,	// 0 1 1 1 1 0 0 0		34
  0x87,	// 1 0 0 0 0 1 1 1		35
  0x8b,	// 1 0 0 0 1 0 1 1		36
  0x8d,	// 1 0 0 0 1 1 0 1		37
  0x8e,	// 1 0 0 0 1 1 1 0		38
  0x93,	// 1 0 0 1 0 0 1 1		39
  0x95,	// 1 0 0 1 0 1 0 1		40
  0x96,	// 1 0 0 1 0 1 1 0		41
  0x99,	// 1 0 0 1 1 0 0 1		42
  0x9a,	// 1 0 0 1 1 0 1 0		43
  0x9c,	// 1 0 0 1 1 1 0 0		44
  0xa3,	// 1 0 1 0 0 0 1 1		45
  0xa5,	// 1 0 1 0 0 1 0 1		46
  0xa6,	// 1 0 1 0 0 1 1 0		47
  0xa9,	// 1 0 1 0 1 0 0 1		48
  0xaa,	// 1 0 1 0 1 0 1 0		49
  0xac,	// 1 0 1 0 1 1 0 0		50
  0xb1,	// 1 0 1 1 0 0 0 1		51
  0xb2,	// 1 0 1 1 0 0 1 0		52
  0xb4,	// 1 0 1 1 0 1 0 0		53
  0xb8,	// 1 0 1 1 1 0 0 0		54
  0xc3,	// 1 1 0 0 0 0 1 1		55
  0xc5,	// 1 1 0 0 0 1 0 1		56
  0xc6,	// 1 1 0 0 0 1 1 0		57
  0xc9,	// 1 1 0 0 1 0 0 1		58
  0xca,	// 1 1 0 0 1 0 1 0		59
  0xcc,	// 1 1 0 0 1 1 0 0		60
  0xd1,	// 1 1 0 1 0 0 0 1		61
  0xd2,	// 1 1 0 1 0 0 1 0		62
  0xd4,	// 1 1 0 1 0 1 0 0		63
  0xd8,	// 1 1 0 1 1 0 0 0		64
  0xe1,	// 1 1 1 0 0 0 0 1		65
  0xe2,	// 1 1 1 0 0 0 1 0		66
  0xe4,	// 1 1 1 0 0 1 0 0		67
  0xe8,	// 1 1 1 0 1 0 0 0		68
  0xf0	// 1 1 1 1 0 0 0 0		69
};

static const int8_t cReftable[] = {
   2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,
   1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,
   0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,
  -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1,
   2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,
   1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,
   0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,
  -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1,
   2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,
   1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,
   0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,
  -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1,
   2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,
   1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,
   0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,
  -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1,
   2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,
   1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,
   0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,
  -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1,
   2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,
   1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,
   0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,
  -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1,
   2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,
  -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1,
   0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,
   1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,
   2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,
  -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1,
   0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,
   1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,
   2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,
  -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1,
   0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,
   1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,
   2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,
  -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1,
   0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,
   1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,
   2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,
  -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1,
   0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,
   1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,
   2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,  2,  0,  0,  0,
  -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1,
   0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,  0,  2,  2,  2,
   1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1
};

TiiDetector::TiiDetector(const uint8_t iDabMode)
  : mParams(iDabMode)
  , mT_u(mParams.get_T_u())
  , mT_g(mParams.get_T_g())
  , mK(mParams.get_K())
  , mFftHandler(mT_u, false)
{
  mNullSymbolBufferVec.resize(mT_u);
  reset();
}

void TiiDetector::set_collisions(bool b)
{
  mCollisions = b;
}

void TiiDetector::set_subid(uint8_t subid)
{
  mSelectedSubId = subid;
}

void TiiDetector::_resetBuffer()
{
  for (auto & i : mNullSymbolBufferVec) i = cmplx(0, 0);
}

void TiiDetector::reset()
{
  _resetBuffer();
  for (auto & i : mDecodedBufferArr) i = cmplx(0, 0);
}

// To reduce noise in the input signal, we might
// add a few spectra before computing (up to the user)
void TiiDetector::add_to_tii_buffer(const std::vector<cmplx> & v)
{
  auto * const buffer = make_vla(cmplx, mT_u);
  memcpy(buffer, &(v[mT_g]), mT_u * sizeof(cmplx));
  mFftHandler.fft(buffer);
  for (int i = 0; i < mT_u; i++)
    mNullSymbolBufferVec[i] += buffer[i];
}

void TiiDetector::_decode(const std::vector<cmplx> & iVec, TBufferArr & ioVec) const
{
  for (int32_t idx = -mK / 2, i = 0; idx < mK / 2; idx += 2, ++i)
  {
    const int32_t fftIdx = fft_shift_skip_dc(idx, mT_u);
    ioVec[i] += iVec[fftIdx] * conj(iVec[fftIdx + 1]);
  }
}

// we map the "K" carriers (complex values) onto
// a collapsed vector of "K / 8" length,
// considered to consist of 8 segments of 24 values
// Each "value" is the sum of 4 pairs of subsequent carriers,
// taken from the 4 quadrants -768 .. 385, 384 .. -1, 1 .. 384, 385 .. 768
void TiiDetector::_collapse(const TBufferArr & iVec, TCmplxTable & oEtsiVec, TCmplxTable & oNonEtsiVec) const
{
  assert(mK / 2 == (int)iVec.size());
  auto * const buffer = make_vla(cmplx, mK / 2);
  memcpy(buffer, iVec.data(), mK / 2 * sizeof(cmplx));

  // a single carrier cannot be a TII carrier.
  if (mCarrierDelete)
  {
    for (int i = 0; i < 192; i++)
    {
      std::array<float, 4> x;
      float max = 0;
      float sum = 0;
      int index = 0;
      for (int j = 0; j < 4; j++)
      {
        x[j] = abs(buffer[i + j * 192]);
        sum += x[j];
        if (x[j] > max)
        {
          max = x[j];
          index = j;
        }
      }
      float min = (sum - max) / 3;
      if (sum < max * 1.5 && max > 0.0)
      {
        buffer[i + index * 192] *= min / max;
        //fprintf(stderr, "carrier index %d\n", i+index*192);
      }
    }
  }
  for (int i = 0; i < 192; i++)
  {
    oEtsiVec[i] = cmplx(0, 0);
    oNonEtsiVec[i] = cmplx(0, 0);
    for (int j = 0; j < 4; j++)
    {
      cmplx x = buffer[i + j * 192];
      oEtsiVec[i] += x;
      oNonEtsiVec[i] += std::polar(abs(x), arg(x) - (float)cReftable[i + j * 192] * F_M_PI_2);
    }
  }
}

// We determine first the offset of the "best fit", the offset indicates the subId
/*static*/ const std::array<const uint8_t, TiiDetector::cNumGroups> TiiDetector::cBits = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

// Sort the elements according to their strength
static int _fcmp(const void * a, const void * b)
{
  const STiiResult * element1 = (const STiiResult *)a;
  const STiiResult * element2 = (const STiiResult *)b;

  if (element1->strength > element2->strength) return -1;
  if (element1->strength < element2->strength) return  1;

  return 0;
}

std::vector<STiiResult> TiiDetector::process_tii_data(int16_t threshold_db)
{
  TFloatTable etsi_floatTable;    // collapsed ETSI float values
  TFloatTable nonEtsi_floatTable; // collapsed non-ETSI float values
  TCmplxTable etsiTable;          // collapsed ETSI complex values
  TCmplxTable nonEtsiTable;       // collapsed non-ETSI complex values
  float max = 0;      // abs value of the strongest carrier
  float noise = 1e9;  // noise level
  std::vector<STiiResult> theResult; // results
  float threshold = std::pow(10.0f, (float)threshold_db / 10.0f); // threshold above noise

  _decode(mNullSymbolBufferVec, mDecodedBufferArr);
  _collapse(mDecodedBufferArr, etsiTable, nonEtsiTable);

  // fill the float tables, determine the abs value of the strongest carrier
  for (int i = 0; i < cNumGroups * cGroupSize; i++)
  {
    float x = abs(etsiTable[i]);
    etsi_floatTable[i] = x;
    if (x > max) max = x;

    x = abs(nonEtsiTable[i]);
    nonEtsi_floatTable[i] = x;
    if (x > max) max = x;
  }

  // determine the noise level of the lowest group
  for (int subId = 0; subId < cGroupSize; subId++)
  {
    float avg = 0;
    for (int i = 0; i < cNumGroups; i++)
    {
      avg += etsi_floatTable[subId + i * cGroupSize];
    }
    avg /= cNumGroups;
    if (avg < noise) noise = avg;
  }

  for (int subId = 0; subId < cGroupSize; subId++)
  {
    STiiResult element;
    cmplx sum = cmplx(0, 0);
    cmplx etsi_sum = cmplx(0, 0);
    cmplx nonetsi_sum = cmplx(0, 0);
    int count = 0;
    int etsi_count = 0;
    int nonetsi_count = 0;
    int pattern = 0;
    int etsi_pattern = 0;
    int nonetsi_pattern = 0;
    int mainId = 0;
    bool norm = false;
    const cmplx * cmplx_ptr = nullptr;
    const float * float_ptr = nullptr;

    // The number of times the limit is reached in the group is counted
    for (int i = 0; i < cNumGroups; i++)
    {
      if (etsi_floatTable[subId + i * cGroupSize] > noise * threshold)
      {
        etsi_count++;
        etsi_pattern |= (0x80 >> i);
        etsi_sum += etsiTable[subId + cGroupSize * i];
      }
      if (nonEtsi_floatTable[subId + i * cGroupSize] > noise * threshold)
      {
        nonetsi_count++;
        nonetsi_pattern |= (0x80 >> i);
        nonetsi_sum += nonEtsiTable[subId + cGroupSize * i];
      }
    }
    if ((etsi_count >= 4) || (nonetsi_count >= 4))
    {
      if (abs(nonetsi_sum) > abs(etsi_sum))
      {
        norm = true;
        sum = nonetsi_sum;
        cmplx_ptr = nonEtsiTable.data();
        float_ptr = nonEtsi_floatTable.begin();
        count = nonetsi_count;
        pattern = nonetsi_pattern;
      }
      else
      {
        sum = etsi_sum;
        cmplx_ptr = etsiTable.data();
        float_ptr = etsi_floatTable.data();
        count = etsi_count;
        pattern = etsi_pattern;
      }
    }

    // Find the Main Id that matches the pattern
    if (count == 4)
    {
      for (; mainId < (int)sizeof(cTable); mainId++)
      {
        if (cTable[mainId] == pattern)
          break;
      }
    }
    // Find the best match. We extract the four max values as bits
    else if (count > 4)
    {
      float mm = 0;
      for (int k = 0; k < (int)sizeof(cTable); k++)
      {
        cmplx val = cmplx(0, 0);
        for (int i = 0; i < cNumGroups; i++)
        {
          if (cTable[k] & cBits[i])
            val += cmplx_ptr[subId + cGroupSize * i];
        }

        if (abs(val) > mm)
        {
          mm = abs(val);
          sum = val;
          mainId = k;
        }
      }
    }
    // List the result
    if (count >= 4)
    {
      element.mainId = mainId;
      element.subId = subId;
      element.strength = abs(sum) / max / 4;
      element.phase = arg(sum) * F_DEG_PER_RAD;
      element.norm = norm;
      theResult.push_back(element);
    }

    if ((count > 4) && mCollisions)
    {
      sum = cmplx(0, 0);

      // Calculate the level of the second main ID
      for (int i = 0; i < cNumGroups; i++)
      {
        if ((cTable[mainId] & cBits[i]) == 0)
        {
          int index = subId + cGroupSize * i;
          if (float_ptr[index] > noise * threshold)
            sum += cmplx_ptr[index];
        }
      }

      if (subId == mSelectedSubId) // List all possible main IDs
      {
        for (int k = 0; k < (int)sizeof(cTable); k++)
        {
          int pattern2 = cTable[k] & pattern;
          int count2 = 0;

          for (int i = 0; i < cNumGroups; i++)
          {
            if (pattern2 & cBits[i]) count2++;
          }

          if ((count2 == 4) && (k != mainId))
          {
            element.mainId = k;
            element.strength = abs(sum) / max / (count - 4);
            element.phase = arg(sum) * F_DEG_PER_RAD;
            element.norm = norm;
            theResult.push_back(element);
          }
        }
      }
      else // List only additional main ID 99
      {
        element.mainId = 99;
        element.strength = abs(sum) / max / (count - 4);
        element.phase = arg(sum) * F_DEG_PER_RAD;
        element.norm = norm;
        theResult.push_back(element);
      }
    }
  }
  //fprintf(stderr, "max =%.0f, noise = %.1fdB\n", max, 10 * log10(noise/max));
  if (max > 4'000'000)
  {
    for (int i = 0; i < mK / 2; i++)
      mDecodedBufferArr[i] *= 0.9f;
  }
  _resetBuffer();
  qsort(theResult.data(), theResult.size(), sizeof(STiiResult), &_fcmp);

  return theResult;
}
