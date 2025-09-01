//
// Created by tomneda on 2025-08-23.
//
#include "fib-decoder.h"
#include "bit-extractors.h"
#include "fib-table.h"
#include <QDebug>
#include <QTimer>


void FibDecoder::process_FIG0(const u8 * const d)
{
  const u8 extension = getBits_5(d, 8 + 3);  // skip C/N, OE and P/D flags

  switch (extension)
  {
  // MCI: Multiplex Configuration Information
  // SI:  Service Information
  case  0: FIG0Extension0(d);  break;  // MCI, ensemble information (6.4.1)
  case  1: FIG0Extension1(d);  break;  // MCI, sub-channel organization (6.2.1)
  case  2: FIG0Extension2(d);  break;  // MCI, service organization (6.3.1)
  case  3: FIG0Extension3(d);  break;  // MCI, service component in packet mode (6.3.2)
  case  4:                     break;  // MCI, service component with CA (6.3.3)
  case  5: FIG0Extension5(d);  break;  // SI,  service component language (8.1.2)
  case  6:                     break;  // SI,  service linking information (8.1.15)
  case  7: FIG0Extension7(d);  break;  // MCI, configuration information (6.4.2)
  case  8: FIG0Extension8(d);  break;  // MCI, service component global definition (6.3.5)
  case  9: FIG0Extension9(d);  break;  // SI,  country, LTO & international table (8.1.3.2)
  case 10: FIG0Extension10(d); break;  // SI,  date and time (8.1.3.1)
  case 13: FIG0Extension13(d); break;  // MCI, user application information (6.3.6)
  case 14: FIG0Extension14(d); break;  // MCI, FEC subchannel organization (6.2.2)
  case 17: FIG0Extension17(d); break;  // SI,  Program type (8.1.5)
  case 18: FIG0Extension18(d); break;  // SI,  announcement support (8.1.6.1)
  case 19: FIG0Extension19(d); break;  // SI,  announcement switching (8.1.6.2)
  case 20:                     break;  // SI,  service component information (8.1.4)
  case 21: FIG0Extension21(d); break;  // SI,  frequency information (8.1.8)
  case 24:                     break;  // SI,  OE services (8.1.10)
  case 25:                     break;  // SI,  OE announcement support (8.1.6.3)
  case 26:                     break;  // SI,  OE announcement switching (8.1.6.4)
  default: ;                           // undefined
  }
}

// Ensemble information, 6.4.1. FIG0/0
void FibDecoder::FIG0Extension0(const u8 * const d)
{
  SFig0s0_EnsembleInformation fig0s0;
  fig0s0.EId = getBits(d, 16, 16);
  fig0s0.ChangeFlags = getBits_2(d, 16 + 16);
  fig0s0.AlarmFlag = getBits_1(d, 16 + 16 + 2);
  fig0s0.CIFCountHi = getBits_5(d, 16 + 19);
  fig0s0.CIFCountLo = getBits_8(d, 16 + 24);
  fig0s0.OccurrenceChange = getBits_8(d, 16 + 32);

  CIFcount_hi = fig0s0.CIFCountHi;
  CIFcount_lo = fig0s0.CIFCountLo;
  CIFcount = fig0s0.CIFCountHi * 250 + fig0s0.CIFCountLo;

  if (fig0s0.ChangeFlags == 0 && prevChangeFlag == 3)
  {
    qDebug() << "Change FIB configuration";
    std::swap(currentConfig, nextConfig);
    nextConfig->reset();
    cleanup_service_list();
    emit signal_change_in_configuration();
  }

  prevChangeFlag = fig0s0.ChangeFlags;
}

// Subchannel organization 6.2.1 FIG0 extension 1 creates a mapping between the sub channel identifications and the positions in the relevant CIF.
void FibDecoder::FIG0Extension1(const u8 * const d)
{
  i16 used = 2;    // offset in bytes
  const SFigHeader fh = get_fig_header(d);

  while (used < fh.Length)
  {
    used = HandleFIG0Extension1(d, used, fh);
  }
}

