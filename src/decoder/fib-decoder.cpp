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
 */
#include "fib-decoder.h"
#include "dabradio.h"
#include "charsets.h"
#include "fib-config.h"
#include "bit-extractors.h"
#include <cstring>
#include <vector>


/*static*/ std::unique_ptr<IFibDecoder> FibDecoderFactory::create(DabRadio* radio)
{
  return std::make_unique<FibDecoder>(radio);
}

FibDecoder::FibDecoder(DabRadio * mr)
  : myRadioInterface(mr)
  , ensemble(std::make_unique<EnsembleDescriptor>())
  , currentConfig (std::make_unique<FibConfig>())
  , nextConfig(std::make_unique<FibConfig>())
{
  //connect(this, &IFibDecoder::addtoEnsemble, myRadioInterface, &RadioInterface::slot_add_to_ensemble);
  connect(this, &IFibDecoder::signal_name_of_ensemble, myRadioInterface, &DabRadio::slot_name_of_ensemble);
  connect(this, &IFibDecoder::signal_clock_time, myRadioInterface, &DabRadio::slot_clock_time);
  connect(this, &IFibDecoder::signal_change_in_configuration, myRadioInterface, &DabRadio::slot_change_in_configuration);
  connect(this, &IFibDecoder::signal_start_announcement, myRadioInterface, &DabRadio::slot_start_announcement);
  connect(this, &IFibDecoder::signal_stop_announcement, myRadioInterface, &DabRadio::slot_stop_announcement);
  connect(this, &IFibDecoder::signal_nr_services, myRadioInterface, &DabRadio::slot_nr_services);

  mpTimerDataLoaded = new QTimer(this);
  connect(mpTimerDataLoaded, &QTimer::timeout, this, &FibDecoder::slot_timer_data_loaded);
  mpTimerDataLoaded->setInterval(1000);
  mpTimerDataLoaded->setSingleShot(true);
}

void FibDecoder::process_FIB(const std::array<std::byte, cFibSizeVitOut> & iFibBits, u16 const iFicNo)
{
  QMutexLocker lock(&mMutex); // TODO: maybe shift to emplace_back() place?

  // iFibBits: 30 Bytes Data * 8  + 16 Bit CRC = 32 Byte / 256 bits
  // The caller already does CRC16 check.
  i32 processedBytes = 0;

  while (processedBytes < 30)
  {
    const u8 * const d = reinterpret_cast<const u8 *>(iFibBits.data()) + processedBytes * 8;

    const u8 FIGtype = getBits_3(d, 0);
    const u8 FIGlength = getBits_5(d, 3);

    if (FIGtype == 0x07 && FIGlength == 0x1F) // end marker
    {
      break;
    }

    switch (FIGtype)
    {
    case 0: process_FIG0(d); break;
    case 1: process_FIG1(d); break;
    default: ;
    }

    processedBytes += FIGlength + 1; // data length plus header length
  }

  if (processedBytes > 30)
  {
    qCritical() << "FIG package length error" << processedBytes;
  }
}

FibDecoder::SFigHeader FibDecoder::get_fig_header(const u8 * const d)
{
  SFigHeader header;
  header.Length  = getBits_5(d, 3);
  header.CN_Flag = getBits_1(d, 8 + 0);
  header.OE_Flag = getBits_1(d, 8 + 1);
  header.PD_Flag = getBits_1(d, 8 + 2);
  return header; // should be "moved"    
}

