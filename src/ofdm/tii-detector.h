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
  static constexpr int32_t cNumBlocks4 = 4;
  static constexpr int32_t cNumGroups8 =  8;
  static constexpr int32_t cGroupSize24 = 24;
  static constexpr int32_t cBlockSize192 = cNumGroups8 * cGroupSize24; // == 192

  using TBufferArr768  = std::array<cmplx, 768>;
  using TFloatTable192 = std::array<float, cBlockSize192>;
  using TCmplxTable192 = std::array<cmplx, cBlockSize192>;

  // static const std::array<const uint8_t, cNumGroups8> cBits;
  const DabParams mParams;
  const int16_t mT_u;
  const int16_t mT_g;
  const int16_t mK;
  bool mCollisions = false;
  bool mCarrierDelete = true;
  uint8_t mSelectedSubId = 0;
  TBufferArr768 mDecodedBufferArr;
  std::vector<cmplx> mNullSymbolBufferVec;
  FftHandler mFftHandler;

  void _resetBuffer();
  void _decode(const std::vector<cmplx> &, TBufferArr768 &) const;
  void _collapse(const TBufferArr768 & iVec, TCmplxTable192 & ioEtsiVec, TCmplxTable192 & ioNonEtsiVec) const;
};