// Basic Sub Channels Organization 6.2.1
i16 FibDecoder::HandleFIG0Extension1(const u8 * const d, i16 offset, const SFigHeader & iFH)
{
  i16 bitOffset = offset * 8;
  FibConfig::SFig0s1_BasicSubChannelOrganization fig0s1;

  fig0s1.SubChId = getBits_6(d, bitOffset);

  FibConfig * const pConfig = get_config_ptr(iFH.CN_Flag);
  const auto * const pData = pConfig->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(fig0s1.SubChId);

  if (pData == nullptr)
  {
    fig0s1.StartAddr = getBits(d, bitOffset + 6, 10);
    fig0s1.ShortForm = getBits_1(d, bitOffset + 16) == 0;

    if (fig0s1.ShortForm)
    {
      // UEP short form
      fig0s1.TableSwitch = getBits_1(d, bitOffset + 17);
      fig0s1.TableIndex = getBits_6(d, bitOffset + 18);

      // This should be evaluated only if fig0s1.TableSwitch == 0 but we have no index overflow here and what do else?
      if (fig0s1.TableSwitch != 0) qWarning() << "TableSwitch == 1";
      const SProtLevel &  tableEntry = cProtLevelTable[fig0s1.TableIndex];
      fig0s1.SubChannelSize = tableEntry.CUSize;
      fig0s1.ProtectionLevel = tableEntry.ProtLevel;
      fig0s1.BitRate = tableEntry.BitRate;
      bitOffset += 24;
    }
    else
    {
      // EEP long form
      fig0s1.Option = getBits_3(d, bitOffset + 17);
      fig0s1.ProtectionLevel = getBits_2(d, bitOffset + 20);
      fig0s1.SubChannelSize = getBits(d, bitOffset + 22, 10);

      if (fig0s1.Option == 0x0)
      {
        static constexpr i32 table[] = { 12, 8, 6, 4 };
        fig0s1.BitRate = fig0s1.SubChannelSize / table[fig0s1.ProtectionLevel] * 8;
      }
      else if (fig0s1.Option == 0x1)
      {
        static constexpr i32 table[] = { 27, 21, 18, 15 };
        fig0s1.BitRate = fig0s1.SubChannelSize / table[fig0s1.ProtectionLevel] * 32;
        fig0s1.ProtectionLevel += (1 << 2); // TODO: (1 << 2) this is used to retrieve the option state in later stages, make this nicer
      }
      else qWarning() << "Option" << fig0s1.Option << "not supported";
      bitOffset += 32;
    }

    pConfig->Fig0s1_BasicSubChannelOrganizationVec.emplace_back(fig0s1);
    retrigger_timer_data_loaded();
  }
  else
  {
    bitOffset += pData->ShortForm ? 24 : 32;
  }
  return (i16)(bitOffset / 8);  // we return bytes
}

// Service organization, 6.3.1. Bind channels to SIds.
void FibDecoder::FIG0Extension2(const u8 * const d)
{
  i16 used = 2;    // offset in bytes
  const SFigHeader fh = get_fig_header(d);

  while (used < fh.Length)
  {
    used = HandleFIG0Extension2(d, used, fh);
  }
}

