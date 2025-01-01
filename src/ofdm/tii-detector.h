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
  float phaseDeg;
  bool isNonEtsiPhase;
};

class TiiDetector
{
public:
  explicit TiiDetector(uint8_t iDabMode);
  ~TiiDetector() = default;

  void reset();
  void set_detect_collisions(bool);
  void set_subid_for_collision_search(uint8_t);
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

  const DabParams mParams;
  const int16_t mT_u;
  const int16_t mT_g;
  const int16_t mK;
  bool mShowTiiCollisions = false;
  bool mCarrierDelete = true;
  uint8_t mSubIdCollSearch = 0;
  TBufferArr768 mDecodedBufferArr;
  std::vector<cmplx> mNullSymbolBufferVec;
  FftHandler mFftHandler;

  float _calculate_average_noise(const TFloatTable192 & iFloatTable) const;
  void _get_float_table_and_max_abs_value(TFloatTable192 & oFloatTable, float & ioMax, const TCmplxTable192 & iCmplxTable) const;
  void _compare_etsi_and_non_etsi(bool & oIsNonEtsiPhase, int & oCount, cmplx & oSum, std::byte & oPattern,
                               int iSubId, const float iThresholdLevel,
                               const TFloatTable192 & iEtsiFloatTable, const TFloatTable192 & iNonEtsiFloatTable,
                               const TCmplxTable192 & iEtsiCmplxTable, const TCmplxTable192 & iNonEtsiCmplxTable) const;
  void _find_collisions(std::vector<STiiResult> & ioResultVec, int iMainId, int iSubId,
                        std::byte iPattern, float iMax, float iThresholdLevel, int iCount, bool iIsNonEtsi,
                        const TCmplxTable192 & iCmplxTable, const TFloatTable192 & iFloatTable) const;
  int _find_exact_main_id_match(std::byte iPattern) const;
  int _find_best_main_id_match(cmplx & oSum, int iSubId, const TCmplxTable192 & ipCmplxTable) const;
  void _reset_null_symbol_buffer();
  void _remove_single_carrier_values(TBufferArr768 & ioBuffer) const;
  void _decode_and_accumulate_carrier_pairs(TBufferArr768 & ioVec, const std::vector<cmplx> & iVec) const;
  void _collapse_tii_groups(TCmplxTable192 & ioEtsiVec, TCmplxTable192 & ioNonEtsiVec, const TBufferArr768 & iVec) const;
};
