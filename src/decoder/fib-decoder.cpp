//
// Created by tomneda on 2025-08-23.
//
#include "fib-decoder.h"
#include "dabradio.h"
#include "charsets.h"
#include "bit-extractors.h"
#include <cstring>
#include <vector>

/*static*/ std::unique_ptr<IFibDecoder> FibDecoderFactory::create(DabRadio* radio)
{
  return std::make_unique<FibDecoder>(radio);
}

FibDecoder::FibDecoder(DabRadio * mr)
  : mpRadioInterface(mr)
  , mpFibConfigFig1(std::make_unique<FibConfigFig1>())
  , mpFibConfigFig0Curr (std::make_unique<FibConfigFig0>())
  , mpFibConfigFig0Next(std::make_unique<FibConfigFig0>())
{
  connect(this, &IFibDecoder::signal_name_of_ensemble, mpRadioInterface, &DabRadio::slot_name_of_ensemble);
  connect(this, &IFibDecoder::signal_fib_time_info, mpRadioInterface, &DabRadio::slot_fib_time);
  connect(this, &IFibDecoder::signal_change_in_configuration, mpRadioInterface, &DabRadio::slot_change_in_configuration);
  connect(this, &IFibDecoder::signal_start_announcement, mpRadioInterface, &DabRadio::slot_start_announcement);
  connect(this, &IFibDecoder::signal_stop_announcement, mpRadioInterface, &DabRadio::slot_stop_announcement);
  connect(this, &IFibDecoder::signal_nr_services, mpRadioInterface, &DabRadio::slot_nr_services);

  mpTimerDataLoadedFast = new QTimer(this);
  mpTimerDataLoadedSlow = new QTimer(this);
  connect(mpTimerDataLoadedFast, &QTimer::timeout, this, &FibDecoder::_slot_timer_data_loaded_fast);
  connect(mpTimerDataLoadedSlow, &QTimer::timeout, this, &FibDecoder::_slot_timer_data_loaded_slow);
  mpTimerDataLoadedFast->setInterval(cMaxFibLoadingTimeFast_ms);
  mpTimerDataLoadedSlow->setInterval(cMaxFibLoadingTimeSlow_ms);
  mpTimerDataLoadedFast->setSingleShot(true);
  mpTimerDataLoadedSlow->setSingleShot(true);
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
    case 0: _process_Fig0(d); break;
    case 1: _process_Fig1(d); break;
    default:
      qDebug() << QString("Fig type %1 not handled").arg(FIGtype);
    }

    processedBytes += FIGlength + 1; // data length plus header length
  }

  if (processedBytes > 30)
  {
    qCritical() << "FIG package length error" << processedBytes;
  }
}

void FibDecoder::_reset()
{
  mpTimerDataLoadedFast->stop();
  mpTimerDataLoadedSlow->stop();
  mFibDataLoadedFast = false;
  mFibDataLoadedSlow = false;
  mDiffMaxFast = {};
  mDiffMaxSlow = {};
  mLastTimePointFast = {};
  mLastTimePointSlow = {};

  mCifCount = 0;
  mCifCount_hi = 0;
  mCifCount_lo = 0;
  mModJulianDate = 0;
  mPrevChangeFlag = 0;
  mUtcTimeSet = {};
  mUnhandledFig0Set.clear();
  mUnhandledFig1Set.clear();
}

void FibDecoder::connect_channel()
{
  QMutexLocker lock(&mMutex);
  mpFibConfigFig0Curr->reset();
  mpFibConfigFig0Next->reset();
  mpFibConfigFig1->reset();
  _reset();
}

void FibDecoder::disconnect_channel()
{
  QMutexLocker lock(&mMutex);
  mpFibConfigFig0Curr->reset();
  mpFibConfigFig0Next->reset();
  mpFibConfigFig1->reset();
}

