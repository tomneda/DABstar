//
// Created by tomneda on 2025-08-30.
//

#ifndef DABSTAR_IFC_FIB_DECODER_H
#define DABSTAR_IFC_FIB_DECODER_H

#include "glob_defs.h"
#include "dab-constants.h"
#include <array>
#include <vector>
#include <QObject>
#include <QString>
#include <QStringList>

class DabRadio;

class IFibDecoder : public QObject
{
Q_OBJECT
public:
  virtual ~IFibDecoder() = default;

  virtual void process_FIB(const std::array<std::byte, cFibSizeVitOut> &, u16) = 0;

  virtual void connect_channel() = 0;
  virtual void disconnect_channel() = 0;
  virtual bool sync_reached() const = 0;
  virtual void get_data_for_audio_service(const QString &, AudioData *) const = 0;
  virtual void get_data_for_packet_service(const QString &, std::vector<PacketData> & oPDVec) const = 0;
  virtual i32 get_sub_channel_id(const QString &, u32) const = 0;
  virtual std::vector<SServiceId> get_services() const = 0;

  virtual QString find_service(u32, i32) const = 0;
  virtual void get_parameters(const QString & iServiceName, u32 & oSId, i32 & oSCIdS) const = 0;
  virtual u8 get_ecc() const = 0;
  virtual u16 get_country_name() const = 0;
  virtual u8 get_countryId() const = 0;
  virtual i32 get_ensembleId() const = 0;
  virtual QString get_ensemble_name() const = 0;

  virtual void get_channel_info(ChannelData *, i32) const = 0;

  virtual i32 get_cif_count() const = 0;
  virtual void get_cif_count(i16 *, i16 *) const = 0;

  virtual u32 get_julian_date() const = 0;

  virtual void set_epg_data(u32, i32, const QString &, const QString &) const = 0;
  virtual std::vector<SEpgElement> get_timeTable(u32) const = 0;
  virtual std::vector<SEpgElement> get_timeTable(const QString &) const = 0;
  virtual bool has_time_table(u32 SId) const = 0;
  virtual std::vector<SEpgElement> find_epg_data(u32) const = 0;

  virtual QStringList basic_print() const = 0;
  virtual i32 get_scan_width() const = 0;

protected:
  IFibDecoder() = default;
  
signals:
  void signal_add_to_ensemble(const QString &, u32);
  void signal_name_of_ensemble(i32, const QString &);
  void signal_clock_time(i32, i32, i32, i32, i32, i32, i32, i32, i32);
  void signal_change_in_configuration();
  void signal_start_announcement(const QString &, i32);
  void signal_stop_announcement(const QString &, i32);
  void signal_nr_services(i32);
};

class FibDecoderFactory
{
public:
  static std::unique_ptr<IFibDecoder> create(DabRadio * radio);
};

#endif // DABSTAR_IFC_FIB_DECODER_H