i16 FibDecoder::HandleFIG0Extension2(const u8 * const d, i16 offset, const SFigHeader & iFH)
{
  i16 bitOffset = 8 * offset;
  {
    u8 CountryId;
    u32 SId;

    if (iFH.PD_Flag == 1)
    {    // long Sid, data
      const u8 ecc = getBits_8(d, bitOffset);
      (void)ecc;
      CountryId = getBits_4(d, bitOffset + 4);
      SId = getLBits(d, bitOffset, 32); // SId overlaps ecc and CountryId
      bitOffset += 32;
    }
    else
    {
      CountryId = getBits_4(d, bitOffset);
      SId = getBits(d, bitOffset, 16);  // SId overlaps CountryId
      bitOffset += 16;
    }

    ensemble->countryId = CountryId;

    const i16 numOfServComp = getBits_4(d, bitOffset + 4);
    bitOffset += 8;

    FibConfig * const pConfig = get_config_ptr(iFH.CN_Flag);

    for (i16 i = 0; i < numOfServComp; i++)
    {
      const ETMId TMId = (ETMId)getBits_2(d, bitOffset);

      if (TMId == ETMId::StreamModeAudio)
      {
        // Audio
        const u8 ASCTy = getBits_6(d, bitOffset + 2);
        const u8 SubChId = getBits_6(d, bitOffset + 8);
        const u8 PS_flag = getBits_1(d, bitOffset + 14);
        bind_audio_service(pConfig, TMId, SId, i, SubChId, PS_flag, ASCTy);
      }
      else if (TMId == ETMId::PacketModeData)
      {
        // MSC packet data
        const i16 SCId = getBits(d, bitOffset + 2, 12);
        const u8 PS_flag = getBits_1(d, bitOffset + 14);
        const u8 CA_flag = getBits_1(d, bitOffset + 15);
        bind_packet_service(pConfig, TMId, SId, i, SCId, PS_flag, CA_flag);
      }
      else
      {
        qWarning() << "TIMid not supported --" << (int)TMId;
      }
      bitOffset += 16;
    }
  }
  const i16 bitOffset2 = bitOffset; // for test only
  // *****************************************
  {
    bitOffset = 8 * offset;
    FibConfig::SFig0s2_BasicService_ServiceComponentDefinition fig0s2;

    fig0s2.PD_Flag = iFH.PD_Flag;

    // u8 CountryId;
    u32 SId;

    if (iFH.PD_Flag == 1)
    {    // long Sid, data
      SId = fig0s2.PD1.SId = getLBits(d, bitOffset, 32); // SId overlaps ecc and CountryId
      ensemble->countryId = fig0s2.PD1.CountryId;
      bitOffset += 32;
    }
    else
    {
      SId = fig0s2.PD0.SId = getLBits(d, bitOffset, 16); // SId overlaps ecc and CountryId
      ensemble->countryId = fig0s2.PD0.CountryId;
      bitOffset += 16;
    }

    FibConfig * const pConfig = get_config_ptr(iFH.CN_Flag);
    // const auto * const pData = pConfig->get_Fig0s2_BasicService_ServiceComponentDefinition_of_SId(fig0s2.PD0.SId);

    // if (pData == nullptr)
    {
      fig0s2.NumServiceComp = getBits_4(d, bitOffset + 4);
      bitOffset += 8;

      // FibConfig::SId_struct SId_element;
      // SId_element.announcing = 0;
      // SId_element.SId = SId;

      // if (pConfig->get_SIdTable_of_SId(SId) != nullptr) // SId already recorded
      // {
      //   bitOffset += fig0s2.NumServiceComp * 16;
      //   assert(bitOffset == bitOffset2);
      //   return bitOffset / 8;
      // }

      for (i16 servCombIdx = 0; servCombIdx < fig0s2.NumServiceComp; servCombIdx++)
      {
        if (pConfig->get_Fig0s2_BasicService_ServiceComponentDefinition_of_SId_ScIdx(SId, servCombIdx) == nullptr) // is SID on same index already known?
        {
          FibConfig::SFig0s2_ServiceComponentDefinition & fig0s2_scd = fig0s2.ServiceComp_C;
          fig0s2.ServiceComp_C_index = servCombIdx;
          fig0s2_scd.TMId = getBits_2(d, bitOffset);

          if ((ETMId)fig0s2_scd.TMId == ETMId::StreamModeAudio)
          {
            // Audio
            fig0s2_scd.TMId00.ASCTy = getBits_6(d, bitOffset + 2);
            fig0s2_scd.TMId00.SubChId = getBits_6(d, bitOffset + 8);
          }
          else if ((ETMId)fig0s2_scd.TMId == ETMId::PacketModeData)
          {
            // MSC packet data
            fig0s2_scd.TMId11.SCId = getBits(d, bitOffset + 2, 12);
          }
          else
          {
            qWarning() << "TIMid not supported" << fig0s2_scd.TMId;
          }
          fig0s2_scd.PS_Flag = getBits_1(d, bitOffset + 14);
          fig0s2_scd.CA_Flag = getBits_1(d, bitOffset + 15);

          // SId_element.comps.push_back(pConfig->Fig0s2_BasicService_ServiceComponentDefinitionVec.size());
          pConfig->Fig0s2_BasicService_ServiceComponentDefinitionVec.emplace_back(fig0s2);
          retrigger_timer_data_loaded();
        }
        bitOffset += 16;
      }
    }
    // else
    // {
    //   bitOffset += 8 + pData->NumServiceComp * 16;
    // }
  }
  assert(bitOffset == bitOffset2);
  assert(bitOffset % 8 == 0); // only full bytes should occur
  return bitOffset / 8;    // in Bytes
}

// Service component in packet mode 6.3.2.
// The Extension 3 of FIG type 0 (FIG 0/3) gives additional information about the service component description in packet mode.
void FibDecoder::FIG0Extension3(const u8 * const d)
{
  i16 used = 2;            // offset in bytes
  const SFigHeader fh = get_fig_header(d);

  while (used < fh.Length)
  {
    used = HandleFIG0Extension3(d, used, fh);
  }
}