void FibDecoder::get_data_for_audio_service(const QString & iServiceLabel, SAudioData * opAD) const
{
  QMutexLocker lock(&mMutex);

  opAD->isDefined = false;

  if (!mFibDataLoadedFast)
  {
    qWarning() << "Fast data not loaded";
    return;
  }

  const FibConfigFig1::SSId_SCIdS * const pSId_SCIdS = mpFibConfigFig1->get_SId_SCIdS_from_service_label(iServiceLabel);

  if (pSId_SCIdS == nullptr)
  {
    return;
  }

  const auto * const pFig0s2 = mpFibConfigFig0Curr->get_Fig0s2_BasicService_ServiceComponentDefinition_of_SId_TMId(pSId_SCIdS->SId, ETMId::StreamModeAudio);

  if (pFig0s2 == nullptr)
  {
    qWarning() << "SId" << pSId_SCIdS->SId << "for programme service label" << iServiceLabel.trimmed() << "in FIG 0/2 not found";
    return;
  }

  if (!_get_data_for_audio_service(*pFig0s2, opAD))
  {
    return;
  }
}

void FibDecoder::get_data_for_audio_service_addon(const QString & iServiceLabel, SAudioDataAddOns * opADAO) const
{
  QMutexLocker lock(&mMutex);

  if (!mFibDataLoadedSlow)
  {
    qWarning() << "Slow data not loaded";
    return;
  }

  const FibConfigFig1::SSId_SCIdS * const pSId_SCIdS = mpFibConfigFig1->get_SId_SCIdS_from_service_label(iServiceLabel);

  if (pSId_SCIdS == nullptr)
  {
    return;
  }

  const auto * const pFig0s2 = mpFibConfigFig0Curr->get_Fig0s2_BasicService_ServiceComponentDefinition_of_SId_TMId(pSId_SCIdS->SId, ETMId::StreamModeAudio);

  if (pFig0s2 == nullptr)
  {
    qWarning() << "SId" << pSId_SCIdS->SId << "for programme service label" << iServiceLabel.trimmed() << "in FIG 0/2 not found";
    return;
  }

  if (!_get_data_for_audio_service_addon(*pFig0s2, opADAO))
  {
    return;
  }
}

bool FibDecoder::_get_data_for_audio_service(const FibConfigFig0::SFig0s2_BasicService_ServiceComponentDefinition & iFig0s2, SAudioData * opAD) const
{
  opAD->isDefined = false;

  if (iFig0s2.ServiceComp_C.PS_Flag != 1) qWarning() << "This should be only a primary service";
  if (iFig0s2.PD_Flag != 0)
  {
    qCritical() << "Unexpected big SId here" ;
    return false;
  }
  assert(iFig0s2.ServiceComp_C.TMId == ETMId::StreamModeAudio); // only audio elements
  const u8 SubChId = iFig0s2.ServiceComp_C.TMId00.SubChId;
  const u16 SId = iFig0s2.get_SId();
  if ((SId & 0xFFFF0000) != 0) qWarning() << "Unexpected big ";

  const auto * const pFig0s1  = mpFibConfigFig0Curr->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(SubChId);
  const auto * const pFig1s1  = mpFibConfigFig1->get_Fig1s1_ProgrammeServiceLabel_of_SId(SId);

  if (pFig0s1 != nullptr && pFig1s1 != nullptr)
  {
    opAD->serviceName = pFig1s1->Name;
    opAD->serviceNameShort = pFig1s1->NameShort;
    opAD->SId = SId;
    opAD->SCIdS = 0;
    opAD->subchId = SubChId;
    opAD->startAddr = pFig0s1->StartAddr;
    opAD->shortForm = pFig0s1->ShortForm;
    opAD->protLevel = pFig0s1->ProtectionLevel;
    opAD->length = pFig0s1->SubChannelSize;
    opAD->bitRate = pFig0s1->BitRate;
    opAD->ASCTy = iFig0s2.ServiceComp_C.TMId00.ASCTy;
    opAD->isDefined = true;
  }

  return true;
}

