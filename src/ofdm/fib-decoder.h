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
  void get_data_for_audio_service(const QString &, Audiodata *);
  void get_data_for_packet_service(const QString &, Packetdata *, i16);
  int get_sub_channel_id(const QString &, u32);
  std::vector<SServiceId> get_services();

  QString find_service(u32, int);
  void get_parameters(const QString &, u32 *, int *);
  u8 get_ecc();
  u16 get_country_name();
  u8 get_countryId();
  i32 get_ensembleId();
  QString get_ensemble_name();
  void get_channel_info(ChannelData *, int);
  i32 get_cif_count();
  void get_cif_count(i16 *, i16 *);
  u32 get_julian_date();
  void set_epg_data(u32, i32, const QString &, const QString &);
  std::vector<SEpgElement> get_timeTable(u32);
  std::vector<SEpgElement> get_timeTable(const QString &);
  bool has_time_table(u32 SId);
  std::vector<SEpgElement> find_epg_data(u32);
  QStringList basic_print();
  int scan_width();

protected:
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

  int findService(const QString &);
  int find_service_index_from_SId(u32);
  void cleanupServiceList();
  void createService(QString name, u32 SId, int SCIds);
  int findServiceComponent(DabConfig *, i16);
  int findComponent(DabConfig * db, u32 SId, i16 subChId);
  int findServiceComponent(DabConfig *, u32, u8);
  void bind_audioService(DabConfig *, i8, u32, i16, i16, i16, i16);
  void bind_packetService(DabConfig *, i8, u32, i16, i16, i16, i16);

  QString announcements(u16);

  void setCluster(DabConfig *, int, i16, u16);
  Cluster * getCluster(DabConfig *, i16);

  QString serviceName(int index);
  QString serviceIdOf(int index);
  QString subChannelOf(int index);
  QString startAddressOf(int index);
  QString lengthOf(int index);
  QString protLevelOf(int index);
  QString codeRateOf(int index);
  QString bitRateOf(int index);
  QString dabType(int index);
  QString languageOf(int index);
  QString programTypeOf(int index);
  QString fmFreqOf(int index);
  QString appTypeOf(int index);
  QString FEC_scheme(int index);
  QString packetAddress(int index);
  QString DSCTy(int index);

  QString audioHeader();
  QString packetHeader();
  QString audioData(int index);
  QString packetData(int index);

signals:
  void signal_add_to_ensemble(const QString &, u32);
  void signal_name_of_ensemble(int, const QString &);
  void signal_clock_time(int, int, int, int, int, int, int, int, int);
  void signal_change_in_configuration();
  void signal_start_announcement(const QString &, int);
  void signal_stop_announcement(const QString &, int);
  void signal_nr_services(int);
};

#endif

