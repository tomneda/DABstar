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
  u32 SId;
  i32 SCIdS;
  bool hasName;
  QString serviceLabel;
  i32 language;
  i32 programType;
  bool is_shown;
  i32 fmFrequency;
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

  using TMapService = std::map<u32, Service>;
  TMapService servicesMap;
};

struct SSubChannelDescFig0_1
{
  i32 SubChId  = 0;
  i32 startAddr = 0;
  i32 Length = 0;
  bool shortForm = false;
  i32 protLevel = 0;
  i32 bitRate = 0;
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

class DabConfig
{
public:
  DabConfig()
  {
    reset();
  }

  ~DabConfig() = default;

  void reset()
  {
    subChDescMapFig0_1.clear();
    subChDescMapFig0_5.clear();
    subChDescMapFig0_14.clear();
    mapServiceCompDesc_SCId.clear();
    mapServiceCompDesc_SId.clear();
    mapServiceCompDesc_SId_SubChId.clear();
    mapServiceCompDesc_SId_SCIdS.clear();
    mapServiceCompDesc_SId_CompNr.clear();
    serviceCompPtrStorage.clear();
    
    // for (auto & serviceComp : serviceComps)
    // {
    //   serviceComp.reset();
    // }
  }

  using TMapSubChDescFig0_1  = std::map<i32, SSubChannelDescFig0_1>;  // key is SubChId
  using TMapSubChDescFig0_5  = std::map<i32, SSubChannelDescFig0_5>;  // key is SubChId
  using TMapSubChDescFig0_14 = std::map<i32, SSubChannelDescFig0_14>; // key is SubChId

  TMapSubChDescFig0_1  subChDescMapFig0_1;
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