bool FibDecoder::_get_data_for_audio_service_addon(const FibConfigFig0::SFig0s2_BasicService_ServiceComponentDefinition & iFig0s2, SAudioDataAddOns * opADAO) const
{
  if (iFig0s2.ServiceComp_C.PS_Flag != 1) qWarning() << "This should be only a primary service";
  if (iFig0s2.PD_Flag != 0)
  {
    qCritical() << "Unexpected big SId here" ;
    return false;
  }
  assert(iFig0s2.ServiceComp_C.TMId == ETMId::StreamModeAudio); // only audio elements
  const u8 SubChId = iFig0s2.ServiceComp_C.TMId00.SubChId;
  const u16 SId = iFig0s2.get_SId();
  if ((SId & 0xFFFF0000) != 0) qWarning() << "Unexpected big ";

  const auto * const pFig0s5  = mpFibConfigFig0Curr->get_Fig0s5_ServiceComponentLanguage_of_SubChId(SubChId);
  const auto * const pFig0s17 = mpFibConfigFig0Curr->get_Fig0s17_ProgrammeType_of_SId(SId);

  if (pFig0s5 != nullptr)  opADAO->language    = pFig0s5->Language; else opADAO->language    = std::nullopt;
  if (pFig0s17 != nullptr) opADAO->programType = pFig0s17->IntCode; else opADAO->programType = std::nullopt;
  // opADAO->fmFrequency = itService->second.fmFrequency; // TODO:

  return true;
}

void FibDecoder::get_data_for_packet_service(const QString & iServiceLabel, std::vector<SPacketData> & oPDVec) const
{
  QMutexLocker lock(&mMutex);

  if (!mFibDataLoadedSlow)
  {
    return;
  }

  const FibConfigFig1::SSId_SCIdS * const pSId_SCIdS = mpFibConfigFig1->get_SId_SCIdS_from_service_label(iServiceLabel);

  if (pSId_SCIdS == nullptr || pSId_SCIdS->SId == 0) // SId not found  TODO: assuming SId == 0 is not allowed
  {
    return;
  }

  i16 numComp = 0;

  for (i16 compIdx = 0; compIdx < FibConfigFig0::SFig0s2_BasicService_ServiceComponentDefinition::cNumServiceCompMax; ++compIdx)
  {
    if (numComp > 0 && compIdx >= numComp)
    {
      break;  // quit for loop
    }

    const auto * const pFig0s2 = mpFibConfigFig0Curr->get_Fig0s2_BasicService_ServiceComponentDefinition_of_SId_ScIdx(pSId_SCIdS->SId, compIdx);

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

    SPacketData pd;
    if (!_get_data_for_packet_service(*pFig0s2, compIdx, &pd))
    {
      continue; // TODO: or return?
    }

    oPDVec.emplace_back(pd);
  }  // for (i16 compIdx ...

  qDebug() << oPDVec.size() << "packets found";
  // for (const auto & pd : oPDVec)
  // {
  //   qDebug() << "ServiceName" << pd.serviceName.trimmed() << "SId" << pd.SId << "SCIdS" << pd.SCIdS << "SubChId" << pd.subchId << "CUStartAddr" << pd.startAddr
  //            << "ShortForm" << pd.shortForm << "ProtLevel" << pd.protLevel << "CUSize" << pd.length << "BitRate" << pd.bitRate << "FEC_scheme" << pd.FEC_scheme
  //            << "DSCTy" << pd.DSCTy << "DGflag" << pd.DGflag << "PacketAddress" << pd.packetAddress << "AppType" << pd.appTypeVec[0];
  // }
}

