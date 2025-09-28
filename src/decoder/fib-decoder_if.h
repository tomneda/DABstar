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

  struct SUtcTimeSet
  {
    i32 ModJulianDate; // 17 bits used
    i16 Hour;
    i16 Minute;
    i16 Second;        // if -1, no seconds information is given
    i16 LTO_Minutes;   // Local Time Offset
  };

  virtual void connect_channel() = 0;
  virtual void disconnect_channel() = 0;
  virtual void get_data_for_audio_service(const QString &, SAudioData *) const = 0;
  virtual void get_data_for_packet_service(const QString &, std::vector<SPacketData> & oPDVec) const = 0;
  virtual std::vector<SServiceId> get_service_list() const = 0;

  virtual const QString & find_service(u32, i32) const = 0;
  virtual void get_parameters(const QString & iServiceName, u32 & oSId, i32 & oSCIdS) const = 0;
  virtual u8 get_ecc() const = 0;
  // virtual u16 get_country_name() const = 0;
  // virtual u8 get_countryId() const = 0;
  virtual i32 get_ensembleId() const = 0;
  virtual QString get_ensemble_name() const = 0;

  virtual void get_channel_info(SChannelData *, i32) const = 0;

  virtual i32 get_cif_count() const = 0;
  virtual void get_cif_count(i16 *, i16 *) const = 0;
  virtual u32 get_mod_julian_date() const = 0;
  virtual QStringList get_fib_content_str_list(i32 & oNumCols) const = 0;

protected:
  IFibDecoder() = default;
  
signals:
  void signal_name_of_ensemble(i32, const QString &, const QString &);
  void signal_change_in_configuration();
  void signal_start_announcement(const QString &, i32);
  void signal_stop_announcement(const QString &, i32);
  void signal_nr_services(i32);
  void signal_fib_time_info(const SUtcTimeSet & fibTimeInfo);
  void signal_fib_data_loaded();
};

class FibDecoderFactory
{
public:
  static std::unique_ptr<IFibDecoder> create(DabRadio * radio);
};

#endif // DABSTAR_IFC_FIB_DECODER_H