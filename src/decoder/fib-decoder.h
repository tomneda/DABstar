/*
 * Copyright (c) 2025 by Thomas Neder (https://github.com/tomneda)
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef  FIB_DECODER_H
#define  FIB_DECODER_H

#include "fib-decoder_if.h"
#include "fib-config-fig0.h"
#include "fib-config-fig1.h"
#include "fib-helper.h"
#include <QMutex>
#include <set>
#include <chrono>

class DabRadio;
class QTimer;

class FibDecoder final : public IFibDecoder, public FibHelper
{
public:
  explicit FibDecoder(DabRadio *);
  ~FibDecoder() override = default;

  void process_FIB(const std::array<std::byte, cFibSizeVitOut> &, u16) override;

  void connect_channel() override;
  void disconnect_channel() override;

  void set_SId_for_fast_audio_selection(u32 iSId) override;

  void get_data_for_audio_service(u32 iSId, SAudioData & oAD) const override;
  void get_data_for_audio_service_addon(u32 iSId, SAudioDataAddOns & oADAO) const override;
  void get_data_for_packet_service(u32 iSId, std::vector<SPacketData> & oPDVec) const override;
  std::vector<SServiceId> get_service_list() const override;

  const QString & get_service_label_from_SId_SCIdS(u32, i32) const override;
  void get_SId_SCIdS_from_service_label(const QString & iServiceLabel, u32 & oSId, i32 & oSCIdS) const override;
  u8 get_ecc() const override;
  i32 get_ensembleId() const override;
  QString get_ensemble_name() const override;
  std::vector<i8> get_sub_channel_id_list() const override;
  void get_sub_channel_info(SChannelData *, i32) const override;
  i32 get_cif_count() const override;
  void get_cif_count(i16 *, i16 *) const override;
  u32 get_mod_julian_date() const override;
  QStringList get_fib_content_str_list(i32 & oNumCols) const override;

  // void set_epg_data(u32, i32, const QString &, const QString &) const override;
  // std::vector<SEpgElement> get_timeTable(u32) const override;
  // std::vector<SEpgElement> get_timeTable(const QString &) const override;
  // bool has_time_table(u32 SId) const override;
  // std::vector<SEpgElement> find_epg_data(u32) const override;

private:
  DabRadio * const mpRadioInterface = nullptr;
  std::unique_ptr<FibConfigFig1> mpFibConfigFig1;
  std::unique_ptr<FibConfigFig0> mpFibConfigFig0Curr;
  std::unique_ptr<FibConfigFig0> mpFibConfigFig0Next;
  i32 mCifCount = 0;
  i16 mCifCount_hi = 0;
  i16 mCifCount_lo = 0;
  i32 mModJulianDate = 0;
  u32 mSIdForFastAudioSelection = 0;
  mutable QMutex mMutex;
  u8 mPrevChangeFlag = 0;
  QTimer * mpTimerDataConsistencyCheck = nullptr;
  QTimer * mpTimerCheckStateAndPrintFigs = nullptr;
  EFibLoadingState mFibLoadingState = EFibLoadingState::S0_Init;
  SUtcTimeSet mUtcTimeSet{};
  std::set<u8> mUnhandledFig0Set;
  std::set<u8> mUnhandledFig1Set;
  std::chrono::milliseconds mDiffTimeMax{};
  std::chrono::time_point<std::chrono::system_clock> mLastTimePoint{};
  FibHelper::TTP mFirstFigTimePoint{};

  struct SFigHeader // plus flags
  {
    u8 Length;   // Length of the FIG in bytes
    u8 CN_Flag;  // Current or Next config
    u8 OE_Flag;  // Other Ensemble
    u8 PD_Flag;  // Program or Data service flag
  };

  void _reset();

  FibConfigFig0 * _get_config_ptr(const u8 iCN_Bit) const { return iCN_Bit == 0 ? mpFibConfigFig0Curr.get() : mpFibConfigFig0Next.get(); }
  SFigHeader _get_fig_header(const u8 *) const;

  void _process_Fig0(const u8 *);
  void _process_Fig1(const u8 *);

  using TFnFibProc = i16 (FibDecoder::*)(const u8 *, i16, const SFigHeader &);
  void _process_fig0_loop(const u8 *, TFnFibProc);

  void _process_Fig0s0(const u8 *);
  i16 _subprocess_Fig0s1(const u8 *, i16, const SFigHeader &);
  i16 _subprocess_Fig0s2(const u8 *, i16, const SFigHeader &);
  i16 _subprocess_Fig0s3(const u8 *, i16, const SFigHeader &);
  i16 _subprocess_Fig0s5(const u8 *, i16, const SFigHeader &);
  void _process_Fig0s7(const u8 *);
  i16 _subprocess_Fig0s8(const u8 *, i16, const SFigHeader &);
  void _process_Fig0s9(const u8 *);
  void _process_Fig0s10(const u8 *);
  i16 _subprocess_Fig0s13(const u8 *, i16, const SFigHeader &);
  i16 _subprocess_Fig0s14(const u8 *, i16, const SFigHeader &);
  void _process_Fig0s17(const u8 *);
  void _process_Fig0s18(const u8 *);
  void _process_Fig0s19(const u8 *);
  i16 _subprocess_Fig0s21(const u8 *, i16, const SFigHeader &) const;

  void _process_Fig1s0(const u8 *);
  void _process_Fig1s1(const u8 *);
  void _process_Fig1s4(const u8 *);
  void _process_Fig1s5(const u8 *);
  // void _process_Fig1s6(const u8 *) const;

  void _set_cluster(FibConfigFig0 *, i32, u32 iSId, u16);
  Cluster * _get_cluster(FibConfigFig0 *, i16) const;

  bool _get_data_for_audio_service(const FibConfigFig0::SFig0s2_BasicService_ServiceCompDef & iFig0s2, SAudioData * opAD) const;
  bool _get_data_for_audio_service_addon(const FibConfigFig0::SFig0s2_BasicService_ServiceCompDef & iFig0s2, SAudioDataAddOns * opADAO) const;
  bool _get_data_for_packet_service(const FibConfigFig0::SFig0s2_BasicService_ServiceCompDef & iFig0s2, i16 iCompIdx, SPacketData * opPD) const;

  QString _get_audio_data_str(const FibConfigFig0::SFig0s2_BasicService_ServiceCompDef & iFig0s2) const;
  QString _get_packet_data_str(const FibConfigFig0::SFig0s2_BasicService_ServiceCompDef & iFig0s2) const;

  bool _extract_character_set_label(FibConfigFig1::SFig1_DataField & oFig1DF, const u8 * d, i16 iLabelOffs) const;
  void _retrigger_timer_data_loaded_fast(const char * iCallerName);
  void _retrigger_timer_data_loaded_slow(const char * iCallerName);
  void _process_fast_audio_selection();
  bool _check_audio_data_completeness() const; // FIG 0/1, FIG 0/2 and FIG 1/1 are loaded
  bool _check_packet_data_completeness() const;

  template<typename T> inline QString hex_str(const T iVal) const { return QString("0x%1").arg(iVal, 0, 16); }

private slots:
  void _slot_timer_data_consistency_check();
  void _slot_timer_check_state_and_print_FIGs();
};

#endif