// Note that the SCId (Service Component Identifier) is	a unique 12 bit number in the ensemble
i16 FibDecoder::HandleFIG0Extension3(const u8 * const d, const i16 used, const SFigHeader & iFH)
{
  i16 bitOffset = used * 8;
  {
    const i16 SCId = getBits(d, bitOffset, 12);
    const i16 CAOrgflag = getBits_1(d, bitOffset + 15);
    const i16 DGflag = getBits_1(d, bitOffset + 16);
    const i16 DSCTy = getBits_6(d, bitOffset + 18);
    const i16 SubChId = getBits_6(d, bitOffset + 24);
    const i16 packetAddress = getBits(d, bitOffset + 30, 10);

    FibConfig * const localBase = get_config_ptr(iFH.CN_Flag);

    if (CAOrgflag == 1)
    {
      const u16 CAOrg = getBits(d, bitOffset + 40, 16);
      (void)CAOrg;
      bitOffset += 16;
    }
    bitOffset += 40;

    // const auto itScd = localBase->serviceCompDescMap.find(SCId);
    const auto pScd = find_service_component(localBase, SCId);

    if (pScd == nullptr)
    {
      goto gotolabel;
    }

    // We want to have the subchannel OK
    if (localBase->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(SubChId) == nullptr) // TODO: why necessary?
    {
      goto gotolabel;
    }

    // If the component exists, we first look whether is was already handled
    if (pScd->isMadePublic)
    {
      goto gotolabel;
    }

    // If the Data Service Component Type == 0, we do not deal with it
    if (DSCTy == 0)
    {
      goto gotolabel;
    }

    const auto itServ = ensemble->servicesMap.find(pScd->SId);

    if (itServ == ensemble->servicesMap.end())
    {
      return used;
    }

    const QString serviceName = itServ->second.serviceLabel;

    if (!itServ->second.is_shown)
    {
      emit signal_add_to_ensemble(serviceName, pScd->SId);
    }

    itServ->second.is_shown = true;
    pScd->isMadePublic = true;
    pScd->subChannelId = SubChId;
    pScd->DSCTy = DSCTy;
    pScd->DgFlag = DGflag;
    pScd->packetAddress = packetAddress;
  }
  // ****************************************
gotolabel:
  i16 bitOffset2 = bitOffset; // for test only
  bitOffset = used * 8;
  {
    bitOffset = used * 8;
    FibConfig::SFig0s3_ServiceComponentPacketMode fig0s3;

    fig0s3.SCId = getBits(d, bitOffset, 12);

    auto * const pConfig = get_config_ptr(iFH.CN_Flag);
    auto * const pData = pConfig->get_Fig0s3_ServiceComponentPacketMode_of_SCId(fig0s3.SCId);

    if (pData == nullptr)
    {
      fig0s3.CAOrg_Flag = getBits_1(d, bitOffset + 15);
      fig0s3.DG_Flag = getBits_1(d, bitOffset + 16);
      fig0s3.DSCTy = getBits_6(d, bitOffset + 18);
      fig0s3.SubChId = getBits_6(d, bitOffset + 24);
      fig0s3.PacketAddress = getBits(d, bitOffset + 30, 10);
      bitOffset += 40;

      if (fig0s3.CAOrg_Flag == 1)
      {
        fig0s3.CAOrg = getBits(d, bitOffset + 40, 16);
        bitOffset += 16;
      }

      pConfig->Fig0s3_ServiceComponentPacketModeVec.emplace_back(fig0s3);
      retrigger_timer_data_loaded();
    }
    else
    {
      bitOffset += 40 + (pData->CAOrg_Flag != 0 ? 16 : 0);
    }
  }

  assert(bitOffset == bitOffset2);
  assert(bitOffset % 8 == 0); // only full bytes should occur
  return bitOffset / 8;
}

//	Service component language 8.1.2
void FibDecoder::FIG0Extension5(const u8 * const d)
{
  i16 used = 2;    // offset in bytes
  const SFigHeader fh = get_fig_header(d);

  while (used < fh.Length)
  {
    used = HandleFIG0Extension5(d, used, fh);
  }
}

