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
#include <cstring>
#include <vector>
#include "dabradio.h"
#include "charsets.h"
#include "dab-config.h"
#include "fib-table.h"
#include "bit-extractors.h"


FibDecoder::FibDecoder(DabRadio * mr)
  : myRadioInterface(mr)
  , ensemble(new EnsembleDescriptor)
{
  //connect(this, &FibDecoder::addtoEnsemble, myRadioInterface, &RadioInterface::slot_add_to_ensemble);
  connect(this, &FibDecoder::signal_name_of_ensemble, myRadioInterface, &DabRadio::slot_name_of_ensemble);
  connect(this, &FibDecoder::signal_clock_time, myRadioInterface, &DabRadio::slot_clock_time);
  connect(this, &FibDecoder::signal_change_in_configuration, myRadioInterface, &DabRadio::slot_change_in_configuration);
  connect(this, &FibDecoder::signal_start_announcement, myRadioInterface, &DabRadio::slot_start_announcement);
  connect(this, &FibDecoder::signal_stop_announcement, myRadioInterface, &DabRadio::slot_stop_announcement);
  connect(this, &FibDecoder::signal_nr_services, myRadioInterface, &DabRadio::slot_nr_services);

  currentConfig = new DabConfig();
  nextConfig = new DabConfig();
}

FibDecoder::~FibDecoder()
{
  delete nextConfig;
  delete currentConfig;
  delete ensemble;
}

void FibDecoder::process_FIB(const std::array<std::byte, cFibSizeVitOut> & iFibBits, u16 const iFicNo)
{
  // iFibBits: 30 Bytes Data * 8  + 16 Bit CRC = 32 Byte / 256 bits
  // The caller already does CRC16 check.
  i32 processedBytes = 0;

  fibLocker.lock();
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
    case 0:
      process_FIG0(d);
      break;

    case 1:
      process_FIG1(d);
      break;

    default: break;
    }
    processedBytes += FIGlength + 1; // data length plus header length
  }
  fibLocker.unlock();

  if (processedBytes > 30)
  {
    qCritical() << "FIG package length error" << processedBytes;
  }
}

FibDecoder::SFigHeader FibDecoder::get_fig_header(const u8 * const d)
{
  SFigHeader header;
  header.Length = getBits_5(d, 3);
  header.CN_bit = getBits_1(d, 8 + 0);
  header.OE_bit = getBits_1(d, 8 + 1);
  header.PD_bit = getBits_1(d, 8 + 2);
  return header; // should be "moved"
}

void FibDecoder::process_FIG0(const u8 * const d)
{
  const u8 extension = getBits_5(d, 8 + 3);  // skip C/N, OE and P/D flags

  switch (extension)
  {
  case 0:    // ensemble information (6.4.1)
    FIG0Extension0(d);
    break;

  case 1:    // sub-channel organization (6.2.1)
    FIG0Extension1(d);
    break;

  case 2:    // service organization (6.3.1)
    FIG0Extension2(d);
    break;

  case 3:    // service component in packet mode (6.3.2)
    FIG0Extension3(d);
    break;

  case 4:    // service component with CA (6.3.3)
    break;

  case 5:    // service component language (8.1.2)
    FIG0Extension5(d);
    break;

  case 6:    // service linking information (8.1.15)
    break;

  case 7:    // configuration information (6.4.2)
    FIG0Extension7(d);
    break;

  case 8:    // service component global definition (6.3.5)
    FIG0Extension8(d);
    break;

  case 9:              // country, LTO & international table (8.1.3.2)
    FIG0Extension9(d);
    break;

  case 10:             // date and time (8.1.3.1)
    FIG0Extension10(d);
    break;

  case 11:    // obsolete
    break;

  case 12:    // obsolete
    break;

  case 13:             // user application information (6.3.6)
    FIG0Extension13(d);
    break;

  case 14:             // FEC subchannel organization (6.2.2)
    FIG0Extension14(d);
    break;

  case 15:    // obsolete
    break;

  case 16:    // obsolete
    break;

  case 17:    // Program type (8.1.5)
    FIG0Extension17(d);
    break;

  case 18:             // announcement support (8.1.6.1)
    FIG0Extension18(d);
    break;

  case 19:             // announcement switching (8.1.6.2)
    FIG0Extension19(d);
    break;

  case 20:    // service component information (8.1.4)
    break;    // to be implemented

  case 21:    // frequency information (8.1.8)
    FIG0Extension21(d);
    break;

  case 22:    // obsolete
    break;

  case 23:    // obsolete
    break;

  case 24:    // OE services (8.1.10)
    break;    // not implemented

  case 25:    // OE announcement support (8.1.6.3)
    break;    // not implemented

  case 26:    // OE announcement switching (8.1.6.4)
    break;    // not implemented

  case 27:
  case 28:
  case 29:    // undefined
    break;

  default: break;
  }
}

