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
 *
 * 	fib decoder. Functionality is shared between fic handler, i.e. the
 *	one preparing the FIC blocks for processing, and the mainthread
 *	from which calls are coming on selecting a program
 *
 *
 *	Definition of the "configuration" as maintained during reception of
 *	a channel
 */
#ifndef  FIB_CONFIG_H
#define  FIB_CONFIG_H

class Service
{
public:
  Service()
  {
    reset();
  }

  ~Service() = default;

  void reset()
  {
    inUse = false;
    SId = 0;
    SCIds = 0;
    hasName = false;
    serviceLabel = "";
    language = 0;
    programType = 0;
    is_shown = false;
    fmFrequency = -1;
    epgData.resize(0);
  }

  bool inUse;
  uint32_t SId;
  int SCIds;
  bool hasName;
  QString serviceLabel;
  int language;
  int programType;
  bool is_shown;
  int32_t fmFrequency;
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
    for (auto & service : services)
    {
      service.reset();
    }
  }

  QString ensembleName;
  int32_t ensembleId;
  bool namePresent;
  bool ecc_Present;
  uint8_t ecc_byte;
  uint8_t countryId;
  bool isSynced;
  Service services[64];
};

class SubChannelDescriptor
{
public:
  SubChannelDescriptor()
  {
    reset();
  }

  ~SubChannelDescriptor() = default;

  void reset()
  {
    inUse = false;
    language = 0;
    FEC_scheme = 0;
    SCIds = 0;
  }

  bool inUse = false;
  int32_t SubChId  = 0;
  int32_t startAddr = 0;
  int32_t Length = 0;
  bool shortForm = false;
  int32_t protLevel = 0;
  int32_t bitRate = 0;
  int16_t language = 0;
  int16_t FEC_scheme = 0;
  int16_t SCIds = 0;    // for audio channels
};

//      The service component describes the actual service
//      It really should be a union, the component data for
//      audio and data are quite different
class ServiceComponentDescriptor
{
public:
  ServiceComponentDescriptor()
  {
    reset();
  }

  ~ServiceComponentDescriptor() = default;

  void reset()
  {
    inUse = false;
    isMadePublic = false;
    SCIds = -1;
    componentNr = -1;
    SCId = -1;
    subChannelId = -1;
  }

  bool inUse;            // field in use
  int8_t TMid;           // the transport mode
  uint32_t SId;
  int16_t SCIds;         // component within service
  int16_t subChannelId;  // used in both audio and packet
  int16_t componentNr;   // component
  int16_t ASCTy;         // used for audio
  int16_t DSCTy;         // used in packet
  int16_t PsFlag;        // use for both audio and packet
  uint16_t SCId;         // Component Id (12 bit, unique)
  uint8_t CaFlag;        // used in packet (or not at all)
  uint8_t DgFlag;        // used for TDC
  int16_t packetAddress; // used in packet
  int16_t appType;       // used in packet and Xpad
  int16_t language;
  bool isMadePublic;     // used to make service visible
};

//
//	cluster is for announcement handling
class Cluster
{
public:
  uint16_t flags;
  std::vector<uint16_t> services;
  bool inUse;
  int announcing;
  int clusterId;

  Cluster()
  {
    flags = 0;
    services.resize(0);
    inUse = false;
    announcing = 0;
    clusterId = -1;
  }

  ~Cluster()
  {
    flags = 0;
    services.resize(0);
    inUse = false;
    announcing = 0;
    clusterId = -1;
  }
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
    for (auto & subChannel : subChannels)
    {
      subChannel.reset();
    }

    for (auto & serviceComp : serviceComps)
    {
      serviceComp.reset();
    }
  }

  SubChannelDescriptor subChannels[64];
  ServiceComponentDescriptor serviceComps[64];
  Cluster clusterTable[128];
};

#endif