bool FibDecoder::_get_data_for_packet_service(const FibConfigFig0::SFig0s2_BasicService_ServiceComponentDefinition & iFig0s2, const i16 iCompIdx, SPacketData * opPD) const
{
  opPD->isDefined = false;

  if (iCompIdx >= FibConfigFig0::SFig0s2_BasicService_ServiceComponentDefinition::cNumServiceCompMax)
  {
    return false;
  }

  assert(iFig0s2.ServiceComp_C.TMId == ETMId::PacketModeData);

  const auto * const pFig0s3 = mpFibConfigFig0Curr->get_Fig0s3_ServiceComponentPacketMode_of_SCId(iFig0s2.ServiceComp_C.TMId11.SCId);

  if (pFig0s3 == nullptr)
  {
    qWarning() << "SCId" << iFig0s2.ServiceComp_C.TMId11.SCId << "in FIG 0/3 not found";
    return false;
  }

  const i8 subChId = pFig0s3->SubChId;
  const auto * const pFig0s1 = mpFibConfigFig0Curr->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(subChId);

  if (pFig0s1 == nullptr)
  {
    qWarning() << "SubChId" << subChId << "in FIG 0/1 not found";
    return false;
  }

  const u32 SId = iFig0s2.get_SId();
  const auto * const pFig0s8 = mpFibConfigFig0Curr->get_Fig0s8_ServiceComponentGlobalDefinition_of_SId(SId); // TODO: only for long form valid (with SCId)?

  if (pFig0s8 == nullptr)
  {
    qWarning() << "SId" << SId << "in FIG 0/8 not found";
    return false;
  }

  const auto SCIdS = pFig0s8->SCIdS;
  const auto pFig0s13 = mpFibConfigFig0Curr->get_Fig0s13_UserApplicationInformation_of_SId_SCIdS(SId, SCIdS);

  if (pFig0s13 == nullptr || pFig0s13->NumUserApps < 1)
  {
    qWarning() << "SId" << SId << "and SCIdS" << SCIdS << "in FIG 0/13 not found";
    return false;
  }

  const auto * const pFig0s14 = mpFibConfigFig0Curr->get_Fig0s14_SubChannelOrganization_of_SubChId(subChId);

  if (pFig0s14 == nullptr)
  {
    // qDebug() << "SubChId" << subChId << "in FIG 0/14 not found -> no FEC scheme for packet data";
  }

  const auto * const pFig1s4 = mpFibConfigFig1->get_Fig1s4_ServiceComponentLabel_of_SId_SCIdS(SId, pFig0s8->SCIdS);
  const auto * const pFig1s5 = mpFibConfigFig1->get_Fig1s5_DataServiceLabel_of_SId(SId);

  if (pFig1s5 == nullptr)
  {
    if (pFig1s4 == nullptr)
    {
      return false;
    }
    opPD->serviceName = pFig1s4->Name;
    opPD->serviceNameShort = pFig1s4->NameShort;
  }
  else
  {
    opPD->serviceName = pFig1s5->Name;
    opPD->serviceNameShort = pFig1s5->NameShort;
  }

  opPD->SId = SId;
  opPD->SCIdS = iCompIdx; // TODO: is this correct?
  opPD->subchId = subChId;
  opPD->startAddr = pFig0s1->StartAddr;
  opPD->shortForm = pFig0s1->ShortForm;
  opPD->protLevel = pFig0s1->ProtectionLevel;
  opPD->length = pFig0s1->SubChannelSize;
  opPD->bitRate = pFig0s1->BitRate;
  opPD->FEC_scheme = pFig0s14 != nullptr ? pFig0s14->FEC_scheme : 0;
  opPD->DSCTy = pFig0s3->DSCTy;
  opPD->DGflag = pFig0s3->DG_Flag;
  opPD->packetAddress = pFig0s3->PacketAddress;
  const i16 numUserApps = std::min(pFig0s13->NumUserApps, (i16)pFig0s13->UserAppVec.size());
  opPD->appTypeVec.resize(numUserApps);
  for (i16 i = 0; i < numUserApps; ++i)
  {
    opPD->appTypeVec[i] = pFig0s13->UserAppVec[i].UserAppType;
  }
  if (opPD->appTypeVec.empty()) opPD->appTypeVec.emplace_back(-1); // ensure at least one element
  opPD->isDefined = true; // TODO: obsolete

  return true;
}

