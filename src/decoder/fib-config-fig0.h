//
// Created by tomneda on 2025-09-15.
//
#ifndef  FIB_CONFIG_FIG0_H
#define  FIB_CONFIG_FIG0_H

#include "glob_data_types.h"
#include <vector>
#include <QString>
#include <chrono>

struct SFig0s0_EnsembleInformation
{
  union
  {
    u16 EId = - 1;
    struct { u16 EnsembleId : 12; u16 CountryId : 4; };
  };
  u8 ChangeFlags = - 1;
  u8 AlarmFlag = - 1;
  u8 CIFCountHi = - 1;
  u8 CIFCountLo = - 1;
  u8 OccurrenceChange = - 1;
};

struct SSubChannelDescFig0_14
{
  i16 FEC_scheme = 0;
};

// cluster is for announcement handling
struct Cluster
{
  u16 flags = 0;
  bool inUse = false;
  i32 announcing = 0;
  i32 clusterId = -1;
  std::vector<u32> servicesSIDs;
};

class FibConfigFig0
{
public:
  FibConfigFig0();
  ~FibConfigFig0() = default;

  void reset();

  struct SFigBase
  {
    using TTP = std::chrono::time_point<std::chrono::steady_clock>;
    TTP TimePoint{};
    TTP TimePoint2ndCall{};
    void set_current_time() { TimePoint = std::chrono::steady_clock::now(); }
    void set_current_time_2nd_call() { if (TimePoint2ndCall ==  TTP()) TimePoint2ndCall = std::chrono::steady_clock::now(); }
  };

  struct SFig0s1_BasicSubChannelOrganization : SFigBase
  {
    i8  SubChId = -1;          // this 6-bit field, coded as an unsigned binary number, shall identify a sub-channel.
    i16 StartAddr = -1;        // this 10-bit field, coded as an unsigned binary number (in the range 0 to 863), shall address the first Capacity Unit (CU) of the sub-channel.

    // short form (ShortLongForm == 0)
    i8  TableSwitch = -1;      // this 1-bit flag shall indicate whether table 8 is signalled or there is some other use of the table index field
    i8  TableIndex = -1;       // his 6-bit field, coded as an unsigned binary number, contains an index which shall identify one of the 64 options available for the sub-channel size and protection level.

    // long form (ShortLongForm == 1)
    i8  Option = -1;           // this 3-bit field shall indicate the option used for the long form coding. Two options (000 and 001) are defined to provide Equal Error Protection as defined in clause 11.3.2.
    i8  ProtectionLevel = -1;  // this 2-bit field shall indicate the protection level as follows: option = 0: 1-A .. 4-A (Table 9), option = 1: 1-B .. 4-B (Table 10)
    i16 SubChannelSize = -1;   // this 10-bit field, coded as an unsigned binary number (in the range 1 to 864), shall define the number of Capacity Units occupied by the sub-channel.

    // derived
    i32 BitRate = -1;
    bool ShortForm = false;    // his 1-bit flag shall indicate whether the short or the long form of the size and protection field is used. true: short form
  };

  struct SFig0s2_ServiceComponentDefinition
  {
    i8 TMId = -1;  // TMId (Transport Mechanism Identifier): this 2-bit field shall indicate the transport mechanism used, as follows: 0 = MSC - Stream mode - audio; 1 = MSC - Stream mode - data; 2 = Reserved; 3 = MSC - Packet mode - data.
    union
    {
      i16 dummy = -1;  // for init the union
      struct { u8 SubChId; u8 ASCTy; } TMId00; // ASCTy (Audio Service Component Type): this 6-bit field shall indicate the type of the audio service component. The interpretation of this field shall be as defined in ETSI TS 101 756 [3], table 2a.
      struct { u8 SubChId; u8 DSCTy; } TMId01; // DSCTy (Data Service Component Type):  this 6-bit field shall indicate the transport protocol used by the data service component. The interpretation of this field shall be as defined in ETSI TS 101 756 [3], table 2b.
                                               // SubChId (Sub-channel Identifier): this 6-bit field shall identify the sub-channel in which the service component is carried.
      struct { u16 SCId;             } TMId11; // SCId (Service Component Identifier): this 12-bit field shall uniquely identify the service component within the ensemble.
    };
    i8 PS_Flag = -1;  // P/S (Primary/Secondary): this 1-bit flag shall indicate whether the service component is the primary one, as follows: 0: secondary, 1: primary
    i8 CA_Flag = -1;  // CA flag: this 1-bit field flag shall indicate whether access control applies to the service component
  };

