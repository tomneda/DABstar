//
// Created by tomneda on 2025-09-15.
//
#ifndef  FIB_CONFIG_FIG1_H
#define  FIB_CONFIG_FIG1_H

#include "glob_data_types.h"
#include "fib-helper.h"
#include <vector>
#include <map>
#include <QString>

class FibConfigFig1 : public FibHelper
{
public:
  FibConfigFig1();
  ~FibConfigFig1() = default;

  void reset();

  struct SFig1_DataField
  {
    i8  Charset = -1; // this 4-bit field shall identify a character set to qualify the character information contained in the FIG type 1 field. The interpretation of this field shall be as defined in ETSI TS 101 756 [3], table 1.
    /*
     * CharacterField: this 16-byte field shall define the label. It shall be coded as a string of up to 16 characters, which are
     * chosen from the character set signalled by the Charset field in the first byte of the FIG type 1 data field. The characters
     * are coded from byte 15 to byte 0. The first character starts at byte 15. Bytes at the end of the character field that are not
     * required to carry the label shall be filled with 0x00.
     */
    QString Name;  // interpreted CharacterField (16 Bytes)
    QString NameShort;  // interpreted short name regarding CharFlagField
    u16 CharFlagField = 0;  // this 16-bit flag field shall indicate which of the characters of the character field are to be displayed in an abbreviated form of the label.
  };

  struct SFig1s0_EnsembleLabel : SFig1_DataField, SFigBase
  {
    u16 EId = -1; // (Ensemble Identifier) (EId): this 16-bit field shall identify the ensemble. The coding details are given in clause 6.4.
  };

  struct SFig1s1_ProgrammeServiceLabel : SFig1_DataField, SFigBase
  {
    u16 SId = -1; // this 16-bit field shall identify the service (see clause 6.3.1).
  };

  struct SFig1s4_ServiceComponentLabel : SFig1_DataField, SFigBase
  {
    /*
     * PD_Flag: this 1-bit flag shall indicate whether the Service Identifier (SId) field is used for Programme services or Data services, as follows:
     * 0: 16-bit SId, used for Programme services;
     * 1: 32-bit SId, used for Data services.
     */
    i8  PD_Flag = -1;
    i8  SCIdS = -1; // (Service Component Identifier within the Service): this 4-bit field shall identify the service component within the service. NOTE: The service component label is not used for the primary component, so the value 0 is reserved for future use.
    u32 SId = -1;   // (Service Identifier): this 16-bit or 32-bit field shall identify the service. The length of the SId shall be signalled by the P/D flag, see clause 5.2.2.1.
  };

  struct SFig1s5_DataServiceLabel : SFig1_DataField, SFigBase
  {
    u32 SId = -1; // this 32-bit field shall identify the service (see clause 6.3.1).
  };

  std::vector<SFig1s0_EnsembleLabel>         Fig1s0_EnsembleLabelVec; // vector can only have one element!
  std::vector<SFig1s1_ProgrammeServiceLabel> Fig1s1_ProgrammeServiceLabelVec;
  std::vector<SFig1s4_ServiceComponentLabel> Fig1s4_ServiceComponentLabelVec;
  std::vector<SFig1s5_DataServiceLabel>      Fig1s5_DataServiceLabelVec;

  struct SSId_SCIdS
  {
    u32 SId = 0;
    i8  SCIdS = -99;
  };

  using TMapServiceLabel_To_SId_SCIdS = std::map<QString, SSId_SCIdS>; // Key is service name -> SId
  TMapServiceLabel_To_SId_SCIdS serviceLabel_To_SId_SCIdS_Map;

  template<typename T> inline QString hex_str(const T iVal) { return QString("0x%1").arg(iVal, 0, 16); }

  const SFig1s1_ProgrammeServiceLabel * get_Fig1s1_ProgrammeServiceLabel_of_SId(u32 SId) const;
  const SFig1s4_ServiceComponentLabel * get_Fig1s4_ServiceComponentLabel_of_SId_SCIdS(u32 SId, i8 SCIdS) const;
  const SFig1s5_DataServiceLabel      * get_Fig1s5_DataServiceLabel_of_SId(u32 SId) const;
  const QString                       & get_service_label_of_SId_from_all_Fig1(u32 iSId) const;
  const SSId_SCIdS                    * get_SId_SCIdS_from_service_label(const QString & s) const;

  void print_Fig1s0_EnsembleLabel(SStatistic & ioS, bool iCollectStatisticsOnly);
  void print_Fig1s1_ProgrammeServiceLabelVec(SStatistic & ioS, bool iCollectStatisticsOnly);
  void print_Fig1s4_ServiceComponentLabel(SStatistic & ioS, bool iCollectStatisticsOnly);
  void print_Fig1s5_DataServiceLabel(SStatistic & ioS, bool iCollectStatisticsOnly);
};

#endif