std::vector<SServiceId> FibDecoder::get_service_list() const
{
  QMutexLocker lock(&mMutex);
  std::vector<SServiceId> services;

  // if (_are_fib_data_loaded())
  // {
    services.reserve(mpFibConfigFig1->serviceLabel_To_SId_SCIdS_Map.size());
    for (const auto & [serviceLabel, SId_SCIdS] : mpFibConfigFig1->serviceLabel_To_SId_SCIdS_Map)
    {
      if (SId_SCIdS.SCIdS <= 0) // avoid using listing secondary subservices (negative SCIdS means "not specified")
      {
        const auto * pFig0s2 = mpFibConfigFig0Curr->get_Fig0s2_BasicService_ServiceComponentDefinition_of_SId_TMId(SId_SCIdS.SId, ETMId::StreamModeAudio);
        SServiceId ed;
        ed.isAudioChannel = pFig0s2 != nullptr; // else is a primary data service
        ed.hasSpiEpgData = false; // TODO: fill out
        ed.name = serviceLabel;
        ed.SId = SId_SCIdS.SId;
        services.emplace_back(ed);
      }
    }
  // }
  // else
  // {
  //   qWarning() << "Fib data not loaded yet";
  // }

  return services;
}

const QString & FibDecoder::get_service_label_from_SId_SCIdS(const u32 iSId, const i32 iSCIdS) const
{
  QMutexLocker lock(&mMutex);
  static const QString emptyString;

  const auto * const pFig1s4 = mpFibConfigFig1->get_Fig1s4_ServiceComponentLabel_of_SId_SCIdS(iSId, iSCIdS);

  if (pFig1s4 == nullptr)
  {
    qWarning() << "SId" << iSId << "and SCIdS" << iSCIdS << "in FIG 1/4 not found";
    return emptyString;
  }

  return pFig1s4->Name;
}

void FibDecoder::get_SId_SCIdS_from_service_label(const QString & iServiceLabel, u32 & oSId, i32 & oSCIdS) const
{
  QMutexLocker lock(&mMutex);
  const FibConfigFig1::SSId_SCIdS * pSId_SCIdS = mpFibConfigFig1->get_SId_SCIdS_from_service_label(iServiceLabel);

  if (pSId_SCIdS == nullptr || pSId_SCIdS->SId == 0)
  {
    oSId = 0;
    oSCIdS = 0;
  }
  else
  {
    oSId = pSId_SCIdS->SId;
    oSCIdS = pSId_SCIdS->SCIdS;  // TODO: SCIdS may be negative, check if this works
  }
}

i32 FibDecoder::get_ensembleId() const
{
  QMutexLocker lock(&mMutex);
  if (!mpFibConfigFig1->Fig1s0_EnsembleLabelVec.empty())
  {
    return mpFibConfigFig1->Fig1s0_EnsembleLabelVec[0].EId;
  }

  return 0;
}

QString FibDecoder::get_ensemble_name() const
{
  QMutexLocker lock(&mMutex);
  if (!mpFibConfigFig1->Fig1s0_EnsembleLabelVec.empty())
  {
    return mpFibConfigFig1->Fig1s0_EnsembleLabelVec[0].Name;
  }

  return " ";
}

i32 FibDecoder::get_cif_count() const
{
  QMutexLocker lock(&mMutex);
  return mCifCount;
}

void FibDecoder::get_cif_count(i16 * h, i16 * l) const
{
  QMutexLocker lock(&mMutex);
  *h = mCifCount_hi;
  *l = mCifCount_lo;
}

u8 FibDecoder::get_ecc() const
{
  QMutexLocker lock(&mMutex);
  if (const auto * const pFig0s9 = mpFibConfigFig0Curr->get_Fig0s9_CountryLtoInterTab();
      pFig0s9 != nullptr)
  {
    return pFig0s9->Ensemble_ECC;
  }
  return 0;
}