  struct SFig0s2_BasicService_ServiceCompDef : SFigBase
  {
    constexpr static i8 cNumServiceCompMax = 12;
    u32 get_SId() const { return PD_Flag == 0 ? PD0.SId : PD1.SId; }
    i8 PD_Flag = -1;  // to select one of the both unions PD0 or PD1 below (programme or data service)
    // SId (Service Identifier): this 16-bit or 32-bit field shall identify the service.
    // Country Id (Identification): this 4-bit field shall be as defined in ETSI TS 101 756 [3], tables 3 to 7.
    // ECC (Extended Country Code): this 8-bit field shall be as defined in ETSI TS 101 756 [3], tables 3 to 7.
    union { u16 SId = 0; struct { u16 ServiceReference: 12; u16 CountryId: 4; }; } PD0;
    union { u32 SId = 0; struct { u32 ServiceReference: 20; u32 CountryId: 4; u32 EEC: 8; }; } PD1;
    // i8  CAId = -1;           // this 3-bit field shall identify the Access Control System (ACS) used for the service. The definition is given in ETSI TS 102 367 [4].
    i8  NumServiceComp = -1; // this 4-bit field, coded as an unsigned binary number, shall indicate the number of service components (maximum 12 for 16-bit SIds and maximum 11 for 32-bit SIds), associated with the service. Each component shall be coded, according to the transport mechanism used.
    i32 ServiceComp_C_index = -1; // index of the ServiceComp_C in the ServiceCompVec
    SFig0s2_ServiceComponentDefinition ServiceComp_C{};
    // std::array<SFig0s2_ServiceComponentDefinition, 16> ServiceCompVec{}; // 16-bit field
  };

  struct SFig0s3_ServiceComponentPacketMode : SFigBase
  {
    i16 SCId = -1;          // (Service Component Identifier): see clause 6.3.1 (FIG 0/2).
    i8  CAOrg_Flag = -1;    // this 1-bit flag shall indicate whether the Conditional Access Organization (CAOrg) field is present (CAOrg_Flag == 1), or not (CAOrg_Flag == 0).
    i8  DG_Flag = -1;       // this 1-bit flag shall indicate whether data groups are used (DG_FLAG == 0 !) to transport the service component as follows:
    i8  DSCTy = -1;         // (Data Service Component Type): see clause 6.3.1 (FIG 0/2).
    i8  SubChId = -1;       // (Sub-channel Identifier): see clause 6.3.1 (FIG 0/2).
    i16 PacketAddress = -1; // this 10-bit field shall define the address of the packet in which the service component is carried.
    u16 CAOrg = -1;         // this 16-bit field shall contain information about the applied Conditional Access Systems and mode (see ETSI TS 102 367 [4]).
  };

  struct SFig0s5_ServiceComponentLanguage : SFigBase
  {
    i8  LS_Flag = -1;  // this 1-bit flag shall indicate whether the service component identifier takes the short or the long form, as follows: 0: short form; 1: long form.
    u8  Language = -1; // this 8-bit field shall indicate the language of the audio or data service component. It shall be coded according to ETSI TS 101 756 [3], tables 9 and 10.

    // LS_Flag == 0: short form for SubChId
    i8  SubChId = -1;  // (Sub-channel Identifier): this 6-bit field shall identify the sub-channel in which the service component is carried.

    // LS_Flag == 1: long form for SCId
    i16 SCId = -1;     // this 12-bit field shall identify the service component (see clause 6.3.1).
  };

  struct SFig0s8_ServiceComponentGlobalDefinition : SFigBase
  {
    i8  PD_Flag = -1;  // programme or data service
    u32 SId = -1;      // (Service Identifier): this 16-bit or 32-bit field shall identify the service. The length of the SId shall be signalled by the P/D flag, see clause 5.2.2.1.
    i8  Ext_Flag = -1; // (Extension) flag: this 1-bit flag shall indicate whether or not the 8-bit Rfa field is present, as follows: 0: Rfa field absent; 1: Rfa field present.
    i8  SCIdS = -1;    // (Service Component Identifier within the Service): this 4-bit field shall identify the service component within the service. The primary service component shall use the value 0. Each secondary service component of the service shall use a different SCIdS value other than 0.
    i8  LS_Flag = -1;  // this 1-bit flag shall indicate whether the service component identifier takes the short or the long form, as follows: 0: short form; 1: long form.

    // LS_Flag == 0: short form for SubChId
    i8  SubChId = -1;  // (Sub-channel Identifier): this 6-bit field shall identify the sub-channel in which the service component is carried.

    // LS_Flag == 1: long form for SCId
    i16 SCId = -1;     // this 12-bit field shall identify the service component (see clause 6.3.1).
  };