i16 FibDecoder::HandleFIG0Extension5(const u8 * const d, i16 offset, const SFigHeader & iFH)
{
  i16 bitOffset = offset * 8;
  u8 lsFlag = getBits_1(d, bitOffset);
  i16 language;

  FibConfig * const localBase = get_config_ptr(iFH.CN_Flag);

  if (lsFlag == 0)
  {
    // short form
    if (getBits_1(d, bitOffset + 1) == 0)
    {
      SSubChannelDescFig0_5 subChannel;
      i16 subChId = getBits_6(d, bitOffset + 2);
      subChannel.language = getBits_8(d, bitOffset + 8);
      localBase->subChDescMapFig0_5.try_emplace(subChId, subChannel);
    }
    bitOffset += 16;
  }
  else
  {
    // long form
    i16 SCId = getBits(d, bitOffset + 4, 12);
    language = getBits_8(d, bitOffset + 16);

    //const auto itScd = localBase->serviceCompDescMap.find(SCId);
    const auto pScd = find_service_component(localBase, SCId);

    if (pScd != nullptr)
    {
      pScd->language = language;
    }
    bitOffset += 24;
  }

  assert(bitOffset % 8 == 0); // only full bytes should occur
  return bitOffset / 8;
}

// FIG0/7: Configuration linking information 6.4.2,
//
void FibDecoder::FIG0Extension7(const u8 * const d)
{
  i16 used = 2;            // offset in bytes
  const SFigHeader fh = get_fig_header(d);

  const i32 serviceCount = getBits_6(d, used * 8);
  const i32 counter = getBits(d, used * 8 + 6, 10);
  (void)counter;

  if (fh.CN_Flag == 0)
  {  // only current configuration for now
    emit signal_nr_services(serviceCount);
  }
}

// FIG0/8:  Service Component Global Definition (6.3.5)
//
void FibDecoder::FIG0Extension8(const u8 * const d)
{
  i16 used = 2;    // offset in bytes
  const SFigHeader fh = get_fig_header(d);

  while (used < fh.Length)
  {
    used = HandleFIG0Extension8(d, used, fh);
  }
}

i16 FibDecoder::HandleFIG0Extension8(const u8 * const d, const i16 used, const SFigHeader & iFH)
{
  i16 bitOffset = used * 8;
  {
    const u32 SId = getLBits(d, bitOffset, iFH.PD_Flag == 1 ? 32 : 16);
    bitOffset += iFH.PD_Flag == 1 ? 32 : 16;

    FibConfig * const localBase = get_config_ptr(iFH.CN_Flag);

    const u8 extFlag = getBits_1(d, bitOffset);
    const u16 SCIdS = getBits_4(d, bitOffset + 4);
    bitOffset += 8;

    const u8 lsFlag = getBits_1(d, bitOffset);
    // qDebug() << "SId1" << SId << "lsFlag" << lsFlag;

    if (lsFlag == 0)
    {
      // short form
      const i16 subChId = getBits_6(d, bitOffset + 2);

      if (localBase->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(subChId) != nullptr) // TODO: why necessary?
      {
        const auto pScd = find_component(localBase, SId, subChId);

        if (pScd != nullptr)
        {
          pScd->SCIdS = SCIdS;
        }
      }
      bitOffset += 8;
    }
    else
    {
      // long form
      const i32 SCId = getBits(d, bitOffset + 4, 12);
      const auto pScd = find_service_component(localBase, SCId);

      if (pScd != nullptr)
      {
        pScd->SCIdS = SCIdS;
      }
      bitOffset += 16;  // TODO: was 8 before, is a bug?
    }

    if (extFlag)
    {
      bitOffset += 8; // skip Rfa
    }
  }
  // ***********************************
  i16 bitOffset2 = bitOffset;
  bitOffset = used * 8;
  {
    const u32 SId = getLBits(d, bitOffset, iFH.PD_Flag == 1 ? 32 : 16);
    bitOffset += iFH.PD_Flag == 1 ? 32 : 16;
    const u8 LS_Flag = getBits_1(d, bitOffset + 8);
    auto * const pConfig = get_config_ptr(iFH.CN_Flag);

    FibConfig::SFig0s8_ServiceComponentGlobalDefinition fig0s8;
    fig0s8.SId = SId;
    fig0s8.PD_Flag = iFH.PD_Flag;
    fig0s8.LS_Flag = LS_Flag;
    fig0s8.Ext_Flag = getBits_1(d, bitOffset);
    fig0s8.SCIdS = getBits_4(d, bitOffset + 4);

    if (LS_Flag == 0) // short form
    {
      fig0s8.SubChId = getBits_6(d, bitOffset + 4 + 4 + 2);
      bitOffset += (fig0s8.Ext_Flag != 0 ? 16 + 8 : 16); // skip Rfa
    }
    else // long form
    {
      fig0s8.SCId = getBits(d, bitOffset + 4 + 4 + 1 + 3, 12);
      bitOffset += (fig0s8.Ext_Flag != 0 ? 24 + 8 : 24); // skip Rfa
    }

    const auto * const pData = pConfig->get_Fig0s8_ServiceComponentGlobalDefinition_of_SId(fig0s8.SId);
    if (pData == nullptr)
    {
      pConfig->Fig0s8_ServiceComponentGlobalDefinitionVec.emplace_back(fig0s8);
      retrigger_timer_data_loaded();
    }

    // if (dataChanged)
    // {
    //   for (const auto & eSCId : pConfig->Fig0s8_ServiceComponentGlobalDefinitionVec_SCId)
    //   {
    //     for (const auto & eSubChId : pConfig->Fig0s8_ServiceComponentGlobalDefinitionVec_SubChId)
    //     {
    //        if (eSubChId.SId == eSCId.SId)
    //        {
    //          qDebug() << "eSubChId.SubChId" << eSubChId.SubChId << "eSubChId.SId" << eSubChId.SId << "eSCId.SCId" << eSCId.SCId << "eSubChId.SCIdS" << eSubChId.SCIdS << "eSCId.SCIdS" << eSCId.SCIdS;
    //        }
    //     }
    //   }
    // }
  }
  assert(bitOffset == bitOffset2);
  assert(bitOffset % 8 == 0); // only full bytes should occur
  return bitOffset / 8;
}