//	Ensemble information, 6.4.1
//	FIG0/0 indicated a change in channel organization
//	we are not equipped for that, so we just ignore it for the moment
//	The info is MCI
void FibDecoder::FIG0Extension0(const u8 * const d)
{
  const u8 CN_bit = getBits_1(d, 8 + 0);
  const u16 EId = getBits(d, 16, 16);
  const u8 changeFlag = getBits_2(d, 16 + 16);
  const u8 alarmFlag = getBits_1(d, 16 + 16 + 2);
  const u16 highpart = getBits_5(d, 16 + 19);
  const u16 lowpart = getBits_8(d, 16 + 24);
  const i16 occurrenceChange = getBits_8(d, 16 + 32);

  (void)CN_bit;
  (void)EId;
  (void)alarmFlag;
  (void)occurrenceChange;

  CIFcount_hi = highpart;
  CIFcount_lo = lowpart;
  CIFcount = highpart * 250 + lowpart;

  if ((changeFlag == 0) && (prevChangeFlag == 3))
  {
    fprintf(stdout, "handling change\n");
    DabConfig * temp = currentConfig;
    currentConfig = nextConfig;
    nextConfig = temp;
    nextConfig->reset();
    cleanup_service_list();
    emit signal_change_in_configuration();
  }

  prevChangeFlag = changeFlag;
  //	if (alarmFlag)
  //	   fprintf (stderr, "serious problem\n");
}

//	Subchannel organization 6.2.1
//	FIG0 extension 1 creates a mapping between the
//	sub channel identifications and the positions in the
//	relevant CIF.
void FibDecoder::FIG0Extension1(const u8 * const d)
{
  i16 used = 2;    // offset in bytes
  const SFigHeader fh = get_fig_header(d);

  while (used < fh.Length /*- 1*/) // TODO: -1 is a bug?
  {
    used = HandleFIG0Extension1(d, used, fh);
  }
}