//	Programme Type (PTy) 8.1.5
/////////////////////////////////////////////////////////////////////////
//	Support functions
//
//	bind_audioService is the main processor for - what the name suggests -
//	connecting the description of audioservices to a SID
//	by creating a service Component
void FibDecoder::bind_audio_service(FibConfig * iopDabConfig, const ETMId iTMid, const u32 iSId, const i16 iCompNr,
                                                             const i16 iSubChId, const i16 iPsFlag, const i16 iASCTy)
{
  const auto itServ = ensemble->servicesMap.find(iSId);

  if (itServ == ensemble->servicesMap.end())
  {
    return;
  }

  //	if (ensemble -> services [serviceIndex]. programType == 0)
  //	   return;

  if (iopDabConfig->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(iSubChId) == nullptr) // TODO: why necessary?
  {
    return;
  }

  if (iopDabConfig->mapServiceCompDesc_SId_CompNr.find({iSId, iCompNr}) != iopDabConfig->mapServiceCompDesc_SId_CompNr.end())
  {
    return; // already bound
  }

  if (currentConfig->serviceCompPtrStorage.size() >= 64)
  {
    qCritical("No free slots");
    return;
  }

  const auto pScd = std::make_shared<ServiceComponentDescriptor>();

  pScd->SId = iSId;
  pScd->SCIdS = 0;
  pScd->TMid = iTMid;
  pScd->componentNr = iCompNr;
  pScd->subChannelId = iSubChId;
  pScd->PsFlag = iPsFlag;
  pScd->ASCTy = iASCTy;
  itServ->second.SCIdS = 0;

  currentConfig->serviceCompPtrStorage.emplace_back(pScd);

  // currentConfig->mapServiceCompDesc_SCId.try_emplace(iSCId, pScd); // TODO
  currentConfig->mapServiceCompDesc_SId.try_emplace(iSId, pScd);
  currentConfig->mapServiceCompDesc_SId_SubChId.try_emplace({iSId, iSubChId}, pScd);
  currentConfig->mapServiceCompDesc_SId_CompNr.try_emplace({iSId, iCompNr}, pScd);
  currentConfig->mapServiceCompDesc_SId_SCIdS.try_emplace({iSId, pScd->SCIdS}, pScd);

  const QString dataName = itServ->second.serviceLabel;

  emit signal_add_to_ensemble(dataName, iSId); // emit audio services

  itServ->second.is_shown = true;
}

//      bind_packetService is the main processor for - what the name suggests -
//      connecting the service component defining the service to the SId,
//	So, here we create a service component. Note however,
//	that FIG0/3 provides additional data, after that we
//	decide whether it should be visible or not
void FibDecoder::bind_packet_service(FibConfig * iopDabConfig, const ETMId iTMId, const u32 iSId, const i16 iCompNr,
                                     const i16 iSCId, const i16 iPsFlag, const i16 iCaFlag)
{
  const auto itServ = ensemble->servicesMap.find(iSId);

  if (itServ == ensemble->servicesMap.end())
  {
    return;
  }

  if (iopDabConfig->mapServiceCompDesc_SId_CompNr.find({iSId, iCompNr}) != iopDabConfig->mapServiceCompDesc_SId_CompNr.end())
  {
    return; // already bound
  }

  if (currentConfig->serviceCompPtrStorage.size() >= 64)
  {
    qCritical("No free slots");
    return;
  }

  const auto pScd = std::make_shared<ServiceComponentDescriptor>();

  pScd->SId = iSId;

  if (iCompNr == 0) // TODO: stimmt das so?!
  {
    pScd->SCIdS = 0;
  }
  else
  {
    pScd->SCIdS = -1;
  }

  pScd->SCId = iSCId;
  pScd->TMid = iTMId;
  pScd->componentNr = iCompNr;
  pScd->PsFlag = iPsFlag;
  pScd->CaFlag = iCaFlag;
  pScd->isMadePublic = false;

  currentConfig->serviceCompPtrStorage.emplace_back(pScd);

  currentConfig->mapServiceCompDesc_SCId.try_emplace(iSCId, pScd);
  currentConfig->mapServiceCompDesc_SId.try_emplace(iSId, pScd);
  // currentConfig->mapServiceCompDesc_SId_SubChId.try_emplace({iSId, 0}, pScd); // TODO:
  currentConfig->mapServiceCompDesc_SId_CompNr.try_emplace({iSId, iCompNr}, pScd);
  currentConfig->mapServiceCompDesc_SId_SCIdS.try_emplace({iSId, pScd->SCIdS}, pScd);
}


EnsembleDescriptor::TMapService::iterator FibDecoder::find_service_from_service_name(const QString & s) const
{
  // TODO: extra map for this search?
  for (auto it = ensemble->servicesMap.begin(); it != ensemble->servicesMap.end(); ++it)
  {
    if (it->second.serviceLabel == s)
    {
      return it;
    }
  }

  return ensemble->servicesMap.end();
}