//	User Application Information 6.3.6
void FibDecoder::FIG0Extension13(const u8 * const d)
{
  i16 used = 2;    // offset in bytes
  const SFigHeader fh = get_fig_header(d);

  while (used < fh.Length)
  {
    used = HandleFIG0Extension13(d, used, fh);
  }
}

//
//	section 6.3.6 User application Data
i16 FibDecoder::HandleFIG0Extension13(const u8 * const d, i16 used, const SFigHeader & iFH)
{
  i16 bitOffset = used * 8;
  {
    const u32 SId = getLBits(d, bitOffset, iFH.PD_Flag == 1 ? 32 : 16);

    FibConfig * const localBase = get_config_ptr(iFH.CN_Flag);

    bitOffset += iFH.PD_Flag == 1 ? 32 : 16;
    const u16 SCIdS = getBits_4(d, bitOffset);
    const i16 NoApplications = getBits_4(d, bitOffset + 4);
    bitOffset += 8;

    const auto it = ensemble->servicesMap.find(SId);

    for (i16 i = 0; i < NoApplications; i++)
    {
      const i16 appType = getBits(d, bitOffset, 11);
      const i16 length = getBits_5(d, bitOffset + 11);
      bitOffset += (11 + 5 + 8 * length);

      if (it == ensemble->servicesMap.end())
      {
        continue; // only to count up the bitOffset
      }

      auto pScd = find_service_component(localBase, SId, SCIdS);

      if (pScd != nullptr)
      {
        if (pScd->TMid == ETMId::PacketModeData)
        {
          pScd->appType = appType;
        }
      }
    }
  }
  // ***********************************
  i16 bitOffset2 = bitOffset;
  bitOffset = used * 8;
  {
    FibConfig::SFig0s13_UserApplicationInformation fig0s13;
    fig0s13.SId = getLBits(d, bitOffset, iFH.PD_Flag == 1 ? 32 : 16);
    bitOffset += iFH.PD_Flag == 1 ? 32 : 16;
    fig0s13.SCIdS = getBits_4(d, bitOffset);

    const auto pFig0s13 = currentConfig->get_Fig0s13_UserApplicationInformation_of_SId_SCIdS(fig0s13.SId, fig0s13.SCIdS);
    if (pFig0s13 == nullptr)
    {
      fig0s13.PD_Flag = iFH.PD_Flag;
      fig0s13.NumUserApps = getBits_4(d, bitOffset + 4);
      bitOffset += 8;

      if (fig0s13.NumUserApps > (i16)fig0s13.UserAppVec.size())
      {
        qCritical() << "Error: NumUserApps > UserAppVec.size()";
        fig0s13.NumUserApps = fig0s13.UserAppVec.size();
      }

      for (i16 i = 0; i < fig0s13.NumUserApps; ++i)
      {
        FibConfig::SFig0s13_UserApplicationInformation::SUserApp & userApp = fig0s13.UserAppVec[i];
        userApp.UserAppType = getBits(d, bitOffset, 11);
        userApp.UserAppDataLength = getBits_5(d, bitOffset + 11); // in bytes
        bitOffset += (11 + 5 + 8 * userApp.UserAppDataLength);
      }

      fig0s13.SizeBits = bitOffset - used * 8; // only store netto size
      auto * const pConfig = get_config_ptr(iFH.CN_Flag);
      pConfig->Fig0s13_UserApplicationInformationVec.emplace_back(fig0s13);
      retrigger_timer_data_loaded();
    }
    else
    {
      bitOffset = pFig0s13->SizeBits + used * 8;
    }
  }
  assert(bitOffset == bitOffset2);
  assert(bitOffset % 8 == 0); // only full bytes should occur
  return bitOffset / 8;
}

