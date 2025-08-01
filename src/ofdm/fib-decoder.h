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

//
#include  <cstdint>
#include  <cstdio>
#include  <QObject>
#include  <QByteArray>
#include  "msc-handler.h"
#include  <QMutex>
#include  "dab-config.h"

class DabRadio;
class EnsembleDescriptor;
class DabConfig;
class Cluster;

class FibDecoder : public QObject
{
Q_OBJECT
public:
  explicit FibDecoder(DabRadio *);
  ~FibDecoder();

  void clearEnsemble();
  void connect_channel();
  void disconnect_channel();
  bool sync_reached();
  void get_data_for_audio_service(const QString &, AudioData *);
  void get_data_for_packet_service(const QString &, PacketData *, i16);
  i32 get_sub_channel_id(const QString &, u32);
  std::vector<SServiceId> get_services();

  QString find_service(u32, i32);
  void get_parameters(const QString &, u32 *, i32 *);
  u8 get_ecc();
  u16 get_country_name();
  u8 get_countryId();
  i32 get_ensembleId();
  QString get_ensemble_name();
  void get_channel_info(ChannelData *, i32);
  i32 get_cif_count();
  void get_cif_count(i16 *, i16 *);
  u32 get_julian_date();
  void set_epg_data(u32, i32, const QString &, const QString &);
  std::vector<SEpgElement> get_timeTable(u32);
  std::vector<SEpgElement> get_timeTable(const QString &);
  bool has_time_table(u32 SId);
  std::vector<SEpgElement> find_epg_data(u32);
  QStringList basic_print();
  i32 get_scan_width();
  void process_FIB(const u8 *, u16);

private:
  std::vector<SServiceId> insert_sorted(const std::vector<SServiceId> & l, SServiceId n);
  DabRadio * myRadioInterface = nullptr;
  DabConfig * currentConfig = nullptr;
  DabConfig * nextConfig = nullptr;
  EnsembleDescriptor * ensemble = nullptr;
  std::array<i32, 8> dateTime{};
  QMutex fibLocker;
  i32 CIFcount = 0;
  i16 CIFcount_hi = 0;
  i16 CIFcount_lo = 0;
  u32 mjd = 0;      // julianDate

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

  i16 HandleFIG0Extension1(const u8 *, i16, u8, u8, u8);
  i16 HandleFIG0Extension2(const u8 *, i16, u8, u8, u8);
  i16 HandleFIG0Extension3(const u8 *, i16, u8, u8, u8);
  i16 HandleFIG0Extension5(const u8 *, u8, u8, u8, i16);
  i16 HandleFIG0Extension8(const u8 *, i16, u8, u8, u8);
  i16 HandleFIG0Extension13(const u8 *, i16, u8, u8, u8);
  i16 HandleFIG0Extension21(const u8 *, u8, u8, u8, i16);

  void FIG1Extension0(const u8 *);
  void FIG1Extension1(const u8 *);
  void FIG1Extension4(const u8 *);
  void FIG1Extension5(const u8 *);
  void FIG1Extension6(const u8 *);

  i32 find_service(const QString &);
  i32 find_service_index_from_SId(u32);
  void cleanup_service_list();
  void create_service(QString name, u32 SId, i32 SCIds);
  i32 find_service_component(DabConfig *, i16);
  i32 find_component(DabConfig * db, u32 SId, i16 subChId);
  i32 find_service_component(DabConfig *, u32, u8);
  void bind_audio_service(DabConfig *, i8, u32, i16, i16, i16, i16);
  void bind_packet_service(DabConfig *, i8, u32, i16, i16, i16, i16);

  QString get_announcement_type_str(u16);

  void set_cluster(DabConfig *, i32, i16, u16);
  Cluster * get_cluster(DabConfig *, i16);

  QString get_service_name(i32 index);
  QString get_service_id_of(i32 index);
  QString get_sub_channel_of(i32 index);
  QString get_start_address_of(i32 index);
  QString get_length_of(i32 index);
  QString get_prot_level_of(i32 index);
  QString get_code_rate_of(i32 index);
  QString get_bit_rate_of(i32 index);
  QString get_dab_type(i32 index);
  QString get_language_of(i32 index);
  QString get_program_type_of(i32 index);
  QString get_fm_freq_of(i32 index);
  QString get_app_type_of(i32 index);
  QString get_FEC_scheme(i32 index);
  QString get_packet_address(i32 index);
  QString get_DSCTy(i32 index);

  QString get_audio_header();
  QString get_packet_header();
  QString get_audio_data(i32 index);
  QString get_packet_data(i32 index);

signals:
  void signal_add_to_ensemble(const QString &, u32);
  void signal_name_of_ensemble(i32, const QString &);
  void signal_clock_time(i32, i32, i32, i32, i32, i32, i32, i32, i32);
  void signal_change_in_configuration();
  void signal_start_announcement(const QString &, i32);
  void signal_stop_announcement(const QString &, i32);
  void signal_nr_services(i32);
};

#endif

