//
// Created by tomneda on 2025-08-23.
//
#ifndef  FIB_DECODER_H
#define  FIB_DECODER_H

#include "fib-decoder_if.h"
#include "fib-config-fig0.h"
#include "fib-config-fig1.h"
#include <QMutex>
#include <set>
#include <chrono>

class DabRadio;
class QTimer;

class FibDecoder final : public IFibDecoder
{
public:
  explicit FibDecoder(DabRadio *);
  ~FibDecoder() override = default;

  void process_FIB(const std::array<std::byte, cFibSizeVitOut> &, u16) override;

  void connect_channel() override;
  void disconnect_channel() override;

  void get_data_for_audio_service(const QString &, SAudioData *) const override;
  void get_data_for_packet_service(const QString &, std::vector<SPacketData> & oPDVec) const override;
  std::vector<SServiceId> get_service_list() const override;

  const QString & find_service(u32, i32) const override;
  void get_parameters(const QString & iServiceLabel, u32 & oSId, i32 & oSCIdS) const override;
  u8 get_ecc() const override;
  i32 get_ensembleId() const override;
  QString get_ensemble_name() const override;
  void get_channel_info(SChannelData *, i32) const override;
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
  static constexpr i32 cMaxFibLoadingTime_ms = 3000;
  DabRadio * const mpRadioInterface = nullptr;
  std::unique_ptr<FibConfigFig1> mpFibConfigFig1;
  std::unique_ptr<FibConfigFig0> mpFibConfigFig0Curr;
  std::unique_ptr<FibConfigFig0> mpFibConfigFig0Next;
  i32 mCifCount = 0;
  i16 mCifCount_hi = 0;
  i16 mCifCount_lo = 0;
  i32 mModJulianDate = 0;
  mutable QMutex mMutex;
  u8 mPrevChangeFlag = 0;
  QTimer * mpTimerDataLoaded = nullptr;
  bool mFibDataLoaded = false;
  SUtcTimeSet mUtcTimeSet{};
  std::set<u8> mUnhandledFig0Set;
  std::set<u8> mUnhandledFig1Set;
  std::chrono::milliseconds mDiffMax{};
  std::chrono::time_point<std::chrono::system_clock> mLastTimePoint{};

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

  void _process_Fig0s0(const u8 *);
  void _process_Fig0s1(const u8 *);
  void _process_Fig0s2(const u8 *);
  void _process_Fig0s3(const u8 *);
  void _process_Fig0s5(const u8 *);
  void _process_Fig0s7(const u8 *);
  void _process_Fig0s8(const u8 *);
  void _process_Fig0s9(const u8 *);
  void _process_Fig0s10(const u8 *);
  void _process_Fig0s13(const u8 *);
  void _process_Fig0s14(const u8 *);
  void _process_Fig0s17(const u8 *);
  void _process_Fig0s18(const u8 *);
  void _process_Fig0s19(const u8 *);
  void _process_Fig0s21(const u8 *) const;

  i16 _subprocess_Fig0s1(const u8 *, i16, const SFigHeader &);
  i16 _subprocess_Fig0s2(const u8 *, i16, const SFigHeader &);
  i16 _subprocess_Fig0s3(const u8 *, i16, const SFigHeader &);
  i16 _subprocess_Fig0s5(const u8 *, i16);
  i16 _subprocess_Fig0s8(const u8 *, i16, const SFigHeader &);
  i16 _subprocess_Fig0s13(const u8 *, i16, const SFigHeader &);
  i16 _subprocess_Fig0s21(const u8 *, i16, const SFigHeader &) const;

  void _process_Fig1s0(const u8 *);
  void _process_Fig1s1(const u8 *);
  void _process_Fig1s4(const u8 *);
  void _process_Fig1s5(const u8 *);
  // void _process_Fig1s6(const u8 *) const;

  bool _are_fib_data_loaded() const;

  void _set_cluster(FibConfigFig0 *, i32, u32 iSId, u16);
  Cluster * _get_cluster(FibConfigFig0 *, i16);

  bool _get_data_for_audio_service(const FibConfigFig0::SFig0s2_BasicService_ServiceComponentDefinition & iFig0s2, SAudioData * opAD) const;
  bool _get_data_for_packet_service(const FibConfigFig0::SFig0s2_BasicService_ServiceComponentDefinition & iFig0s2, i16 iCompIdx, SPacketData * opPD) const;

  QString _get_audio_data_str(const FibConfigFig0::SFig0s2_BasicService_ServiceComponentDefinition & iFig0s2) const;
  QString _get_packet_data_str(const FibConfigFig0::SFig0s2_BasicService_ServiceComponentDefinition & iFig0s2) const;

  bool _extract_character_set_label(FibConfigFig1::SFig1_DataField & oFig1DF, const u8 * d, i16 iLabelOffs) const;
  void _retrigger_timer_data_loaded(const char * iCallerName);

  template<typename T> inline QString hex_str(const T iVal) const { return QString("0x%1").arg(iVal, 0, 16); }

private slots:
  void _slot_timer_data_loaded();
};

#endif