// i32 FibDecoder::find_service_index_from_SId(const u32 iSId) const
// {
//   bool unusedFound = false;
//   // TODO: make it faster with a map?
//   for (i32 i = 0; i < 64; i++)
//   {
//     unusedFound |= !ensemble->services[i].inUse;
//
//     if (ensemble->services[i].inUse &&
//         ensemble->services[i].SId == iSId)
//     {
//       if (unusedFound)
//       {
//         qWarning() << "Unused service entry found in find_service_index_from_SId() while search";
//       }
//       return i;
//     }
//   }
//   return -1;
// }

//	find data component using the SCId
FibConfig::TSPServiceCompDesc FibDecoder::find_service_component(FibConfig * db, i16 SCId) const
{
  const auto it = db->mapServiceCompDesc_SCId.find(SCId);

  if (it == db->mapServiceCompDesc_SCId.end())
  {
    return nullptr;
  }

  return it->second;
}

//
//	find serviceComponent using the SId and the SCIdS
FibConfig::TSPServiceCompDesc FibDecoder::find_service_component(FibConfig * ipDabConfig, u32 iSId, u8 iSCIdS) const
{
  const auto itServ = ensemble->servicesMap.find(iSId); // TODO: is that necessary?

  if (itServ == ensemble->servicesMap.end())
  {
    return nullptr;
  }

  const auto itScd = ipDabConfig->mapServiceCompDesc_SId_SCIdS.find({iSId, iSCIdS});
  return (itScd != ipDabConfig->mapServiceCompDesc_SId_SCIdS.end() ? itScd->second : nullptr);
}

//	find serviceComponent using the SId and the subchannelId
FibConfig::TSPServiceCompDesc FibDecoder::find_component(FibConfig * ipDabConfig, u32 iSId, i16 iSubChId) const
{
  const auto it = ipDabConfig->mapServiceCompDesc_SId_SubChId.find({iSId, iSubChId});
  return (it != ipDabConfig->mapServiceCompDesc_SId_SubChId.end() ? it->second : nullptr);
}

// called via FIG 1/1, FIG 1/4, FIG 1/5
void FibDecoder::create_service(QString iServiceName, u32 iSId, i32 iSCIdS)
{
  const auto it = ensemble->servicesMap.find(iSId);

  if (it != ensemble->servicesMap.end())  // check, should not happen
  {
    qWarning() << "Service already exists";
  }

  Service service;

  service.hasName = true;
  service.serviceLabel = iServiceName;
  service.SId = iSId;
  service.SCIdS = iSCIdS;

  ensemble->servicesMap.try_emplace(iSId, service);

  // qDebug() << "Created service" << i << ", inUse:" << service.inUse << ", ServiceLabel:" << service.serviceLabel << ", SId:" << service.SId << ", SCIdS:" << service.SCIdS;
}

//
//	called after a change in configuration to verify
//	the services health
//
void FibDecoder::cleanup_service_list()
{
  for (auto it = ensemble->servicesMap.begin(); it != ensemble->servicesMap.end(); ++it)
  {
    const u32 SId = it->second.SId;
    const i32 SCIdS = it->second.SCIdS;

    if (find_service_component(currentConfig.get(), SId, SCIdS) == nullptr)
    {
      qDebug() << "clean up service ID" << SId << "SCIdS" << SCIdS;
      it = ensemble->servicesMap.erase(it);
    }
  }
}

void FibDecoder::set_cluster(FibConfig * localBase, i32 clusterId, u32 iSId, u16 asuFlags)
{

  if (!sync_reached())
  {
    return;
  }

  Cluster * myCluster = get_cluster(localBase, clusterId);

  if (myCluster == nullptr)
  {
    return;
  }

  if (myCluster->flags != asuFlags)
  {
    //	   fprintf (stdout, "for cluster %d, the flags change from %x to %x\n",
    //	                       clusterId, myCluster -> flags, asuFlags);
    myCluster->flags = asuFlags;
  }

  for (auto SId : myCluster->servicesSIDs)
  {
    if (SId == iSId)
    {
      return;
    }
  }
  myCluster->servicesSIDs.push_back(iSId);
}