#if 0
void FibDecoder::set_epg_data(u32 iSId, i32 iTheTime, const QString & iTheText, const QString & iTheDescr) const
{
  const auto it = mpFibConfigFig1->servicesMap.find(iSId);

  if (it == mpFibConfigFig1->servicesMap.end())
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

  const auto it = mpFibConfigFig1->servicesMap.find(SId);

  if (it == mpFibConfigFig1->servicesMap.end())
  {
    return res;
  }

  return it->second.epgData;
}

std::vector<SEpgElement> FibDecoder::get_timeTable(const QString & service) const
{
  std::vector<SEpgElement> res;

  const auto it = find_service_from_service_name(service);

  if (it == mpFibConfigFig1->servicesMap.end())
  {
    return res;
  }

  return it->second.epgData;
}

bool FibDecoder::has_time_table(u32 SId) const
{
  const auto it = mpFibConfigFig1->servicesMap.find(SId);

  if (it == mpFibConfigFig1->servicesMap.end())
  {
    return false;
  }

  return it->second.epgData.size() > 2;
}

std::vector<SEpgElement> FibDecoder::find_epg_data(u32 SId) const
{
  const auto it = mpFibConfigFig1->servicesMap.find(SId);

  std::vector<SEpgElement> res;

  if (it == mpFibConfigFig1->servicesMap.end())
  {
    return res;
  }

  res = it->second.epgData;
  return res;
}
#endif

u32 FibDecoder::get_mod_julian_date() const
{
  QMutexLocker lock(&mMutex);
  return mModJulianDate;
}

void FibDecoder::get_channel_info(SChannelData * d, const i32 iSubChId) const
{
  QMutexLocker lock(&mMutex);

  static constexpr FibConfigFig0::SFig0s1_BasicSubChannelOrganization emptySubChannel{};

  const auto * const pFig0s1 = mpFibConfigFig0Curr->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(iSubChId);

  d->in_use = pFig0s1 != nullptr;

  const auto & scd = d->in_use ? *pFig0s1 : emptySubChannel;

  d->id = scd.SubChId;
  d->start_cu = scd.StartAddr;
  d->protlev = scd.ProtectionLevel;
  d->size = scd.SubChannelSize;
  d->bitrate = scd.BitRate;
  d->uepFlag = scd.ShortForm;
}

bool FibDecoder::_are_fib_data_loaded() const
{
  return mFibDataLoadedFast && mFibDataLoadedSlow && !mpFibConfigFig1->Fig1s0_EnsembleLabelVec.empty();
}

