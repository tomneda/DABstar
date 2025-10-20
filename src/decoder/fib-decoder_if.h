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

  /* tomneda: Timing Analysis of DAB FIG Data Reception
   *
   * Based on extensive measurements of FIG data reception across 40+ ensembles (both local and worldwide via files),
   * I observed that actual reception times significantly exceed the times suggested in the DAB ETSI standard.
   * To optimize the FIB decoder's user-layer signaling, I've organized the FIG data into four priority states:
   *
   * State 1: Fast Audio Startup (22-191ms)
   * - Contains essential data from FIG 0/1 and 0/2 needed to start audio playback
   * - Triggered when DABstar starts or ensemble changes (frequency/file), set_SId_for_fast_audio_selection() must be called before
   *
   * State 2: Fully basic data FIG 0/1 and FIG 0/2 data loaded, but not e.g. service labels yet (22-191ms)
   * - This state is not sent to the user-layer, as it still waited for service label
   *
   * State 3: FIG 1/1 Service labels for audio received (to fast update the service list) (up to 2398ms)
   *
   * State 4: Service Information (286-2398ms)
   * - Includes data packet service information (EPG/SPI, Journaline, etc.)
   *
   * State 5: Supplementary Data (~10+ seconds)
   * - Contains lower-priority information like FIG 0/17 (programme type)
   * - Sent as separate (and so possible repeated) callbacks to the user-layer due to long reception times
   *
   * Timing Implementation Notes:
   * - Times reflect both collection of different data within FIG-types and repetition intervals
   * - Repetition intervals typically exceed collection times, requiring timer restarts on new packets
   * - While the timing could be optimized per FIG-type, the timing analysis for that outweighs the benefits
   *
   * Group 2 callbacks use a timer-based approach as there's no reliable way to detect complete data collection.
   * While FIG 0/7 could help with this, its low transmission rate (<15% of ensembles) makes it impractical.
   */
  enum class EFibLoadingState : i32
  {
    S0_Init,
    S1_FastAudioDataLoaded,    // special SId in FIG 0/1 and 0/2 loaded -> start fast audio selection (needs valid set_SId_for_fast_audio_selection() setting first)
    S2_PrimaryBaseDataLoaded,  // FIG 0/1 and 0/2 are fully loaded -> not callback-ed
    S3_FullyAudioDataLoaded,   // FIG 0/1 and 0/2 and FIG 1/1 fully loaded for audio service label -> service list can be updated (for scan)
    S4_FullyPacketDataLoaded,  // major FIGs are loaded -> start primary and/or secondary packet services
    S5_DeferredDataLoaded      // (hopefully only) minor important FIGs are loaded, like some FIG 0/17 for displaying the programme type
  };

  struct SUtcTimeSet
  {
    i32 ModJulianDate; // 17 bits used
    i16 Hour;
    i16 Minute;
    i16 Second;        // if -1, no seconds information is given
    i16 LTO_Minutes;   // Local Time Offset
  };

  ~IFibDecoder() override = default;

  virtual void process_FIB(const std::array<std::byte, cFibSizeVitOut> &, u16) = 0;

  virtual void connect_channel() = 0;
  virtual void disconnect_channel() = 0;

  virtual void set_SId_for_fast_audio_selection(u32 iSId) = 0;

  virtual void get_data_for_audio_service(u32 iSId, SAudioData & oAD) const = 0;
  virtual void get_data_for_audio_service_addon(u32 iSId, SAudioDataAddOns & oADAO) const = 0;
  virtual void get_data_for_packet_service(u32 iSId, std::vector<SPacketData> & oPDVec) const = 0;
  virtual std::vector<SServiceId> get_service_list() const = 0;

  virtual const QString & get_service_label_from_SId_SCIdS(u32 iSId, i32 iSCIdS) const = 0;
  virtual void get_SId_SCIdS_from_service_label(const QString & iServiceLabel, u32 & oSId, i32 & oSCIdS) const = 0;
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
  void signal_fib_time_info(const SUtcTimeSet & fibTimeInfo);
  void signal_fib_loaded_state(EFibLoadingState);
};

class FibDecoderFactory
{
public:
  static std::unique_ptr<IFibDecoder> create(DabRadio * radio);
};

#endif // DABSTAR_IFC_FIB_DECODER_H