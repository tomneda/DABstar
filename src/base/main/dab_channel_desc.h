//
// Created by tomneda on 2026-04-05.
//
#pragma once

#include "glob_data_types.h"
#include "ensemble_list.h"
#include "dab_constants.h"
#include <QString>
#include <set>

struct SChannelDescriptor
{
public:
  SChannelDescriptor(const bool & ioIsFileMode)
    : usingFile(ioIsFileMode)
  {}

private:
  const bool & usingFile;
  QString absFilePath; // in case of a played file
  QString deviceName;  // in case of a device
  SIdentInfoEL identInfoEL{};

public:
  void set_device_name(const QString & iDeviceName) { deviceName = iDeviceName; }
  bool set_ident_info(const SIdentInfoEL & iIdentInfoEL) { const bool b = (iIdentInfoEL.fIdOrCh != identInfoEL.fIdOrCh); identInfoEL = iIdentInfoEL; return b; }
  [[nodiscard]] const SIdentInfoEL & get_ident_info() const { return identInfoEL; }
  [[nodiscard]] const QString & get_fId_or_ch() const { return identInfoEL.fIdOrCh; }
  [[nodiscard]] const QString get_fId_or_ch_descr() const { return (usingFile ? "FId_" :  "CH_") + identInfoEL.fIdOrCh; } // add description
  [[nodiscard]] const u32 get_sId_next() const { return identInfoEL.sIdToPlay; }
  [[nodiscard]] const QString & get_device_or_file_name() const { return (usingFile ? absFilePath : deviceName); }
  [[nodiscard]] QString get_type_info() const { return (usingFile ? "FId " : "channel ") + identInfoEL.fIdOrCh; }
  [[nodiscard]] bool check_ensemble_list_scan_level_should_be_achieved(const EScanLevel iScanLevelToAchieve) const
  {
    assert(identInfoEL.curScanLevel <= identInfoEL.scanLevelToAchieve);
    if (identInfoEL.reReadScanLevel || identInfoEL.curScanLevel < identInfoEL.scanLevelToAchieve)
    {
      return (identInfoEL.scanLevelToAchieve == iScanLevelToAchieve);
    }
    return false;
  }

  struct SDeferredData
  {
    std::optional<QString> countryName;
    std::optional<f32> snr;
    std::optional<f32> mer;
    std::optional<i32> nomFreqHz = -1;
    std::optional<i32> bbOffset = 0;

    void reset()
    {
      countryName.reset();
      snr.reset();
      mer.reset();
      nomFreqHz.reset();
      bbOffset.reset();
    }
  };

  // i32 nominalFreqHz = -1;
  QString ensembleName;
  QString ensembleNameShort;
  u8 mainId = 0;
  u8 subId = 0;
  u32 Eid = 0;
  bool tiiFile = false;
  bool itu_code_decoded = false;
  u8 ecc_byte = 0;
  // QString countryName;
  i32 nrTransmitters = 0;
  std::set<u16> transmitters;
  SDeferredData deferredData{};
  bool fibDataSentToEL = false;
  f32 snrLast = 0.0f;
  f32 merLast = 0.0f;

  union UTiiId
  {
    UTiiId(i32 mainId, i32 subId) : MainId(mainId & 0x7F), SubId(subId & 0xFF) {};
    explicit UTiiId(i32 fullId) : FullId(fullId) {};

    u16 FullId = 0;
    struct
    {
      u8 MainId;
      u8 SubId;
    };
  };

  void clean_channel()
  {
    ensembleName = "";
    nrTransmitters = 0;
    Eid = 0;
    itu_code_decoded = false;
    fibDataSentToEL = false;
    snrLast = 0.0f;
    merLast = 0.0f;
    transmitters.clear();
    deferredData.reset();
  }
};