Cluster * FibDecoder::get_cluster(FibConfig * localBase, i16 clusterId)
{
  for (i32 i = 0; i < 64; i++)
  {
    if ((localBase->clusterTable[i].inUse) && (localBase->clusterTable[i].clusterId == clusterId))
    {
      return &(localBase->clusterTable[i]);
    }
  }

  for (i32 i = 0; i < 64; i++)
  {
    if (!localBase->clusterTable[i].inUse)
    {
      localBase->clusterTable[i].inUse = true;
      localBase->clusterTable[i].clusterId = clusterId;
      return &(localBase->clusterTable[i]);
    }
  }
  return &(localBase->clusterTable[0]);  // cannot happen
}

void FibDecoder::connect_channel()
{
  QMutexLocker lock(&mMutex);
  currentConfig->reset();
  nextConfig->reset();
  ensemble->reset();
  connect(this, &FibDecoder::signal_add_to_ensemble, myRadioInterface, &DabRadio::slot_add_to_ensemble);
}

void FibDecoder::disconnect_channel()
{
  QMutexLocker lock(&mMutex);
  disconnect(this, &FibDecoder::signal_add_to_ensemble, myRadioInterface, &DabRadio::slot_add_to_ensemble);
  currentConfig->reset();
  nextConfig->reset();
  ensemble->reset();
}

bool FibDecoder::sync_reached() const
{
  return ensemble->isSynced;
}

i32 FibDecoder::get_sub_channel_id(const QString & s, u32 dummy_SId) const
{
  QMutexLocker lock(&mMutex);
  const auto itService = find_service_from_service_name(s);

  if (itService == ensemble->servicesMap.end())  // this test was missed before so it should happen
  {
    qWarning() << "Service not found";
    return -1;
  }

  (void)dummy_SId;

  const i32 SId = itService->second.SId;
  const i32 SCIdS = itService->second.SCIdS;

  const auto pScd = find_service_component(currentConfig.get(), SId, SCIdS);

  if (pScd == nullptr)
  {
    return -1;
  }

  i32 subChId = pScd->subChannelId;
  return subChId;
}

void FibDecoder::get_data_for_audio_service(const QString & iS, AudioData * opAD) const
{
  QMutexLocker lock(&mMutex);
  opAD->defined = false;

  const auto itService = find_service_from_service_name(iS);

  if (itService == ensemble->servicesMap.end())
  {
    return;
  }

  const i32 SId = itService->second.SId;
  const i32 SCIdS = itService->second.SCIdS;

  const auto pScd = find_service_component(currentConfig.get(), SId, SCIdS);

  if (pScd == nullptr)
  {
    return;
  }

  if (pScd->TMid != ETMId::StreamModeAudio)
  {
    return;
  }

  const i32 subChId = pScd->subChannelId;

  const auto * const pFig0s1 = currentConfig->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(subChId);

  if (pFig0s1 == nullptr)
  {
    return;
  }

  opAD->SId = SId;
  opAD->SCIdS = SCIdS;
  opAD->subchId = subChId;
  opAD->serviceName = iS;
  opAD->startAddr = pFig0s1->StartAddr;
  opAD->shortForm = pFig0s1->ShortForm;
  opAD->protLevel = pFig0s1->ProtectionLevel;
  opAD->length = pFig0s1->SubChannelSize;
  opAD->bitRate = pFig0s1->BitRate;
  opAD->ASCTy = pScd->ASCTy;
  opAD->language = itService->second.language;
  opAD->programType = itService->second.programType;
  opAD->fmFrequency = itService->second.fmFrequency;
  opAD->defined = true;
}