//	FEC sub-channel organization 6.2.2
void FibDecoder::FIG0Extension14(const u8 * const d)
{
  const SFigHeader fh = get_fig_header(d);
  i16 used = 2;      // in Bytes

  FibConfig * const pConfig = get_config_ptr(fh.CN_Flag);

  while (used < fh.Length)
  {
    // if (1)
    {
      const i16 subChId = getBits_6(d, used * 8);
      const u8 FEC_scheme = getBits_2(d, used * 8 + 6);
      used += 1;

      SSubChannelDescFig0_14 subChannel;
      subChannel.FEC_scheme = FEC_scheme;
      pConfig->subChDescMapFig0_14.try_emplace(subChId, subChannel);
    }
    //else // ******************************
    {
      FibConfig::SFig0s14_SubChannelOrganization fig0s14;
      fig0s14.SubChId = getBits_6(d, used * 8);
      fig0s14.FEC_scheme = getBits_2(d, used * 8 + 6);
      used += 1;
      if (pConfig->get_Fig0s14_SubChannelOrganization_of_SubChId(fig0s14.SubChId) == nullptr)
      {
        pConfig->Fig0s14_SubChannelOrganizationVec.emplace_back(fig0s14);
        retrigger_timer_data_loaded();
      }
    }
  }
}

void FibDecoder::FIG0Extension17(const u8 * const d)
{
  const i16 length = getBits_5(d, 3);
  i16 offset = 16;

  while (offset < length * 8)
  {
    const u16 SId = getBits(d, offset, 16);
    const bool L_flag = getBits_1(d, offset + 18);
    const bool CC_flag = getBits_1(d, offset + 19);
    i16 Language = 0x00;  // init with unknown language

    if (L_flag)
    {
      // language field present
      Language = getBits_8(d, offset + 24);
      offset += 8;
    }

    const i16 type = getBits_5(d, offset + 27);
    if (CC_flag)
    {      // cc flag
      offset += 40;
    }
    else
    {
      offset += 32;
    }

    const auto it = ensemble->servicesMap.find(SId);

    if (it != ensemble->servicesMap.end())
    {
      it->second.language = Language;
      it->second.programType = type;
    }
  }
}

//
//	Announcement support 8.1.6.1
void FibDecoder::FIG0Extension18(const u8 * const d)
{
  const SFigHeader fh = get_fig_header(d);
  constexpr i16 used = 2;      // in Bytes
  i16 bitOffset = used * 8;

  FibConfig * const localBase = get_config_ptr(fh.CN_Flag);

  while (bitOffset < fh.Length * 8)
  {
    const u16 SId = getBits(d, bitOffset, 16);
    const auto it = ensemble->servicesMap.find(SId);

    bitOffset += 16;
    const u16 asuFlags = getBits(d, bitOffset, 16);
    (void)asuFlags;
    bitOffset += 16;
    const u8 Rfa = getBits(d, bitOffset, 5);
    (void)Rfa;
    const u8 nrClusters = getBits(d, bitOffset + 5, 3);
    bitOffset += 8;

    for (i32 i = 0; i < nrClusters; i++)
    {
      if (getBits(d, bitOffset + 8 * i, 8) == 0)
      {
        continue;
      }

      if (it != ensemble->servicesMap.end() && it->second.hasName)
      {
        set_cluster(localBase, getBits(d, bitOffset + 8 * i, 8), SId, asuFlags);
      }
    }
    bitOffset += nrClusters * 8;
  }
}

