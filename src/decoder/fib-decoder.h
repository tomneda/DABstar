/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013 .. 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-TAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef  FIB_DECODER_H
#define  FIB_DECODER_H

#include  "fib-decoder_if.h"
#include  "fib-config.h"
#include  "msc-handler.h"
#include  <QObject>
#include  <QMutex>
#include  <QScopedPointer>

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
  bool sync_reached() const override;
  void get_data_for_audio_service(const QString &, AudioData *) const override;
  void get_data_for_packet_service(const QString &, std::vector<PacketData> & oPDVec) const override;
  i32 get_sub_channel_id(const QString &, u32) const override;
  std::vector<SServiceId> get_services() const override;

  QString find_service(u32, i32) const override;
  void get_parameters(const QString & iServiceName, u32 & oSId, i32 & oSCIdS) const override;
  u8 get_ecc() const override;
  u16 get_country_name() const override;
  u8 get_countryId() const override;
  i32 get_ensembleId() const override;
  QString get_ensemble_name() const override;
  void get_channel_info(ChannelData *, i32) const override;
  i32 get_cif_count() const override;
  void get_cif_count(i16 *, i16 *) const override;
  u32 get_julian_date() const override;
  void set_epg_data(u32, i32, const QString &, const QString &) const override;
  std::vector<SEpgElement> get_timeTable(u32) const override;
  std::vector<SEpgElement> get_timeTable(const QString &) const override;
  bool has_time_table(u32 SId) const override;
  std::vector<SEpgElement> find_epg_data(u32) const override;
  QStringList basic_print() const override;
  i32 get_scan_width() const override;

private:
  DabRadio * const myRadioInterface = nullptr;
  std::unique_ptr<EnsembleDescriptor> ensemble;
  std::unique_ptr<FibConfig> currentConfig;
  std::unique_ptr<FibConfig> nextConfig;
  std::array<i32, 8> dateTime{};
  i32 CIFcount = 0;
  i16 CIFcount_hi = 0;
  i16 CIFcount_lo = 0;
  u32 mjd = 0;      // julianDate
  mutable QMutex mMutex;
  u8 prevChangeFlag = 0;
  QTimer * mpTimerDataLoaded = nullptr;

  struct SFigHeader
  {
    u8 Length;   // Length of the FIG in bytes
    u8 Type;     // Type of the FIG
    u8 CC_Flag;  // Current or Next config
    u8 CN_Flag;  // Current or Next config
    u8 OE_Flag;  // Other Ensemble
    u8 PD_Flag;  // Program or Data service flag
  };

  FibConfig * get_config_ptr(const u8 iCN_Bit) const { return iCN_Bit == 0 ? currentConfig.get() : nextConfig.get(); }

  SFigHeader get_fig_header(const u8 *);

  void process_FIG0(const u8 *);
  void process_FIG1(const u8 *);
  void FIG0Extension0(const u8 *);
  void FIG0Extension1(const u8 *);
  void FIG0Extension2(const u8 *);
  void FIG0Extension3(const u8 *);
  void FIG0Extension5(const u8 *);
  void FIG0Extension7(const u8 *);
  void FIG0Extension8(const u8 *);
  void FIG0Extension9(const u8 *);
  void FIG0Extension10(const u8 *);
  void FIG0Extension13(const u8 *);
  void FIG0Extension14(const u8 *);
  void FIG0Extension17(const u8 *);
  void FIG0Extension18(const u8 *);
  void FIG0Extension19(const u8 *);
  void FIG0Extension21(const u8 *);

  i16 HandleFIG0Extension1(const u8 *, i16, const SFigHeader &);
  i16 HandleFIG0Extension2(const u8 *, i16, const SFigHeader &);
  i16 HandleFIG0Extension3(const u8 *, i16, const SFigHeader &);
  i16 HandleFIG0Extension5(const u8 *, i16, const SFigHeader &);
  i16 HandleFIG0Extension8(const u8 *, i16, const SFigHeader &);
  i16 HandleFIG0Extension13(const u8 *, i16, const SFigHeader &);
  i16 HandleFIG0Extension21(const u8 *, i16, const SFigHeader &);

  void FIG1Extension0(const u8 *);
  void FIG1Extension1(const u8 *);
  void FIG1Extension4(const u8 *);
  void FIG1Extension5(const u8 *);
  void FIG1Extension6(const u8 *);

  void cleanup_service_list();
  void create_service(QString iServiceName, u32 iSId, i32 iSCIdS);

  void slot_timer_data_loaded();
  std::vector<SServiceId> insert_sorted(const std::vector<SServiceId> & iServiceIdList, const SServiceId & iServiceId) const;
  EnsembleDescriptor::TMapService::iterator find_service_from_service_name(const QString &) const;
  FibConfig::TSPServiceCompDesc find_service_component(FibConfig *, i16) const;
  FibConfig::TSPServiceCompDesc find_component(FibConfig * ipDabConfig, u32 iSId, i16 iSubChId) const;
  FibConfig::TSPServiceCompDesc find_service_component(FibConfig *, u32, u8) const;

  void bind_audio_service(FibConfig *, ETMId, u32, i16, i16, i16, i16);
  void bind_packet_service(FibConfig *, ETMId, u32, i16, i16, i16, i16);

  QString get_announcement_type_str(u16);

  void set_cluster(FibConfig *, i32, u32 iSId, u16);
  Cluster * get_cluster(FibConfig *, i16);

  QString get_service_name(const ServiceComponentDescriptor & iScd) const;
  QString get_service_id_of(const ServiceComponentDescriptor & iScd) const;
  QString get_sub_channel_of(const ServiceComponentDescriptor & iScd) const;
  QString get_start_address_of(const ServiceComponentDescriptor & iScd) const;
  QString get_length_of(const ServiceComponentDescriptor & iScd) const;
  QString get_prot_level_of(const ServiceComponentDescriptor & iScd) const;
  QString get_code_rate_of(const ServiceComponentDescriptor & iScd) const;
  QString get_bit_rate_of(const ServiceComponentDescriptor & iScd) const;
  QString get_dab_type(const ServiceComponentDescriptor & iScd) const;
  QString get_language_of(const ServiceComponentDescriptor & iScd) const;
  QString get_program_type_of(const ServiceComponentDescriptor & iScd) const;
  QString get_fm_freq_of(const ServiceComponentDescriptor & iScd) const;
  QString get_app_type_of(const ServiceComponentDescriptor & iScd) const;
  QString get_FEC_scheme(const ServiceComponentDescriptor & iScd) const;
  QString get_packet_address(const ServiceComponentDescriptor & iScd) const;
  QString get_DSCTy(const ServiceComponentDescriptor & iScd) const;
  QString get_audio_header() const;
  QString get_packet_header() const;
  QString get_audio_data(const ServiceComponentDescriptor & iScd) const;
  QString get_packet_data(const ServiceComponentDescriptor & iScd) const;

  bool extract_character_set_label(QString & oName, const u8 * d, i16 iLabelOffs);
  void retrigger_timer_data_loaded();
};

#endif