  struct SFig0s9_CountryLtoInterTab : SFigBase
  {
    i8  Ext_Flag = -1;      // this 1-bit flag shall indicate whether the Extended field is present or not, as follows: 0: extended field absent; 1: extended field present.
    i8  Ensemble_LTO = -1;  // (Local Time Offset): this 6-bit field shall give the Local Time Offset (LTO) for the ensemble. It is expressed in multiples of half hours in the range -15,5 hours to +15,5 hours.
                            // Bit b5 shall give the sense of the LTO, as follows: 0: positive offset; 1: negative offset.
    u8  Ensemble_ECC = -1;  // (Extended Country Code): this 8-bit field shall make the Ensemble Id unique worldwide. The ECC shall be as defined in ETSI TS 101 756 [3], tables 3 to 7
    u8  InterTableId = -1;  // this 8-bit field shall be used to select an international table. The interpretation of this field shall be as defined in ETSI TS 101 756 [3], table 11.
    // std::array<u8, 25> Extended_Field{};  // this n × 8-bit field shall contain one or more sub-fields, which define those services for which their ECC differs from that of the ensemble. The maximum length of the extended field is 25 bytes.

    // derived
    i16 LTO_minutes = 0;    // offset of local time to UTC time in minutes
  };

  struct SFig0s10_DateAndTime : SFigBase
  {
    u32 MJD = -1;       // (Modified Julian Date): this 17-bit binary number shall define the current date according to the Modified Julian
                        // coding strategy. This number increments daily at 0000 Co-ordinated Universal Time (UTC) and extends over the range
                        // 0 to 99 999, where MJD 0 is 1858-11-17. As an example, MJD 58 000 corresponds to 2017-09-04.
    u8  LSI = -1;       // (Leap Second Indicator): this 1-bit flag shall be set to "1" for the period of one hour before the occurrence of a leap second.
    u8  UTC_Flag = -1;  // this 1-bit field shall indicate whether the UTC (see below) takes the short form or the long form, as follows: 0: UTC short form (legacy support only); 1: UTC long form.

    union
    {
      u32 UTC = -1;
      // u32 UTCx = (1 << 22) | (2 << 16) | (3 << 10) | (4 << 0);
      struct {                                         u32 Minutes : 6; u32 Hours : 5;  u32 dummy : 21; } UTC0; // short form
      struct { u32 MilliSeconds : 10; u32 Seconds : 6; u32 Minutes : 6; u32 Hours : 5;  u32 dummy :  5; } UTC1; // long form
    };
  };

  struct SFig0s13_UserApplicationInformation : SFigBase
  {
    i8  PD_Flag = -1;    // programme or data service
    u32 SId = -1;        // (Service Identifier): this 16-bit or 32-bit field shall identify the service (see clause 6.3.1) and the length of the SId shall be signalled by the P/D flag (see clause 5.2.2.1).
    i8  SCIdS = -1;      // (Service Component Identifier within the Service): this 4-bit field shall identify the service component within the service. The combination of the SId and the SCIdS provides a globally valid identifier for a service component.
    i16 NumUserApps = 0; // his 4-bit field, expressed as an unsigned binary number, shall indicate the number of user applications (in the range 1 to 6) contained in the subsequent list.
    // struct SXPadData
    // {
    //   i8  CA_Flag = -1;
    //   i8  CA_Org_Flag = -1;
    //   i8  XPad_AppTy = -1;
    //   i8  DG_Flag = -1;
    //   i8  DSCTy = -1;
    //   u16 CA_Org = 0;
    // };
    struct SUserApp
    {
      i16 UserAppType = -1;      // this 11-bit field identifies the user application that shall be used to decode the data in the channel identified by SId and SCIdS. The interpretation of this field shall be as defined in ETSI TS 101 756 [3], table 16.
      i16 UserAppDataLength = 0; // this 5-bit field, expressed as an unsigned binary number (in the range 0 to 23), indicates the length in bytes of the X-PAD data field (when present) and User Application data field that follows.
      u32 XPadData = 0;          // this field is only present for applications carried in the X-PAD of an audio service component.
      // SXPadData XPadData;
      // i16 UserAppData[16];
    };
    std::array<SUserApp, 6> UserAppVec;
    u16 SizeBits = 0;          // Helper which stores the entire size of the dataset if avoid repeated read-outs of the FIB data
  };

  struct SFig0s14_SubChannelOrganization : SFigBase
  {
    i8  SubChId = -1;     // this 6-bit field, coded as an unsigned binary number, shall identify a sub-channel.
    i16 FEC_scheme = -1;  // this 2-bit field shall indicate the Forward Error Correction scheme in use, as follows: 0 = no FEC, 1 = FEC scheme applied.
  };