void FibDecoder::get_data_for_packet_service(const QString & iS, std::vector<PacketData> & oPDVec) const
{
  QMutexLocker lock(&mMutex);

  const auto itService = find_service_from_service_name(iS);

  if (itService == ensemble->servicesMap.end())
  {
    return;
  }


  i16 numComp = 0;

  for (i16 compIdx = 0; compIdx < 16; ++compIdx)
  {
    if (numComp > 0 && compIdx >= numComp)
    {
      break;  // quit for loop
    }

    const auto * const pFig0s2 = currentConfig->get_Fig0s2_BasicService_ServiceComponentDefinition_of_SId_ScIdx(itService->second.SId, compIdx);

    if (pFig0s2 == nullptr || pFig0s2->ServiceComp_C.TMId != ETMId::PacketModeData)
    {
      continue;
    }

    if (numComp > 0 && numComp != pFig0s2->NumServiceComp)
    {
      qCritical() << "numComp mismatch";
      continue;
    }

    numComp = pFig0s2->NumServiceComp;

    if (numComp <= 0)
    {
      qCritical() << "numComp <= 0";
      continue;
    }

    auto * const pFig0s3 = currentConfig->get_Fig0s3_ServiceComponentPacketMode_of_SCId(pFig0s2->ServiceComp_C.TMId11.SCId);

    if (pFig0s3 == nullptr)
    {
      qWarning() << "SCId" << pFig0s2->ServiceComp_C.TMId11.SCId << "in FIG 0/3 not found";
      continue;
    }

    const i8 subChId = pFig0s3->SubChId;

    const auto * const pFig0s1 = currentConfig->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(subChId);

    if (pFig0s1 == nullptr)
    {
      qWarning() << "SubChId" << subChId << "in FIG 0/1 not found";
      continue;
    }

    const u32 SId = itService->second.SId;

    const auto * const pFig0s8 = currentConfig->get_Fig0s8_ServiceComponentGlobalDefinition_of_SId(SId); // TODO: only for long form valid (with SCId)?

    if (pFig0s8 == nullptr)
    {
      qWarning() << "SId" << SId << "in FIG 0/8 not found";
      continue;
    }

    const auto SCIdS = pFig0s8->SCIdS;

    const auto pFig0s13 = currentConfig->get_Fig0s13_UserApplicationInformation_of_SId_SCIdS(SId, SCIdS);

    if (pFig0s13 == nullptr || pFig0s13->NumUserApps < 1)
    {
      qWarning() << "SId" << SId << "and SCIdS" << SCIdS << "in FIG 0/13 not found";
      continue;
    }

    // const auto numUserApps = pFig0s13->NumUserApps;

    // const auto pScd = find_service_component(currentConfig.get(), SId, iCompIdx);
    //
    // if (pScd == nullptr || pScd->TMid != ETMId::PacketModeData)
    // {
    //   return;
    // }

    // const i32 subChId = pScd->subChannelId;

    // const auto pFig0s1 = currentConfig->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(subChId);
    const auto itFig0_14 = currentConfig->subChDescMapFig0_14.find(subChId);

    if (itFig0_14 == currentConfig->subChDescMapFig0_14.end())
    {
      qDebug() << "SubChId" << subChId << "in FIG 0/14 not found -> no FEC scheme for packet data";
    }

    PacketData pd;

    pd.serviceName = iS;
    pd.SId = SId;
    pd.SCIdS = compIdx; // TODO: is this correct?
    pd.subchId = subChId;
    pd.startAddr = pFig0s1->StartAddr;
    pd.shortForm = pFig0s1->ShortForm;
    pd.protLevel = pFig0s1->ProtectionLevel;
    pd.length = pFig0s1->SubChannelSize;
    pd.bitRate = pFig0s1->BitRate;
    pd.FEC_scheme = itFig0_14 != currentConfig->subChDescMapFig0_14.end() ? itFig0_14->second.FEC_scheme : 0;
    pd.DSCTy = pFig0s3->DSCTy;
    pd.DGflag = pFig0s3->DG_Flag;
    pd.packetAddress = pFig0s3->PacketAddress;
    // pd.compnr = pFig0s3->Com;
    pd.appType = pFig0s13->UserAppVec[0].UserAppType;  // TODO: only the first UserAppType?
    pd.defined = true; // TODO: obsolete

    oPDVec.emplace_back(pd);
  }  // for (i16 compIdx ...

  qDebug() << oPDVec.size() << "packets found";
  for (const auto & pd : oPDVec)
  {
    qDebug() << "ServiceName" << pd.serviceName.trimmed() << "SId" << pd.SId << "SCIdS" << pd.SCIdS << "SubChId" << pd.subchId << "CUStartAddr" << pd.startAddr
             << "ShortForm" << pd.shortForm << "ProtLevel" << pd.protLevel << "CUSize" << pd.length << "BitRate" << pd.bitRate << "FEC_scheme" << pd.FEC_scheme
             << "DSCTy" << pd.DSCTy << "DGflag" << pd.DGflag << "PacketAddress" << pd.packetAddress << "AppType" << pd.appType;
  }
}

