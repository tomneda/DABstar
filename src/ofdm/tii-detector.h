/*
 * This file is adapted by old-dab and Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

#pragma once

#include "dab-constants.h"
#include  "dab-params.h"
#include "fft/fft-handler.h"

struct STiiResult
{
  uint8_t mainId;
  uint8_t subId;
  float strength;
  float phase;
  bool norm;
};

class TiiDetector
{
public:
  explicit TiiDetector(uint8_t iDabMode);
  ~TiiDetector() = default;

  void reset();
  void set_collisions(bool);
  void set_subid(uint8_t);
  void add_to_tii_buffer(const std::vector<cmplx> &);
  std::vector<STiiResult> process_tii_data(int16_t);

private:
  static constexpr int32_t cNumGroups =  8;
  static constexpr int32_t cGroupSize = 24;

  using TBufferArr  = std::array<cmplx, 768>;
  using TFloatTable = std::array<float, cNumGroups * cGroupSize>;
  using TCmplxTable = std::array<cmplx, cNumGroups * cGroupSize>;

  static const std::array<const uint8_t, cNumGroups> cBits;
  const DabParams mParams;
  const int16_t mT_u;
  const int16_t mT_g;
  const int16_t mK;
  bool mCollisions = false;
  bool mCarrierDelete = true;
  uint8_t mSelectedSubId = 0;
  TBufferArr mDecodedBufferArr;
  std::vector<cmplx> mNullSymbolBufferVec;
  FftHandler mFftHandler;

  void _resetBuffer();
  void _decode(const std::vector<cmplx> &, TBufferArr &) const;
  void _collapse(const TBufferArr & iVec, TCmplxTable & oEtsiVec, TCmplxTable & oNonEtsiVec) const;
};
