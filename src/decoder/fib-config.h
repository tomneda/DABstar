/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2018, 2019, 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program              
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
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef  FIB_CONFIG_H
#define  FIB_CONFIG_H

#include "glob_data_types.h"
#include "dab-constants.h"
#include <vector>
#include <map>
#include <QString>

struct Service
{
  u32 SId = -1;
  i32 SCIdS = -1;
  bool hasName = false;
  QString serviceLabel;
  i32 language = -1;
  i32 programType = -1;
  bool is_shown = false;
  i32 fmFrequency = -1;
  std::vector<SEpgElement> epgData;
};

class EnsembleDescriptor
{
public:
  EnsembleDescriptor()
  {
    reset();
  }

  ~EnsembleDescriptor() = default;

  void reset()
  {
    namePresent = false;
    ecc_Present = false;
    countryId = 0;
    isSynced = false;
    servicesMap.clear();
  }

  QString ensembleName;
  i32 ensembleId;
  bool namePresent;
  bool ecc_Present;
  u8 ecc_byte;
  u8 countryId;
  bool isSynced;

  using TMapService = std::map<u32, Service>; // Key is SId
  TMapService servicesMap;
};

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


struct SSubChannelDescFig0_5
{
  i16 language = 0;
};

struct SSubChannelDescFig0_14
{
  i16 FEC_scheme = 0;
};

//      The service component describes the actual service
//      It really should be a union, the component data for
//      audio and data are quite different
struct ServiceComponentDescriptor
{
// public:
//   ServiceComponentDescriptor()
//   {
//     reset();
//   }
//
//   ~ServiceComponentDescriptor() = default;
//
//   void reset()
//   {
//     inUse = false;
//     isMadePublic = false;
//     SCIdS = -1;
//     componentNr = -1;
//     SCId = -1;
//     subChannelId = -1;
//   }
//
//   bool inUse = false;        // field in use
  ETMId TMid = (ETMId)(-1);  // the transport mode
  u32 SId = 0;               
  i16 SCIdS = -1;            // component within service
  i16 subChannelId = -1;     // used in both audio and packet
  i16 componentNr = -1;      // component
  i16 ASCTy = -1;            // used for audio
  i16 DSCTy = -1;            // used in packet
  i16 PsFlag = 0;            // use for both audio and packet
  i16 SCId = -1;             // Component Id (12 bit, unique)
  u8 CaFlag = 0;             // used in packet (or not at all)
  u8 DgFlag = 0;             // used for TDC
  i16 packetAddress = -1;    // used in packet
  i16 appType = -1;          // used in packet and Xpad
  i16 language = -1;
  bool isMadePublic = false; // used to make service visible
};

//
// cluster is for announcement handling
struct Cluster
{
  u16 flags = 0;
  bool inUse = false;
  i32 announcing = 0;
  i32 clusterId = -1;
  std::vector<u32> servicesSIDs;
};

class FibConfig
{
public:
  FibConfig();
  ~FibConfig() = default;

  void reset();

  struct SFig0s1_BasicSubChannelOrganization
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

  struct SFig0s2_BasicService_ServiceComponentDefinition
  {
    u8 PD_Flag = -1;  // to select one of the both unions PD0 or PD1 below
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

  struct SFig0s3_ServiceComponentPacketMode
  {
    i16 SCId = -1;          // (Service Component Identifier): see clause 6.3.1 (FIG 0/2).
    i8  CAOrg_Flag = -1;    // this 1-bit flag shall indicate whether the Conditional Access Organization (CAOrg) field is present (CAOrg_Flag == 1), or not (CAOrg_Flag == 0).
    i8  DG_Flag = -1;       // this 1-bit flag shall indicate whether data groups are used (DG_FLAG == 0 !) to transport the service component as follows:
    i8  DSCTy = -1;         // (Data Service Component Type): see clause 6.3.1 (FIG 0/2).
    i8  SubChId = -1;       // (Sub-channel Identifier): see clause 6.3.1 (FIG 0/2).
    i16 PacketAddress = -1; // this 10-bit field shall define the address of the packet in which the service component is carried.
    u16 CAOrg = -1;         // this 16-bit field shall contain information about the applied Conditional Access Systems and mode (see ETSI TS 102 367 [4]).
  };

  struct SFig0s8_ServiceComponentGlobalDefinition
  {
    u8  PD_Flag = -1;
    u32 SId = -1;      // (Service Identifier): this 16-bit or 32-bit field shall identify the service. The length of the SId shall be signalled by the P/D flag, see clause 5.2.2.1.
    i8  Ext_Flag = -1; // (Extension) flag: this 1-bit flag shall indicate whether or not the 8-bit Rfa field is present, as follows: 0: Rfa field absent; 1: Rfa field present.
    i8  SCIdS = -1;    // (Service Component Identifier within the Service): this 4-bit field shall identify the service component within the service. The primary service component shall use the value 0. Each secondary service component of the service shall use a different SCIdS value other than 0.
    i8  LS_Flag = -1;  // this 1-bit flag shall indicate whether the service component identifier takes the short or the long form, as follows: 0: short form; 1: long form.