std::vector<SServiceId> FibDecoder::get_services() const
{
  std::vector<SServiceId> services;

  for (const auto & [_, serviceEntry] : ensemble->servicesMap)
  {
    if (serviceEntry.hasName)
    {
      SServiceId ed;
      ed.name = serviceEntry.serviceLabel;
      ed.SId = serviceEntry.SId;

      services = insert_sorted(services, ed);
    }
  }

  return services;
}

std::vector<SServiceId> FibDecoder::insert_sorted(const std::vector<SServiceId> & iServiceIdList, const SServiceId & iServiceId) const
{
  std::vector<SServiceId> k;

  if (iServiceIdList.empty())
  {
    k.push_back(iServiceId);
    return k;
  }

  QString baseS = "";
  bool inserted = false;

  for (const auto & serv: iServiceIdList)
  {
    if (!inserted && baseS < iServiceId.name && iServiceId.name < serv.name)
    {
      k.push_back(iServiceId);
      inserted = true;
    }

    baseS = serv.name;
    k.push_back(serv);
  }

  if (!inserted)
  {
    k.push_back(iServiceId);
  }

  return k;
}

QString FibDecoder::find_service(u32 iSId, i32 iSCIdS) const
{
  // TODO: can there more than one SId with different iSCIdS?
  const auto it = ensemble->servicesMap.find(iSId);

  if (it != ensemble->servicesMap.end())
  {
    return "";
  }

  if (it->second.SId != iSId) qCritical() << "SId mismatch";

  if (it->second.SCIdS == iSCIdS)
  {
    return it->second.serviceLabel;
  }
  else
  {
    qDebug() << "SCIdS mismatch. Expect" << iSCIdS << "got" << it->second.SCIdS;
  }

  return "";
}

void FibDecoder::get_parameters(const QString & iServiceName, u32 & oSId, i32 & oSCIdS) const
{
  const auto itService = find_service_from_service_name(iServiceName);

  if (itService == ensemble->servicesMap.end())
  {
    oSId = 0;
    oSCIdS = 0;
  }
  else
  {
    oSId = itService->second.SId;
    oSCIdS = itService->second.SCIdS;
  }
}

i32 FibDecoder::get_ensembleId() const
{
  if (ensemble->namePresent)
  {
    return ensemble->ensembleId;
  }
  else
  {
    return 0;
  }
}

QString FibDecoder::get_ensemble_name() const
{
  if (ensemble->namePresent)
  {
    return ensemble->ensembleName;
  }
  else
  {
    return " ";
  }
}

i32 FibDecoder::get_cif_count() const
{
  return CIFcount;
}

void FibDecoder::get_cif_count(i16 * h, i16 * l) const
{
  *h = CIFcount_hi;
  *l = CIFcount_lo;
}

u8 FibDecoder::get_ecc() const
{
  if (ensemble->ecc_Present)
  {
    return ensemble->ecc_byte;
  }
  return 0;
}

u16 FibDecoder::get_country_name() const
{
  return (get_ecc() << 8) | get_countryId();
}

u8 FibDecoder::get_countryId() const
{
  return ensemble->countryId;
}


/////////////////////////////////////////////////////////////////////////////
//
//	Country, LTO & international table 8.1.3.2
void FibDecoder::FIG0Extension9(const u8 * const d)
{
  const i16 offset = 16;

  // 6 indicates the number of hours
  i32 signbit = getBits_1(d, offset + 2);
  dateTime[6] = (signbit == 1) ? -1 * getBits_4(d, offset + 3) : getBits_4(d, offset + 3);

  // 7 indicates a possible remaining half hour
  dateTime[7] = (getBits_1(d, offset + 7) == 1) ? 30 : 0;

  if (signbit == 1)
  {
    dateTime[7] = -dateTime[7];
  }

  const u8 ecc = getBits(d, offset + 8, 8);

  if (!ensemble->ecc_Present)
  {
    ensemble->ecc_byte = ecc;
    ensemble->ecc_Present = true;
  }
}