  struct SFig0s17_ProgrammeType : SFigBase
  {
    u16 SId = 0;      // this 16-bit field shall identify the service (see clause 6.3.1).
    i8  SD_Flag = -1; // this 1-bit flag shall indicate that the Programme Type code signalled in the programme type field, represents the current programme contents, as follows:
                      //     0: Programme Type code may not represent the current programme contents; 1: Programme Type code represents the current programme contents.
    i8  IntCode = -1; // this 5-bit field shall specify the basic Programme Type (PTy) category. This code is chosen from an international table (see clause 8.1.3.2).
  };

  std::vector<SFig0s1_BasicSubChannelOrganization> Fig0s1_BasicSubChannelOrganizationVec;
  std::vector<SFig0s2_BasicService_ServiceCompDef> Fig0s2_BasicService_ServiceCompDefVec;
  std::vector<SFig0s3_ServiceComponentPacketMode> Fig0s3_ServiceComponentPacketModeVec;
  std::vector<SFig0s5_ServiceComponentLanguage> Fig0s5_ServiceComponentLanguageVec;
  std::vector<SFig0s8_ServiceComponentGlobalDefinition> Fig0s8_ServiceComponentGlobalDefinitionVec;
  std::vector<SFig0s9_CountryLtoInterTab> Fig0s9_CountryLtoInterTabVec;
  std::vector<SFig0s13_UserApplicationInformation> Fig0s13_UserApplicationInformationVec;
  std::vector<SFig0s14_SubChannelOrganization> Fig0s14_SubChannelOrganizationVec;
  std::vector<SFig0s17_ProgrammeType> Fig0s17_ProgrammeTypeVec;

  const SFig0s1_BasicSubChannelOrganization      * get_Fig0s1_BasicSubChannelOrganization_of_SubChId(i32 iSubChId) const;
  const SFig0s2_BasicService_ServiceCompDef      *   get_Fig0s2_BasicService_ServiceCompDef_of_SId_TMId(u32 iSId, u8 iTMId) const;
  const SFig0s2_BasicService_ServiceCompDef      * get_Fig0s2_BasicService_ServiceCompDef_of_SId(u32 iSId) const;
  const SFig0s2_BasicService_ServiceCompDef      * get_Fig0s2_BasicService_ServiceCompDef_of_SId_ScIdx(u32 iSId, i32 iScIdx) const;
  const SFig0s2_BasicService_ServiceCompDef      * get_Fig0s2_BasicService_ServiceCompDef_of_SCId(i16 SCId) const;
  const SFig0s3_ServiceComponentPacketMode       * get_Fig0s3_ServiceComponentPacketMode_of_SCId(i32 iSCId) const;
  const SFig0s5_ServiceComponentLanguage         * get_Fig0s5_ServiceComponentLanguage_of_SubChId(u8 iSubChId) const;
  const SFig0s5_ServiceComponentLanguage         * get_Fig0s5_ServiceComponentLanguage_of_SCId(u8 iSCId) const;
  const SFig0s8_ServiceComponentGlobalDefinition * get_Fig0s8_ServiceComponentGlobalDefinition_of_SId(u32 iSId) const;
  const SFig0s8_ServiceComponentGlobalDefinition * get_Fig0s8_ServiceComponentGlobalDefinition_of_SId_SCIdS(u32 iSId, u8 iSCIdS) const;
  const SFig0s9_CountryLtoInterTab               * get_Fig0s9_CountryLtoInterTab() const;
  const SFig0s13_UserApplicationInformation      * get_Fig0s13_UserApplicationInformation_of_SId_SCIdS(u32 iSId, i32 iSCIdS) const;
  const SFig0s14_SubChannelOrganization          * get_Fig0s14_SubChannelOrganization_of_SubChId(i32 iSubChId) const;
  const SFig0s17_ProgrammeType                   * get_Fig0s17_ProgrammeType_of_SId(u16 iSId) const;

  QString format_time(const SFigBase & iFigBase) const;

  void print_Fig0s1_BasicSubChannelOrganization() const;
  void print_Fig0s2_BasicService_ServiceCompDef() const;
  void print_Fig0s3_ServiceComponentPacketMode() const;
  void print_Fig0s5_ServiceComponentLanguage() const;
  void print_Fig0s8_ServiceComponentGlobalDefinition() const;
  void print_Fig0s9_CountryLtoInterTab() const;
  void print_Fig0s13_UserApplicationInformation() const;
  void print_Fig0s14_SubChannelOrganization() const;
  void print_Fig0s17_ProgrammeType() const;

  template<typename T> inline QString hex_str(const T iVal) const { return QString("0x%1").arg(iVal, 0, 16); }

  Cluster mClusterTable[128];
};

#endif