// Basic Sub Channels Organization 6.2.1
i16 FibDecoder::HandleFIG0Extension1(const u8 * const d, i16 offset, const SFigHeader & iFH)
{
  i16 bitOffset = offset * 8;
  const i16 subChId = getBits_6(d, bitOffset);
  const i16 startAdr = getBits(d, bitOffset + 6, 10);

  SSubChannelDescFig0_1 subChannel;
  subChannel.startAddr = startAdr;
  // subChannel.inUse = true;

  if (getBits_1(d, bitOffset + 16) == 0)
  {
    // short form
    subChannel.shortForm = true;    // short form

    const i16 tableIndex = getBits_6(d, bitOffset + 18);
    subChannel.Length = cProtLevelTable[tableIndex].Length;
    subChannel.protLevel = cProtLevelTable[tableIndex].ProtLevel;
    subChannel.bitRate = cProtLevelTable[tableIndex].BitRate;

    bitOffset += 24;

    // qDebug() << "FIG 0/1 short form" << "subChId" << subChId << "startAdr" << startAdr
    //          << "length" << subChannel.Length << "protLevel" << subChannel.protLevel << "bitRate" << subChannel.bitRate;
  }
  else
  {
    // EEP long form
    subChannel.shortForm = false;

    const i16 option = getBits_3(d, bitOffset + 17);

    if (option == 0x0)
    {
      // A-Level protection
      static constexpr i32 table_1[] = { 12, 8, 6, 4 };

      const i16 protLevel = getBits_2(d, bitOffset + 20);
      subChannel.protLevel = protLevel;

      const i16 subChanSize = getBits(d, bitOffset + 22, 10);
      subChannel.Length = subChanSize;
      subChannel.bitRate = subChanSize / table_1[protLevel] * 8;
    }
    else if (option == 0x1)
    {
      // B-Level protection
      static constexpr i32 table_2[] = { 27, 21, 18, 15 };

      const i16 protLevel = getBits_2(d, bitOffset + 20);
      subChannel.protLevel = protLevel + (1 << 2);

      const i16 subChanSize = getBits(d, bitOffset + 22, 10);
      subChannel.Length = subChanSize;
      subChannel.bitRate = subChanSize / table_2[protLevel] * 32;
    }
    else
    {
      qCritical() << "Unknown option in FIG 0/1" << option;
      return (i16)((bitOffset + 32) / 8); // as this is a unknown path this is only an assumption to go on further
    }

    bitOffset += 32;

    // qDebug() << "FIG 0/1 long form, option" << option << "subChId" << subChId << "startAdr" << startAdr
    //          << "length" << subChannel.Length << "protLevel" << subChannel.protLevel << "bitRate" << subChannel.bitRate;
  }

  DabConfig * const localBase = iFH.CN_bit == 0 ? currentConfig : nextConfig;
  localBase->subChDescMapFig0_1.try_emplace(subChId, subChannel);

  return (i16)(bitOffset / 8);  // we return bytes
}