    // LS_Flag == 0: short form for SubChId
    i8  SubChId = -1;  // (Sub-channel Identifier): this 6-bit field shall identify the sub-channel in which the service component is carried.

    // LS_Flag == 1: long form for SCId
    i16 SCId = -1;     // this 12-bit field shall identify the service component (see clause 6.3.1).
  };

  struct SFig0s13_UserApplicationInformation
  {
    i8  PD_Flag = -1;
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
    std::array<SUserApp, 24> UserAppVec;
    u16 SizeBits = 0;          // Helper which stores the entire size of the dataset if avoid repeated read-outs of the FIB data
  };

  struct SFig0s14_SubChannelOrganization
  {
    i8  SubChId = -1;     // this 6-bit field, coded as an unsigned binary number, shall identify a sub-channel.
    i16 FEC_scheme = -1;  // this 2-bit field shall indicate the Forward Error Correction scheme in use, as follows: 0 = no FEC, 1 = FEC scheme applied.
  };

  // struct SId_struct // TODO: refactor SId_struct
  // {
  //   u32 SId;
  //   std::vector<i32> comps;
  //   u16 announcing;
  // };
  //
  // std::vector<SId_struct> SId_table;

  std::vector<SFig0s1_BasicSubChannelOrganization> Fig0s1_BasicSubChannelOrganizationVec;
  std::vector<SFig0s2_BasicService_ServiceComponentDefinition> Fig0s2_BasicService_ServiceComponentDefinitionVec;  // TODO: serviceComp_C
  std::vector<SFig0s3_ServiceComponentPacketMode> Fig0s3_ServiceComponentPacketModeVec;
  std::vector<SFig0s8_ServiceComponentGlobalDefinition> Fig0s8_ServiceComponentGlobalDefinitionVec;
  std::vector<SFig0s13_UserApplicationInformation> Fig0s13_UserApplicationInformationVec;
  std::vector<SFig0s14_SubChannelOrganization> Fig0s14_SubChannelOrganizationVec;


  SFig0s1_BasicSubChannelOrganization * get_Fig0s1_BasicSubChannelOrganization_of_SubChId(i32 iSubChId);
  SFig0s2_BasicService_ServiceComponentDefinition * get_Fig0s2_BasicService_ServiceComponentDefinition_of_SId(u32 iSId);
  SFig0s2_BasicService_ServiceComponentDefinition * get_Fig0s2_BasicService_ServiceComponentDefinition_of_SId_ScIdx(const u32 iSId, i32 iScIdx);
  SFig0s3_ServiceComponentPacketMode * get_Fig0s3_ServiceComponentPacketMode_of_SCId(i32 iSCId);
  SFig0s8_ServiceComponentGlobalDefinition * get_Fig0s8_ServiceComponentGlobalDefinition_of_SId(const u32 iSId);
  SFig0s13_UserApplicationInformation * get_Fig0s13_UserApplicationInformation_of_SId_SCIdS(const u32 iSId, const i32 iSCIdS);
  SFig0s14_SubChannelOrganization * get_Fig0s14_SubChannelOrganization_of_SubChId(i32 iSubChId);
  // SId_struct * get_SIdTable_of_SId(const u32 iSId); // TODO: refactor SId_struct

  
  void print_Fig0s1_BasicSubChannelOrganization();
  void print_Fig0s2_BasicService_ServiceComponentDefinition();
  void print_Fig0s3_ServiceComponentPacketMode();
  void print_Fig0s8_ServiceComponentGlobalDefinition();
  void print_Fig0s13_UserApplicationInformation();
  void print_Fig0s14_SubChannelOrganization();
  void print_SIdTable_of_SId(); // TODO: refactor SId_struct



  // older style
  using TMapSubChDescFig0_5  = std::map<i32, SSubChannelDescFig0_5>;  // key is SubChId
  using TMapSubChDescFig0_14 = std::map<i32, SSubChannelDescFig0_14>; // key is SubChId

  TMapSubChDescFig0_5  subChDescMapFig0_5;
  TMapSubChDescFig0_14 subChDescMapFig0_14;

  using TSPServiceCompDesc = std::shared_ptr<ServiceComponentDescriptor>;

  std::vector<TSPServiceCompDesc> serviceCompPtrStorage;
  std::map<u32, TSPServiceCompDesc> mapServiceCompDesc_SCId; // key is SCId
  std::map<u32, TSPServiceCompDesc> mapServiceCompDesc_SId;  // key is SId
  std::map<std::pair<u32 /*SId*/, i16 /*SubChId*/>, TSPServiceCompDesc> mapServiceCompDesc_SId_SubChId;
  std::map<std::pair<u32 /*SId*/, u8 /*SCIdS*/>, TSPServiceCompDesc> mapServiceCompDesc_SId_SCIdS;
  std::map<std::pair<u32 /*SId*/, i16 /*CompNr*/>, TSPServiceCompDesc> mapServiceCompDesc_SId_CompNr;
  //TMapServiceCompDesc serviceCompDescMap;
  // ServiceComponentDescriptor serviceComps[64];
  Cluster clusterTable[128];
};

#endif