void FibDecoder::_set_cluster(FibConfigFig0 * localBase, i32 clusterId, u32 iSId, u16 asuFlags)
{
  if (!_are_fib_data_loaded())
  {
    return;
  }

  Cluster * myCluster = _get_cluster(localBase, clusterId);

  if (myCluster == nullptr)
  {
    return;
  }

  if (myCluster->flags != asuFlags)
  {
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

Cluster * FibDecoder::_get_cluster(FibConfigFig0 * localBase, i16 clusterId)
{
  for (i32 i = 0; i < 64; i++)
  {
    if ((localBase->mClusterTable[i].inUse) && (localBase->mClusterTable[i].clusterId == clusterId))
    {
      return &(localBase->mClusterTable[i]);
    }
  }

  for (i32 i = 0; i < 64; i++)
  {
    if (!localBase->mClusterTable[i].inUse)
    {
      localBase->mClusterTable[i].inUse = true;
      localBase->mClusterTable[i].clusterId = clusterId;
      return &(localBase->mClusterTable[i]);
    }
  }
  return &(localBase->mClusterTable[0]);  // cannot happen
}

bool FibDecoder::_extract_character_set_label(FibConfigFig1::SFig1_DataField & oFig1DF, const u8 * const d, const i16 iLabelOffs) const
{
  oFig1DF.Charset = getBits_4(d, 8);

  if (!is_charset_valid((ECharacterSet)oFig1DF.Charset))
  {
    qCritical() << "invalid character set" << (int)oFig1DF.Charset;
    return false;
  }

  char label[17];
  label[16] = 0;

  for (i32 i = 0; i < 16; i++)
  {
    label[i] = getBits_8(d, iLabelOffs + 8 * i);
  }

  oFig1DF.CharFlagField = getBits(d, iLabelOffs + 16 * 8, 16);
  oFig1DF.Name = to_QString_using_charset(label, (ECharacterSet)oFig1DF.Charset);
  oFig1DF.Name.resize(16, ' '); // fill up to 16 spaces

  // retrieve short name
  oFig1DF.NameShort.reserve(8);
  for (u16 bitIdx = 0; bitIdx < 16; ++bitIdx)
  {
    if (oFig1DF.CharFlagField & (1 << (15 - bitIdx)))
    {
      oFig1DF.NameShort.push_back(oFig1DF.Name[bitIdx]);
    }
  }
  oFig1DF.NameShort.resize(8, ' '); // fill up to 8 spaces

  return true;
}

void FibDecoder::_retrigger_timer_data_loaded_fast(const char * const iCallerName)
{
  // evaluate maximum time difference between calls to check whether the empiric time cMaxFibLoadingTimeFast_ms is high enough
  std::chrono::time_point currTimePoint = std::chrono::high_resolution_clock::now();
  const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(currTimePoint - mLastTimePointFast);
  constexpr std::chrono::time_point<std::chrono::system_clock> cBaseVal{};
  if (mLastTimePointFast > cBaseVal && diff > mDiffMaxFast) mDiffMaxFast = diff;
  mLastTimePointFast = currTimePoint;
  // qInfo() << "Fast caller" << iCallerName << "Time elapsed [ms]" << diff.count() << "Diff Max" << mDiffMaxFast.count();

  if (mFibDataLoadedFast) // mFibDataLoaded gets true if the timer runs out the time cMaxFibLoadingTime_ms
  {
    qWarning() << "Fast FIB data collection were already finished for" << iCallerName << ", Max needed time [ms]" << mDiffMaxFast.count();
    return;
  }

  // mpTimerDataLoaded->start() must be called that way as it is a different thread without event-loop which is calling
  QMetaObject::invokeMethod(mpTimerDataLoadedFast, "start", Qt::QueuedConnection);
  QMetaObject::invokeMethod(mpTimerDataLoadedSlow, "start", Qt::QueuedConnection); // retrigger also slow timer as it must always come at last
}

void FibDecoder::_retrigger_timer_data_loaded_slow(const char * const iCallerName)
{
  // evaluate maximum time difference between calls to check whether the empiric time cMaxFibLoadingTimeSlow_ms is high enough
  std::chrono::time_point currTimePoint = std::chrono::high_resolution_clock::now();
  const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(currTimePoint - mLastTimePointSlow);
  constexpr std::chrono::time_point<std::chrono::system_clock> cBaseVal{};
  if (mLastTimePointSlow > cBaseVal && diff > mDiffMaxSlow) mDiffMaxSlow = diff;
  mLastTimePointSlow = currTimePoint;
  // qInfo() << "Slow caller" << iCallerName << "Time elapsed [ms]" << diff.count() << "Diff Max" << mDiffMaxSlow.count();

  if (mFibDataLoadedSlow) // mFibDataLoaded gets true if the timer runs out the time cMaxFibLoadingTime_ms
  {
    qWarning() << "Slow FIB data collection were already finished for" << iCallerName << ", Max needed time [ms]" << mDiffMaxSlow.count();
    return;
  }

  // mpTimerDataLoaded->start() must be called that way as it is a different thread without event-loop which is calling
  QMetaObject::invokeMethod(mpTimerDataLoadedSlow, "start", Qt::QueuedConnection);
}

void FibDecoder::_slot_timer_data_loaded_fast()
{
  QMutexLocker lock(&mMutex);
  mFibDataLoadedFast = true;
  emit signal_fib_data_loaded(false);
}

void FibDecoder::_slot_timer_data_loaded_slow()
{
  QMutexLocker lock(&mMutex);

#ifndef NDEBUG
  mpFibConfigFig0Curr->print_Fig0s1_BasicSubChannelOrganization();
  mpFibConfigFig0Curr->print_Fig0s2_BasicService_ServiceComponentDefinition();
  mpFibConfigFig0Curr->print_Fig0s3_ServiceComponentPacketMode();
  mpFibConfigFig0Curr->print_Fig0s5_ServiceComponentLanguage();
  mpFibConfigFig0Curr->print_Fig0s8_ServiceComponentGlobalDefinition();
  mpFibConfigFig0Curr->print_Fig0s9_CountryLtoInterTab();
  mpFibConfigFig0Curr->print_Fig0s13_UserApplicationInformation();
  mpFibConfigFig0Curr->print_Fig0s14_SubChannelOrganization();
  mpFibConfigFig0Curr->print_Fig0s17_ProgrammeType();
  mpFibConfigFig1->print_Fig1s0_EnsembleLabel();
  mpFibConfigFig1->print_Fig1s1_ProgrammeServiceLabelVec();
  mpFibConfigFig1->print_Fig1s4_ServiceComponentLabel();
  mpFibConfigFig1->print_Fig1s5_DataServiceLabel();

  // show not handled FIGs
  for (const auto & extension: mUnhandledFig0Set)
  {
    qDebug().noquote() << QString("FIG 0/%1 not handled").arg(extension);
  }

  for (const auto & extension: mUnhandledFig1Set)
  {
    qDebug().noquote() << QString("FIG 1/%1 not handled").arg(extension);
  }
#endif

  if (!mFibDataLoadedFast)
  {
    qCritical() << "Fast FIB data must be ready before this slow FIB is signaled";
  }

  mFibDataLoadedSlow = true;
  emit signal_fib_data_loaded(true);
}

FibDecoder::SFigHeader FibDecoder::_get_fig_header(const u8 * const d) const
{
  SFigHeader header;
  header.Length  = getBits_5(d,  3); // FIG header length in bytes

  header.CN_Flag = getBits_1(d,  8); // (Current/Next) this 1-bit flag shall indicate one of two situations, according to the Extension, as follows:
                                     //     a) MCI - the type 0 field applies to the current or the next version of the multiplex configuration, as follows:
                                     //          0: current configuration;  1: next configuration.
                                     //     b) SIV - the type 0 field carries information for a database.
                                     // used in FIG type 0 extensions: 1, 2, 3, 4, (6), 7, 8, 13, 14, (21), (24)   ()==SIV, else MCI

  header.OE_Flag = getBits_1(d,  9); // (Other Ensemble) this 1-bit flag shall indicate, according to the Extension, whether the information is related to this or another ensemble, as follows:
                                     //     0: this ensemble; 1: other ensemble (or FM or AM or DRM service).
                                     // used in FIG type 0 extensions: 21

  header.PD_Flag = getBits_1(d, 10); // (Programme/Data) this 1-bit flag shall indicate, according to the Extension, whether the Service Identifiers (SIds) are in the 16-bit or 32-bit format, as follows:
                                     //     0: 16-bit SId, used for programme services;  1: 32-bit SId, used for data services.
                                     //     When the P/D flag is not used, the Service Identifier (SId) takes the 16-bit format.
                                     //     NOTE:  16-bit and 32-bit Service Identifiers may not be mixed in the same type 0 field.
                                     // used in FIG type 0 extensions: 2, 6, 8, 13, 20, 24
  return header; // should be "moved"
}
