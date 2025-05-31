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

struct STiiResult
{
  u8 mainId;
  u8 subId;
  f32 strength;
  f32 phaseDeg;
  bool isNonEtsiPhase;
};

class TiiDetector
{
public:
  explicit TiiDetector();
  ~TiiDetector();

  void reset();
  void set_detect_collisions(bool);
  void set_subid_for_collision_search(u8);
  void add_to_tii_buffer(const TArrayTu & iV);
  std::vector<STiiResult> process_tii_data(i16);

private:
  static constexpr i32 cNumBlocks4 = 4;
  static constexpr i32 cNumGroups8 =  8;
  static constexpr i32 cGroupSize24 = 24;
  static constexpr i32 cBlockSize192 = cNumGroups8 * cGroupSize24; // == 192

  using TBufferArr768  = std::array<cf32, 768>;
  using TFloatTable192 = std::array<f32, cBlockSize192>;
  using TCmplxTable192 = std::array<cf32, cBlockSize192>;

  bool mShowTiiCollisions = false;
  bool mCarrierDelete = true;
  u8 mSubIdCollSearch = 0;
  TBufferArr768 mDecodedBufferArr;
  TArrayTu mNullSymbolBufferVec;

  f32 _calculate_average_noise(const TFloatTable192 & iFloatTable) const;
  void _get_float_table_and_max_abs_value(TFloatTable192 & oFloatTable, f32 & ioMax, const TCmplxTable192 & iCmplxTable) const;
  void _compare_etsi_and_non_etsi(bool & oIsNonEtsiPhase, i32 & oCount, cf32 & oSum, std::byte & oPattern,
                               i32 iSubId, const f32 iThresholdLevel,
                               const TFloatTable192 & iEtsiFloatTable, const TFloatTable192 & iNonEtsiFloatTable,
                               const TCmplxTable192 & iEtsiCmplxTable, const TCmplxTable192 & iNonEtsiCmplxTable) const;
  void _find_collisions(std::vector<STiiResult> & ioResultVec, i32 iMainId, i32 iSubId,
                        std::byte iPattern, f32 iMax, f32 iThresholdLevel, i32 iCount, bool iIsNonEtsi,
                        const TCmplxTable192 & iCmplxTable, const TFloatTable192 & iFloatTable) const;
  i32 _find_exact_main_id_match(std::byte iPattern) const;
  i32 _find_best_main_id_match(cf32 & oSum, i32 iSubId, const TCmplxTable192 & ipCmplxTable) const;
  void _reset_null_symbol_buffer();
  void _remove_single_carrier_values(TBufferArr768 & ioBuffer) const;
  void _decode_and_accumulate_carrier_pairs(TBufferArr768 & ioVec, const TArrayTu & iVec) const;
  void _collapse_tii_groups(TCmplxTable192 & ioEtsiVec, TCmplxTable192 & ioNonEtsiVec, const TBufferArr768 & iVec) const;
  cf32 _turn_phase(cf32 const value, const u8 phase) const;
};
