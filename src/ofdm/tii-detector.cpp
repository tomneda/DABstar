/*
 * This file is adapted by old-dab and Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */


#include "tii-detector.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(sLogTiiDetector, "TiiDetector", QtInfoMsg)


// TII pattern for transmission modes I, II and IV
static constexpr std::array<const uint8_t, 70> cMainIdPatternTable = {
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

static constexpr std::array<const int8_t, 768> cPhaseCorrTable = {
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

// Sort the elements according to their strength
static int fcmp(const void * a, const void * b)
{
  const auto * element1 = (const STiiResult *)a;
  const auto * element2 = (const STiiResult *)b;

  if (element1->strength > element2->strength) return -1;
  if (element1->strength < element2->strength) return  1;

  return 0;
}

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

void TiiDetector::reset()
{
  _resetBuffer();
  for (auto & i : mDecodedBufferArr) i = cmplx(0, 0);
}

// To reduce noise in the input signal, we might
// add a few spectra before computing (up to the user)
void TiiDetector::add_to_tii_buffer(const std::vector<cmplx> & v)
{
  assert((int)v.size() >= mT_g + mT_u);
  auto * const buffer = make_vla(cmplx, mT_u);
  memcpy(buffer, &(v[mT_g]), mT_u * sizeof(cmplx));

  mFftHandler.fft(buffer);

  for (int i = 0; i < mT_u; i++)
  {
    mNullSymbolBufferVec[i] += buffer[i];
  }
}

std::vector<STiiResult> TiiDetector::process_tii_data(const int16_t iThreshold_db)
{
  _conv_fft_carrier_pairs(mDecodedBufferArr, mNullSymbolBufferVec);

  TFloatTable192 etsiFloatTable;    // collapsed ETSI float values
  TFloatTable192 nonEtsiFloatTable; // collapsed non-ETSI float values
  TCmplxTable192 etsiCmplxTable;    // collapsed ETSI complex values
  TCmplxTable192 nonEtsiCmplxTable; // collapsed non-ETSI complex values

  _collapse(etsiCmplxTable, nonEtsiCmplxTable, mDecodedBufferArr);

  // fill the float tables, determine the abs value of the strongest carrier
  float max = 0;      // abs value of the strongest carrier
  _get_float_table_and_max_value(etsiFloatTable, etsiCmplxTable, max);
  _get_float_table_and_max_value(nonEtsiFloatTable, nonEtsiCmplxTable, max);

  // determine the noise level of the lowest group
  // set noise level to lowest found noise over each subId
  const float noise = _calculate_average_noise(etsiFloatTable);

  std::vector<STiiResult> theResult; // results

  for (int subId = 0; subId < cGroupSize24; subId++)
  {
    cmplx sum = cmplx(0, 0);
    int count = 0;
    int pattern = 0;
    bool isNonEtsiPhase = false;
    const float threshold = std::pow(10.0f, (float)iThreshold_db / 10.0f); // threshold above noise

    _comp_etsi_and_non_etsi(isNonEtsiPhase, count, sum, pattern,
                            etsiFloatTable, nonEtsiFloatTable, etsiCmplxTable, nonEtsiCmplxTable, noise * threshold, subId);

    const TCmplxTable192 & cmplxTable = (isNonEtsiPhase ? nonEtsiCmplxTable : etsiCmplxTable);
    const TFloatTable192 & floatTable = (isNonEtsiPhase ? nonEtsiFloatTable : etsiFloatTable);

    // Find the MainId that matches the pattern
    int mainId = 0;

    if (count == 4)
    {
      mainId = _find_exact_main_id_match(pattern);
    }
    // Find the best match. We extract the four max values as bits
    else if (count > 4)
    {
      mainId = _find_best_main_id_match(sum, subId, cmplxTable);
    }

    // List the result
    if (count >= 4)
    {
      STiiResult element;
      element.mainId = mainId;
      element.subId = subId;
      element.strength = abs(sum) / max / 4;
      element.phase = arg(sum) * F_DEG_PER_RAD;
      element.norm = isNonEtsiPhase;
      theResult.push_back(element);
    }

    if ((count > 4) && mCollisions)
    {
      _find_collisions(theResult, max, noise * threshold, subId, count, pattern, mainId, isNonEtsiPhase, cmplxTable, floatTable);
    }
  }

  //fprintf(stderr, "max =%.0f, noise = %.1fdB\n", max, 10 * log10(noise/max));
  // avoid value overflow
  if (max > 4'000'000)
  {
    for (int i = 0; i < mK / 2; i++)
    {
      mDecodedBufferArr[i] *= 0.9f;
    }
  }

  _resetBuffer();

  qsort(theResult.data(), theResult.size(), sizeof(STiiResult), &fcmp); // sort data due to its strength (high to low)

  return theResult;
}

void TiiDetector::_resetBuffer()
{
  for (auto & i : mNullSymbolBufferVec) i = cmplx(0, 0);
}

void TiiDetector::_conv_fft_carrier_pairs(TBufferArr768 & ioVec, const std::vector<cmplx> & iVec) const
{
  for (int32_t k = -mK / 2, i = 0; k < mK / 2; k += 2, ++i)
  {
    const int32_t fftIdx = fft_shift_skip_dc(k, mT_u);
    const cmplx prod = iVec[fftIdx] * conj(iVec[fftIdx + 1]); // TII carriers are given in pairs
    ioVec[i] += prod;
    if (std::abs(prod) > 1000.0f)
    {
      const int32_t k2 = (k >= 0 ? k + 1 : k); // skip DC
      qCInfo(sLogTiiDetector) << "FFT carrier pair k=" << k2 << ", ioVec " << cmplx_to_polar_str(ioVec[i]) << " += "
                              << cmplx_to_polar_str(iVec[fftIdx]) << " * " << cmplx_to_polar_str(iVec[fftIdx + 1])
                              << " = " << cmplx_to_polar_str(iVec[fftIdx] * conj(iVec[fftIdx + 1]));
    }
  }
}

void TiiDetector::_remove_single_carrier_values(TBufferArr768 & ioBuffer) const
{
  for (int i = 0; i < cBlockSize192; i++)
  {
    std::array<float, cNumBlocks4> x;
    float max = 0;
    float sum = 0;
    int index = 0;

    for (int j = 0; j < cNumBlocks4; j++)
    {
      x[j] = abs(ioBuffer[i + j * cBlockSize192]);
      sum += x[j];

      if (x[j] > max)
      {
        max = x[j];
        index = j;
      }
    }

    const float min = (sum - max) / (cNumBlocks4 - 1);

    if (sum < max * 1.5 && max > 0.0)
    {
      ioBuffer[i + index * cBlockSize192] *= min / max;
      // fprintf(stderr, "carrier index %d = %d + %d * 192 (%d)\n", i+index*192, i, index, (i+index*192)*2 - 768);
    }
  }
}

// we map the "K" carriers (complex values) onto
// a collapsed vector of "K / 8" length,
// considered to consist of 8 segments of 24 values
// Each "value" is the sum of 4 pairs of subsequent carriers,
// taken from the 4 quadrants -768 .. 385, 384 .. -1, 1 .. 384, 385 .. 768
void TiiDetector::_collapse(TCmplxTable192 & ioEtsiVec, TCmplxTable192 & ioNonEtsiVec, const TBufferArr768 & iVec) const
{
  assert(mK / 2 == (int)iVec.size());
  TBufferArr768 buffer = iVec;

  // a single carrier cannot be a TII carrier.
  if (mCarrierDelete)
  {
    _remove_single_carrier_values(buffer);
  }

  for (int i = 0; i < cBlockSize192; i++)
  {
    ioEtsiVec[i] = cmplx(0, 0);
    ioNonEtsiVec[i] = cmplx(0, 0);

    for (int blockIdx = 0; blockIdx < cNumBlocks4; blockIdx++)
    {
      const cmplx x = buffer[i + blockIdx * cBlockSize192];
      ioEtsiVec[i] += x;
      // correct assumed wrong send carrier pair phase difference
      ioNonEtsiVec[i] += std::polar(abs(x), arg(x) - (float)cPhaseCorrTable[i + blockIdx * cBlockSize192] * F_M_PI_2);
    }
  }
}

int TiiDetector::_find_exact_main_id_match(int iPattern) const
{
  for (int mainId = 0; mainId < (int)cMainIdPatternTable.size(); mainId++)
  {
    if (cMainIdPatternTable[mainId] == iPattern)
    {
      return mainId;
    }
  }
  assert(false);
  return -1; // should never happen
}

int TiiDetector::_find_best_main_id_match(cmplx & oSum, const int iSubId, const TCmplxTable192 & ipCmplxTable) const
{
  int oMainId = -1;
  float maxLevel = 0;
  oSum = cmplx(0, 0);

  for (int k = 0; k < (int)cMainIdPatternTable.size(); k++)
  {
    cmplx val = cmplx(0, 0);

    for (int i = 0; i < cNumGroups8; i++)
    {
      if (cMainIdPatternTable[k] & (0x80 >> i))
      {
        val += ipCmplxTable[iSubId + cGroupSize24 * i];
      }
    }

    if (abs(val) > maxLevel)
    {
      maxLevel = abs(val);
      oSum = val;
      oMainId = k;
    }
  }
  assert(oMainId != -1);
  return oMainId;
}

void TiiDetector::_comp_etsi_and_non_etsi(bool & oIsNonEtsiPhase, int & oCount, cmplx & oSum, int & oPattern,
                                          const TFloatTable192 & iEtsiFloatTable, const TFloatTable192 & iNonEtsiFloatTable,
                                          const TCmplxTable192 & iEtsiCmplxTable, const TCmplxTable192 & iNonEtsiCmplxTable, const float iThresholdLevel, int iSubId) const
{
  cmplx etsi_sum = cmplx(0, 0);
  cmplx nonEtsi_sum = cmplx(0, 0);
  int etsi_count = 0;
  int nonEtsi_count = 0;
  int etsi_pattern = 0;
  int nonEtsi_pattern = 0;
  oIsNonEtsiPhase = false;
  oCount = 0;
  oSum = cmplx(0, 0);

  // The number of times the limit is reached in the group is counted
  for (int i = 0; i < cNumGroups8; i++)
  {
    if (iEtsiFloatTable[iSubId + i * cGroupSize24] > iThresholdLevel)
    {
      etsi_count++;
      etsi_pattern |= (0x80 >> i);
      etsi_sum += iEtsiCmplxTable[iSubId + cGroupSize24 * i];
    }

    if (iNonEtsiFloatTable[iSubId + i * cGroupSize24] > iThresholdLevel)
    {
      nonEtsi_count++;
      nonEtsi_pattern |= (0x80 >> i);
      nonEtsi_sum += iNonEtsiCmplxTable[iSubId + cGroupSize24 * i];
    }
  }

  if ((etsi_count >= 4) || (nonEtsi_count >= 4))
  {
    if (abs(nonEtsi_sum) > abs(etsi_sum)) // TODO: is that comp. valid if one count is < 4? I guess yes as the sum with count < 4 got too low
    {
      oIsNonEtsiPhase = true;
    }
  }

  if (oIsNonEtsiPhase)
  {
    oSum = nonEtsi_sum;
    oCount = nonEtsi_count;
    oPattern = nonEtsi_pattern;
  }
  else
  {
    oSum = etsi_sum;
    oCount = etsi_count;
    oPattern = etsi_pattern;
  }
}

void TiiDetector::_find_collisions(std::vector<STiiResult> ioResultVec, float iMax, float iThresholdLevel,
                                   int iSubId, int iCount, int iPattern, int iMainId, bool iNorm,
                                   const TCmplxTable192 & iCmplxTable, const TFloatTable192 & iFloatTable) const
{
  assert(iCount > 4);
  cmplx sum = cmplx(0, 0);

  // Calculate the level of the second main ID
  for (int i = 0; i < cNumGroups8; i++)
  {
    if ((cMainIdPatternTable[iMainId] & (0x80 >> i)) == 0)
    {
      if (const int index = iSubId + cGroupSize24 * i;
          iFloatTable[index] > iThresholdLevel)
      {
        sum += iCmplxTable[index];
      }
    }
  }

  if (iSubId == mSelectedSubId) // List all possible main IDs
  {
    for (int k = 0; k < (int)cMainIdPatternTable.size(); k++)
    {
      const int pattern2 = cMainIdPatternTable[k] & iPattern;
      int count2 = 0;

      for (int i = 0; i < cNumGroups8; i++)
      {
        if (pattern2 & (0x80 >> i)) count2++;
      }

      if ((count2 == 4) && (k != iMainId))
      {
        STiiResult element;
        element.mainId = k;
        element.subId = iSubId;
        element.strength = abs(sum) / iMax / (float)(iCount - 4);
        element.phase = arg(sum) * F_DEG_PER_RAD;
        element.norm = iNorm;
        ioResultVec.push_back(element);
      }
    }
  }
  else // List only additional main ID 99
  {
    STiiResult element;
    element.mainId = 99;
    element.subId = iSubId;
    element.strength = abs(sum) / iMax / (float)(iCount - 4);
    element.phase = arg(sum) * F_DEG_PER_RAD;
    element.norm = iNorm;
    ioResultVec.push_back(element);
  }
}

void TiiDetector::_get_float_table_and_max_value(TFloatTable192 & oFloatTable, const TCmplxTable192 & iCmplxTable, float & ioMax) const
{
  for (int i = 0; i < cNumGroups8 * cGroupSize24; i++)
  {
    const float x = abs(iCmplxTable[i]);
    oFloatTable[i] = x;
    if (x > ioMax) ioMax = x;
  }
}

float TiiDetector::_calculate_average_noise(const TFloatTable192 & iFloatTable)
{
  float noise = 1e9;

  for (int subId = 0; subId < cGroupSize24; subId++)
  {
    float avg = 0;
    for (int i = 0; i < cNumGroups8; i++)
    {
      avg += iFloatTable[subId + i * cGroupSize24];
    }
    avg /= cNumGroups8;
    if (avg < noise) noise = avg;
  }
  return noise;
}