//
//	Announcement switching 8.1.6.2
void FibDecoder::FIG0Extension19(const u8 * const d)
{
  i16 Length = getBits_5(d, 3);  // in Bytes
  u8 CN_bit = getBits_1(d, 8 + 0);
  u8 OE_bit = getBits_1(d, 8 + 1);
  u8 PD_bit = getBits_1(d, 8 + 2);
  i16 used = 2;      // in Bytes
  i16 bitOffset = used * 8;

  FibConfig * const localBase = get_config_ptr(CN_bit);

  (void)OE_bit;
  (void)PD_bit;
  while (bitOffset < Length * 8)
  {
    u8 clusterId = getBits(d, bitOffset, 8);
    bitOffset += 8;
    u16 AswFlags = getBits(d, bitOffset, 16);
    bitOffset += 16;

    u8 newFlag = getBits(d, bitOffset, 1);
    (void)newFlag;
    bitOffset += 1;
    u8 regionFlag = getBits(d, bitOffset, 1);
    bitOffset += 1;
    u8 subChId = getBits(d, bitOffset, 6);
    bitOffset += 6;
    if (regionFlag == 1)
    {
      bitOffset += 2;  // skip Rfa
      u8 regionId = getBits(d, bitOffset, 6);
      bitOffset += 6;
      (void)regionId;
    }

    if (!sync_reached())
    {
      return;
    }
    Cluster * myCluster = get_cluster(localBase, clusterId);
    if (myCluster == nullptr)
    {  // should not happen
      //	      fprintf (stderr, "cluster fout\n");
      return;
    }

    if ((myCluster->flags & AswFlags) != 0)
    {
      myCluster->announcing++;
      if (myCluster->announcing == 5)
      {
        for (u16 i = 0; i < myCluster->servicesSIDs.size(); i++)
        {
          //const QString name = ensemble->services[myCluster->services[i]].serviceLabel;
          const u32 SId = myCluster->servicesSIDs[i];
          const auto it = ensemble->servicesMap.find(SId);
          if (it == ensemble->servicesMap.end())
          {
            qCritical() << "service not found" << SId; // should not happen
            return;
          }
          const QString name = it->second.serviceLabel;
          emit signal_start_announcement(name, subChId);
        }
      }
    }
    else
    {  // end of announcement
      if (myCluster->announcing > 0)
      {
        myCluster->announcing = 0;
        for (u16 i = 0; i < myCluster->servicesSIDs.size(); i++)
        {
          const u32 SId = myCluster->servicesSIDs[i];
          const auto it = ensemble->servicesMap.find(SId);
          if (it == ensemble->servicesMap.end())
          {
            qCritical() << "service not found" << SId; // should not happen
            return;
          }
          const QString name = it->second.serviceLabel;
          emit signal_stop_announcement(name, subChId);
        }
      }
    }
  }
}

//
//	Frequency information (FI) 8.1.8
void FibDecoder::FIG0Extension21(const u8 * const d)
{
  i16 used = 2;    // offset in bytes
  const SFigHeader fh = get_fig_header(d);

  while (used < fh.Length)
  {
    used = HandleFIG0Extension21(d, used, fh);
  }
}

i16 FibDecoder::HandleFIG0Extension21(const u8 * const d, i16 offset, const SFigHeader & iFH)
{
  const i16 l_offset = offset * 8;
  const i16 l = getBits_5(d, l_offset + 11);
  const i16 upperLimit = l_offset + 16 + l * 8;
  i16 base = l_offset + 16;

  while (base < upperLimit)
  {
    const u16 idField = getBits(d, base, 16);
    const u8 RandM = getBits_4(d, base + 16);
    const u8 continuity = getBits_1(d, base + 20);
    (void)continuity;
    const u8 length = getBits_3(d, base + 21);

    if (RandM == 0x08)
    {
      const u16 fmFrequency_key = getBits(d, base + 24, 8);
      const i32 fmFrequency = 87500 + fmFrequency_key * 100;

      const auto it = ensemble->servicesMap.find(idField);

      if (it != ensemble->servicesMap.end())
      {
        if ((it->second.hasName) && (it->second.fmFrequency == -1))
        {
          it->second.fmFrequency = fmFrequency;
        }
      }
    }
    base += 24 + length * 8;
  }
  assert(upperLimit % 8 == 0); // only full bytes should occur
  return upperLimit / 8;
}