//
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
  u8 CountryId;
  u32 SId;

  if (iFH.PD_bit == 1)
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

  DabConfig * const pDabConfig = (iFH.CN_bit == 0) ? currentConfig : nextConfig;

  for (i16 i = 0; i < numOfServComp; i++)
  {
    const ETMId TMId = (ETMId)getBits_2(d, bitOffset);

    if (TMId == ETMId::StreamModeAudio)
    {
      // Audio
      const u8 ASCTy = getBits_6(d, bitOffset + 2);
      const u8 SubChId = getBits_6(d, bitOffset + 8);
      const u8 PS_flag = getBits_1(d, bitOffset + 14);
      bind_audio_service(pDabConfig, TMId, SId, i, SubChId, PS_flag, ASCTy);
    }
    else if (TMId == ETMId::PacketModeData)
    {
      // MSC packet data
      const i16 SCId = getBits(d, bitOffset + 2, 12);
      const u8 PS_flag = getBits_1(d, bitOffset + 14);
      const u8 CA_flag = getBits_1(d, bitOffset + 15);
      bind_packet_service(pDabConfig, TMId, SId, i, SCId, PS_flag, CA_flag);
    }
    else
    {
      qWarning() << "TIMid not supported" << (int)TMId;
    }
    bitOffset += 16;
  }
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
i16 FibDecoder::HandleFIG0Extension3(const u8 * const d, i16 used, const SFigHeader & iFH)
{
  const i16 SCId = getBits(d, used * 8, 12);
  const i16 CAOrgflag = getBits_1(d, used * 8 + 15);
  const i16 DGflag = getBits_1(d, used * 8 + 16);
  const i16 DSCTy = getBits_6(d, used * 8 + 18);
  const i16 SubChId = getBits_6(d, used * 8 + 24);
  const i16 packetAddress = getBits(d, used * 8 + 30, 10);

  DabConfig * localBase = iFH.CN_bit == 0 ? currentConfig : nextConfig;

  if (CAOrgflag == 1)
  {
    const u16 CAOrg = getBits(d, used * 8 + 40, 16);
    (void)CAOrg;
    used += 16 / 8;
  }
  used += 40 / 8;

  // const auto itScd = localBase->serviceCompDescMap.find(SCId);
  const auto pScd = find_service_component(localBase, SCId);

  if (pScd == nullptr)
  {
    return used;
  }

  // We want to have the subchannel OK
  if (localBase->subChDescMapFig0_1.find(SubChId) == localBase->subChDescMapFig0_1.end()) // TODO: why necessary?
  {
    return used;
  }

  // If the component exists, we first look whether is was already handled
  if (pScd->isMadePublic)
  {
    return used;
  }

  // If the Data Service Component Type == 0, we do not deal with it
  if (DSCTy == 0)
  {
    return used;
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
  return used;
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
  DabConfig * localBase = iFH.CN_bit == 0 ? currentConfig : nextConfig;

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

  if (fh.CN_bit == 0)
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

  const u32 SId = getLBits(d, bitOffset, iFH.PD_bit == 1 ? 32 : 16);
  bitOffset += iFH.PD_bit == 1 ? 32 : 16;

  DabConfig * const localBase = iFH.CN_bit == 0 ? currentConfig : nextConfig;

  const u8 extFlag = getBits_1(d, bitOffset);
  const u16 SCIdS = getBits_4(d, bitOffset + 4);
  bitOffset += 8;

  const u8 lsFlag = getBits_1(d, bitOffset);

  if (lsFlag == 0)
  {
    // short form
    const i16 subChId = getBits_6(d, bitOffset + 2);

    if (localBase->subChDescMapFig0_1.find(subChId) != localBase->subChDescMapFig0_1.end()) // TODO: why necessary?
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
  const u32 SId = getLBits(d, bitOffset, iFH.PD_bit == 1 ? 32 : 16);
  DabConfig * localBase = iFH.CN_bit == 0 ? currentConfig : nextConfig;

  bitOffset += iFH.PD_bit == 1 ? 32 : 16;
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
  return bitOffset / 8;
}

//	FEC sub-channel organization 6.2.2
void FibDecoder::FIG0Extension14(const u8 * const d)
{
  const SFigHeader fh = get_fig_header(d);
  i16 used = 2;      // in Bytes

  DabConfig * localBase = fh.CN_bit == 0 ? currentConfig : nextConfig;

  while (used < fh.Length)
  {
    const i16 subChId = getBits_6(d, used * 8);
    const u8 FEC_scheme = getBits_2(d, used * 8 + 6);
    used += 1;

    SSubChannelDescFig0_14 subChannel;
    subChannel.FEC_scheme = FEC_scheme;
    localBase->subChDescMapFig0_14.try_emplace(subChId, subChannel);
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
  DabConfig * localBase = fh.CN_bit == 0 ? currentConfig : nextConfig;

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
  DabConfig * localBase = CN_bit == 0 ? currentConfig : nextConfig;

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

  return upperLimit / 8;
}

//	FIG 1 - Cover the different possible labels, section 5.2
void FibDecoder::process_FIG1(const u8 * const d)
{
  const u8 extension = getBits_3(d, 8 + 5);

  switch (extension)
  {
  case 0:    // ensemble name
    FIG1Extension0(d);
    break;

  case 1:    // service name
    FIG1Extension1(d);
    break;

  case 2:    // obsolete
    break;

  case 3:    // obsolete
    break;

  case 4:    // Service Component Label
    FIG1Extension4(d);
    break;

  case 5:    // Data service label
    FIG1Extension5(d);
    break;

  case 6:    // XPAD label - 8.1.14.4
    FIG1Extension6(d);
    break;

  default:;
  }
}

//	Name of the ensemble
//
void FibDecoder::FIG1Extension0(const u8 * const d)
{

  // from byte 1 we deduce:
  const u8 extension = getBits_3(d, 8 + 5);
  (void)extension;
  const u32 EId = getBits(d, 16, 16);

  QString name;
  if (!extract_character_set_label(name, d, 32))
  {
    return;
  }

  if (!ensemble->namePresent)
  {
    ensemble->ensembleName = name;
    ensemble->ensembleId = EId;
    ensemble->namePresent = true;

    emit signal_name_of_ensemble(EId, name);
  }

  ensemble->isSynced = true;
}

//
//	Name of service
void FibDecoder::FIG1Extension1(const u8 * const d)
{
  const i32 SId = getBits(d, 16, 16);
  const i16 offset = 32;
  const u8 extension = getBits_3(d, 8 + 5);
  (void)extension;

  QString dataName;
  if (!extract_character_set_label(dataName, d, offset))
  {
    return;
  }

  const auto it = find_service_from_service_name(dataName);

  if (it == ensemble->servicesMap.end())
  {
    create_service(dataName, SId, 0);
  }
  else
  {
    it->second.SCIdS = 0;
    it->second.hasName = true;
  }
}

// service component label 8.1.14.3
void FibDecoder::FIG1Extension4(const u8 * const d)
{
  const u8 PD_bit = getBits_1(d, 16);
  // const u8 Rfa = getBits_3(d, 17);
  const u8 SCIdS = getBits_4(d, 20);

  const u32 SId = PD_bit ? getLBits(d, 24, 32) : getLBits(d, 24, 16);
  const i16 offset = PD_bit ? 56 : 40;

  const auto pScd = find_service_component(currentConfig, SId, SCIdS);

  if (pScd != nullptr) // TODO: bug? was > 0?
  {
    QString dataName;
    if (!extract_character_set_label(dataName, d, offset))
    {
      return;
    }

    if (find_service_from_service_name(dataName) == ensemble->servicesMap.end())
    {
      if (pScd->TMid == ETMId::StreamModeAudio)
      {
        create_service(dataName, SId, SCIdS);
        emit signal_add_to_ensemble(dataName, SId);
      }
    }
  }
}

//	Data service label - 32 bits 8.1.14.2
void FibDecoder::FIG1Extension5(const u8 * const d)
{
  const i16 offset = 48;
  const u8 extension = getBits_3(d, 8 + 5);
  const u32 SId = getLBits(d, 16, 32);
  (void)extension;

  const auto it = ensemble->servicesMap.find(SId);

  if (it != ensemble->servicesMap.end())
  {
    return;  // entry already existing
  }

  QString serviceName;
  if (!extract_character_set_label(serviceName, d, offset))
  {
    return;
  }

  create_service(serviceName, SId, 0);
}

//	XPAD label - 8.1.14.4
void FibDecoder::FIG1Extension6(const u8 * const d)
{
  const u8 PD_bit = getBits_1(d, 16);
  const u8 Rfu = getBits_3(d, 17);
  (void)Rfu;
  const u8 SCIdS = getBits_4(d, 20);

  u32 SId = 0;
  i16 offset = 0;
  u8 XPAD_apptype;

  if (PD_bit)
  {  // 32 bits identifier for XPAD label
    SId = getLBits(d, 24, 32);
    XPAD_apptype = getBits_5(d, 59);
    offset = 64;
  }
  else
  {  // 16 bit identifier for XPAD label
    SId = getLBits(d, 24, 16);
    XPAD_apptype = getBits_5(d, 43);
    offset = 48;
  }

  (void)SId;
  (void)SCIdS;
  (void)XPAD_apptype;
  (void)offset;
}

//	Programme Type (PTy) 8.1.5
/////////////////////////////////////////////////////////////////////////
//	Support functions
//
//	bind_audioService is the main processor for - what the name suggests -
//	connecting the description of audioservices to a SID
//	by creating a service Component
void FibDecoder::bind_audio_service(DabConfig * iopDabConfig, const ETMId iTMid, const u32 iSId, const i16 iCompNr,
                                                             const i16 iSubChId, const i16 iPsFlag, const i16 iASCTy)
{
  const auto itServ = ensemble->servicesMap.find(iSId);

  if (itServ == ensemble->servicesMap.end())
  {
    return;
  }

  //	if (ensemble -> services [serviceIndex]. programType == 0)
  //	   return;

  if (iopDabConfig->subChDescMapFig0_1.find(iSubChId) == iopDabConfig->subChDescMapFig0_1.end()) // TODO: why necessary
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

  register_service_comp_to_maps(pScd);

  const QString dataName = itServ->second.serviceLabel;

  emit signal_add_to_ensemble(dataName, iSId);

  itServ->second.is_shown = true;
}

//      bind_packetService is the main processor for - what the name suggests -
//      connecting the service component defining the service to the SId,
//	So, here we create a service component. Note however,
//	that FIG0/3 provides additional data, after that we
//	decide whether it should be visible or not
void FibDecoder::bind_packet_service(DabConfig * iopDabConfig, const ETMId iTMId, const u32 iSId, const i16 iCompNr,
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

  register_service_comp_to_maps(pScd);
}

void FibDecoder::register_service_comp_to_maps(const DabConfig::TSPServiceCompDesc & ipScp)
{

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
DabConfig::TSPServiceCompDesc FibDecoder::find_service_component(DabConfig * db, i16 SCId) const
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
DabConfig::TSPServiceCompDesc FibDecoder::find_service_component(DabConfig * ipDabConfig, u32 iSId, u8 iSCIdS) const
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
DabConfig::TSPServiceCompDesc FibDecoder::find_component(DabConfig * ipDabConfig, u32 iSId, i16 iSubChId) const
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

    if (find_service_component(currentConfig, SId, SCIdS) == nullptr)
    {
      qDebug() << "clean up service ID" << SId << "SCIdS" << SCIdS;
      it = ensemble->servicesMap.erase(it);
    }
  }
}

QString FibDecoder::get_announcement_type_str(u16 a)
{
  switch (a)
  {
  case   0:
  default:  return QString("Alarm");
  case   1: return QString("Road Traffic Flash");
  case   2: return QString("Traffic Flash");
  case   4: return QString("Warning/Service");
  case   8: return QString("News Flash");
  case  16: return QString("Area Weather flash");
  case  32: return QString("Event announcement");
  case  64: return QString("Special Event");
  case 128: return QString("Programme Information");
  }
}

void FibDecoder::set_cluster(DabConfig * localBase, i32 clusterId, u32 iSId, u16 asuFlags)
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

Cluster * FibDecoder::get_cluster(DabConfig * localBase, i16 clusterId)
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
  fibLocker.lock();
  currentConfig->reset();
  nextConfig->reset();
  ensemble->reset();
  connect(this, &FibDecoder::signal_add_to_ensemble, myRadioInterface, &DabRadio::slot_add_to_ensemble);
  fibLocker.unlock();
}

void FibDecoder::disconnect_channel()
{
  fibLocker.lock();
  disconnect(this, &FibDecoder::signal_add_to_ensemble, myRadioInterface, &DabRadio::slot_add_to_ensemble);
  currentConfig->reset();
  nextConfig->reset();
  ensemble->reset();
  fibLocker.unlock();
}

bool FibDecoder::sync_reached() const
{
  return ensemble->isSynced;
}

i32 FibDecoder::get_sub_channel_id(const QString & s, u32 dummy_SId) const
{
  const auto itService = find_service_from_service_name(s);

  if (itService == ensemble->servicesMap.end())  // this test was missed before so it should happen
  {
    qWarning() << "Service not found";
    return -1;
  }

  (void)dummy_SId;
  fibLocker.lock();

  const i32 SId = itService->second.SId;
  const i32 SCIdS = itService->second.SCIdS;

  const i32 compIndex = find_service_component(currentConfig, SId, SCIdS);

  if (compIndex == -1)
  {
    fibLocker.unlock();
    return -1;
  }

  i32 subChId = currentConfig->serviceComps[compIndex].subChannelId;
  fibLocker.unlock();
  return subChId;
}

void FibDecoder::get_data_for_audio_service(const QString & iS, AudioData * opAD) const
{
  opAD->defined = false;

  const auto itService = find_service_from_service_name(iS);

  if (itService == ensemble->servicesMap.end())
  {
    return;
  }

  fibLocker.lock();

  const i32 SId = itService->second.SId;
  const i32 SCIdS = itService->second.SCIdS;

  const i32 compIndex = find_service_component(currentConfig, SId, SCIdS);

  if (compIndex == -1)
  {
    fibLocker.unlock();
    return;
  }

  if (currentConfig->serviceComps[compIndex].TMid != ETMId::StreamModeAudio)
  {
    fibLocker.unlock();
    return;
  }

  i32 subChId = currentConfig->serviceComps[compIndex].subChannelId;

  const auto it = currentConfig->subChDescMapFig0_1.find(subChId);

  if (it == currentConfig->subChDescMapFig0_1.end())
  {
    fibLocker.unlock();
    return;
  }

  opAD->SId = SId;
  opAD->SCIdS = SCIdS;
  opAD->subchId = subChId;
  opAD->serviceName = iS;
  opAD->startAddr = it->second.startAddr;
  opAD->shortForm = it->second.shortForm;
  opAD->protLevel = it->second.protLevel;
  opAD->length = it->second.Length;
  opAD->bitRate = it->second.bitRate;
  opAD->ASCTy = currentConfig->serviceComps[compIndex].ASCTy;
  opAD->language = itService->second.language;
  opAD->programType = itService->second.programType;
  opAD->fmFrequency = itService->second.fmFrequency;
  opAD->defined = true;

  fibLocker.unlock();
}

void FibDecoder::get_data_for_packet_service(const QString & iS, PacketData * opPD, i16 iSCIdS) const
{
  opPD->defined = false;
  const auto itService = find_service_from_service_name(iS);

  if (itService == ensemble->servicesMap.end())
  {
    return;
  }

  fibLocker.lock();

  const i32 SId = itService->second.SId;
  const i32 compIndex = find_service_component(currentConfig, SId, iSCIdS);

  if ((compIndex == -1) || (currentConfig->serviceComps[compIndex].TMid != ETMId::PacketModeData))
  {
    fibLocker.unlock();
    return;
  }

  i32 subChId = currentConfig->serviceComps[compIndex].subChannelId;

  const auto itFig0_1  = currentConfig->subChDescMapFig0_1.find(subChId);
  const auto itFig0_14 = currentConfig->subChDescMapFig0_14.find(subChId);

  if (itFig0_1 == currentConfig->subChDescMapFig0_1.end())
  {
    fibLocker.unlock();
    return;
  }

  opPD->serviceName = iS;
  opPD->SId = SId;
  opPD->SCIdS = iSCIdS;
  opPD->subchId = subChId;
  opPD->startAddr = itFig0_1->second.startAddr;
  opPD->shortForm = itFig0_1->second.shortForm;
  opPD->protLevel = itFig0_1->second.protLevel;
  opPD->length = itFig0_1->second.Length;
  opPD->bitRate = itFig0_1->second.bitRate;
  opPD->FEC_scheme = itFig0_14 != currentConfig->subChDescMapFig0_14.end() ? itFig0_14->second.FEC_scheme : 0;
  opPD->DSCTy = currentConfig->serviceComps[compIndex].DSCTy;
  opPD->DGflag = currentConfig->serviceComps[compIndex].DgFlag;
  opPD->packetAddress = currentConfig->serviceComps[compIndex].packetAddress;
  opPD->compnr = currentConfig->serviceComps[compIndex].componentNr;
  opPD->appType = currentConfig->serviceComps[compIndex].appType;
  opPD->defined = true;

  fibLocker.unlock();
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
  static constexpr SSubChannelDescFig0_1 subChannel{};

  const auto it = currentConfig->subChDescMapFig0_1.find(iSubChId);

  d->in_use = it != currentConfig->subChDescMapFig0_1.end();

  const auto & scd = d->in_use ? it->second : subChannel;

  d->id = scd.SubChId;
  d->start_cu = scd.startAddr;
  d->protlev = scd.protLevel;
  d->size = scd.Length;
  d->bitrate = scd.bitRate;
  d->uepFlag = scd.shortForm;
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