//static
//QString monthTable [] = {
//"jan", "feb", "mar", "apr", "may", "jun",
//"jul", "aug", "sep", "oct", "nov", "dec"};

i32 monthLength[]{
  31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

//
//	Time in 10 is given in UTC, for other time zones
//	we add (or subtract) a number of Hours (half hours)
void adjustTime(i32 * ioDateTime)
{
  //	first adjust the half hour  in the amount of minutes
  ioDateTime[4] += (ioDateTime[7] == 1) ? 30 : 0;
  if (ioDateTime[4] >= 60)
  {
    ioDateTime[4] -= 60;
    ioDateTime[3]++;
  }

  if (ioDateTime[4] < 0)
  {
    ioDateTime[4] += 60;
    ioDateTime[3]--;
  }

  ioDateTime[3] += ioDateTime[6];
  if ((0 <= ioDateTime[3]) && (ioDateTime[3] <= 23))
  {
    return;
  }

  if (ioDateTime[3] > 23)
  {
    ioDateTime[3] -= 24;
    ioDateTime[2]++;
  }

  if (ioDateTime[3] < 0)
  {
    ioDateTime[3] += 24;
    ioDateTime[2]--;
  }

  if (ioDateTime[2] > monthLength[ioDateTime[1] - 1])
  {
    ioDateTime[2] = 1;
    ioDateTime[1]++;
    if (ioDateTime[1] > 12)
    {
      ioDateTime[1] = 1;
      ioDateTime[0]++;
    }
  }

  if (ioDateTime[2] < 0)
  {
    if (ioDateTime[1] > 1)
    {
      ioDateTime[2] = monthLength[ioDateTime[1] - 1 - 1];
      ioDateTime[1]--;
    }
    else
    {
      ioDateTime[2] = monthLength[11];
      ioDateTime[1] = 12;
      ioDateTime[0]--;
    }
  }
}

void FibDecoder::FIG0Extension10(const u8 * dd)
{
  i16 offset = 16;
  this->mjd = getLBits(dd, offset + 1, 17);

  //	Modified Julian Date (recompute according to wikipedia)
  const i32 J = mjd + 2400001;
  const i32 j = J + 32044;
  const i32 g = j / 146097;
  const i32 dg = j % 146097;
  const i32 c = ((dg / 36524) + 1) * 3 / 4;
  const i32 dc = dg - c * 36524;
  const i32 b = dc / 1461;
  const i32 db = dc % 1461;
  const i32 a = ((db / 365) + 1) * 3 / 4;
  const i32 da = db - a * 365;
  const i32 y = g * 400 + c * 100 + b * 4 + a;
  const i32 m = ((da * 5 + 308) / 153) - 2;
  const i32 d = da - ((m + 4) * 153 / 5) + 122;
  const i32 Y = y - 4800 + ((m + 2) / 12);
  const i32 M = ((m + 2) % 12) + 1;
  const i32 D = d + 1;

  i32 theTime[6];
  theTime[0] = Y;  // Year
  theTime[1] = M;  // Month
  theTime[2] = D;  // Day
  theTime[3] = getBits_5(dd, offset + 21); // Hours
  theTime[4] = getBits_6(dd, offset + 26); // Minutes

  if (getBits_6(dd, offset + 26) != dateTime[4])
  {
    theTime[5] = 0;
  }  // Seconds (Uebergang abfangen)

  if (dd[offset + 20] == 1)
  {
    theTime[5] = getBits_6(dd, offset + 32);
  }  // Seconds

  //	take care of different time zones
  bool change = false;

  for (i32 i = 0; i < 6; i++)
  {
    if (theTime[i] != dateTime[i])
    {
      change = true;
    }
    dateTime[i] = theTime[i];
  }

  if (change)
  {
    i32 utc_day = dateTime[2];
    i32 utc_hour = dateTime[3];
    i32 utc_minute = dateTime[4];
    i32 utc_seconds = dateTime[5];
    adjustTime(dateTime.data());
    emit signal_clock_time(dateTime[0], dateTime[1], dateTime[2], dateTime[3], dateTime[4], utc_day, utc_hour, utc_minute, utc_seconds);
  }
}

void FibDecoder::set_epg_data(u32 iSId, i32 iTheTime, const QString & iTheText, const QString & iTheDescr) const
{
  const auto it = ensemble->servicesMap.find(iSId);

  if (it == ensemble->servicesMap.end())
  {
    return;
  }

  Service * S = &(it->second);
  for (u16 j = 0; j < S->epgData.size(); j++)
  {
    if (S->epgData.at(j).theTime == iTheTime)
    {
      S->epgData.at(j).theText = iTheText;
      S->epgData.at(j).theDescr = iTheDescr;
      return;
    }
  }

  SEpgElement ep;
  ep.theTime = iTheTime;
  ep.theText = iTheText;
  ep.theDescr = iTheDescr;
  S->epgData.push_back(ep);
}

std::vector<SEpgElement> FibDecoder::get_timeTable(u32 SId) const
{
  std::vector<SEpgElement> res;

  const auto it = ensemble->servicesMap.find(SId);

  if (it == ensemble->servicesMap.end())
  {
    return res;
  }

  return it->second.epgData;
}

std::vector<SEpgElement> FibDecoder::get_timeTable(const QString & service) const
{
  std::vector<SEpgElement> res;

  const auto it = find_service_from_service_name(service);

  if (it == ensemble->servicesMap.end())
  {
    return res;
  }

  return it->second.epgData;
}

bool FibDecoder::has_time_table(u32 SId) const
{
  const auto it = ensemble->servicesMap.find(SId);

  if (it == ensemble->servicesMap.end())
  {
    return false;
  }

  return it->second.epgData.size() > 2;
}

std::vector<SEpgElement> FibDecoder::find_epg_data(u32 SId) const
{
  const auto it = ensemble->servicesMap.find(SId);

  std::vector<SEpgElement> res;

  if (it == ensemble->servicesMap.end())
  {
    return res;
  }

  res = it->second.epgData;
  return res;
}

u32 FibDecoder::get_julian_date() const
{
  return mjd;
}

void FibDecoder::get_channel_info(ChannelData * d, i32 iSubChId) const
{
  static constexpr FibConfig::SFig0s1_BasicSubChannelOrganization subChannel{};

  const auto * const pFig0s1 = currentConfig->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(iSubChId);

  d->in_use = pFig0s1 != nullptr;

  const auto & scd = d->in_use ? *pFig0s1 : subChannel;

  d->id = scd.SubChId;
  d->start_cu = scd.StartAddr;
  d->protlev = scd.ProtectionLevel;
  d->size = scd.SubChannelSize;
  d->bitrate = scd.BitRate;
  d->uepFlag = scd.ShortForm;
}

bool FibDecoder::extract_character_set_label(QString & oName, const u8 * const d, const i16 iLabelOffs)
{
  const ECharacterSet charSet = (ECharacterSet)getBits_4(d, 8);

  if (!is_charset_valid(charSet))
  {
    qCritical() << "invalid character set" << (int)charSet;
    return false;
  }

  char label[17];
  label[16] = 0;

  for (i32 i = 0; i < 16; i++)
  {
    label[i] = getBits_8(d, iLabelOffs + 8 * i);
  }

  oName = to_QString_using_charset(label, charSet);

  for (i32 i = (i32)oName.length(); i < 16; i++)
  {
    oName.append(' ');
  }

  return true;
}

void FibDecoder::retrigger_timer_data_loaded()
{
  // mpTimerDataLoaded->start() must be called that way as it is a different thread without event-loop which is calling
  QMetaObject::invokeMethod(mpTimerDataLoaded, "start", Qt::QueuedConnection);
}

void FibDecoder::slot_timer_data_loaded()
{
  QMutexLocker lock(&mMutex);
  currentConfig->print_Fig0s1_BasicSubChannelOrganization();
  currentConfig->print_Fig0s2_BasicService_ServiceComponentDefinition();
  currentConfig->print_Fig0s3_ServiceComponentPacketMode();
  currentConfig->print_Fig0s8_ServiceComponentGlobalDefinition();
  currentConfig->print_Fig0s13_UserApplicationInformation();
  currentConfig->print_Fig0s14_SubChannelOrganization();
  // currentConfig->print_SIdTable_of_SId();
}
