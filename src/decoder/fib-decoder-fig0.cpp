/*
 * Copyright (c) 2025 by Thomas Neder (https://github.com/tomneda)
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "fib-decoder.h"
#include "bit-extractors.h"
#include "fib-table.h"
#include <QDebug>
#include <QTimer>


void FibDecoder::_process_Fig0(const u8 * const d)
{
  const SFigHeader & fh = _get_fig_header(d);
  const u8 extension = getBits_5(d, 8 + 3);  // skip FIG header, C/N, OE and P/D flags

  // MCI: Multiplex Configuration Information
  // SI:  Service Information

  /* RepTy: Repetition Type:
    A) 10 times per second;
    B) once per second;
    C) once every 10 seconds;
    D) less frequently than every 10 seconds;
    E) all information within 2 minutes.
    F) all information within 1 minutes.
    ?) no information/sporadic occurrence
  */

  switch (extension)
  {
  case  0: _process_Fig0s0(d);  break;  // MCI, RepTyA,   ensemble information (6.4.1)
  case  1: _process_fig0_loop(d, fh, &FibDecoder::_subprocess_Fig0s1); break;  // MCI, RepTyA,   sub-channel organization (6.2.1)
  case  2: _process_fig0_loop(d, fh, &FibDecoder::_subprocess_Fig0s2);  break;  // MCI, RepTyA,   service organization (6.3.1)
  case  3: _process_fig0_loop(d, fh, &FibDecoder::_subprocess_Fig0s3);  break;  // MCI, RepTyA,   service component in packet mode (6.3.2)
//case  4:                      break;  // MCI, RepTy?,   service component with CA (6.3.3)
  case  5: _process_fig0_loop(d, fh, &FibDecoder::_subprocess_Fig0s5);  break;  // SI,  RepTyA/B, service component language (8.1.2)
//case  6:                      break;  // SI,  RepTyC,   service linking information (8.1.15)
  case  7: _process_Fig0s7(d);  break;  // MCI, RepTyB,   configuration information (6.4.2)
  case  8: _process_fig0_loop(d, fh, &FibDecoder::_subprocess_Fig0s8);  break;  // MCI, RepTyB,   service component global definition (6.3.5)
  case  9: _process_Fig0s9(d);  break;  // SI,  RepTyB/C, country, LTO & international table (8.1.3.2)
  case 10: _process_Fig0s10(d); break;  // SI,  RepTyB/C, date and time (8.1.3.1)
  case 13: _process_fig0_loop(d, fh, &FibDecoder::_subprocess_Fig0s13); break;  // MCI, RepTyB,   user application information (6.3.6)
  case 14: _process_fig0_loop(d, fh, &FibDecoder::_subprocess_Fig0s14); break;  // MCI, RepTyB?,  FEC subchannel organization (6.2.2)
  case 17: _process_Fig0s17(d); break;  // SI,  RepTyA/B, programme type (8.1.5)
  case 18: _process_Fig0s18(d); break;  // SI,  RepTyB,   announcement support (8.1.6.1)
  case 19: _process_Fig0s19(d); break;  // SI,  RepTyA/B, announcement switching (8.1.6.2)
//case 20:                      break;  // SI,  RepTyE,   service component information (8.1.4)
//case 21: _process_Fig0s21(d); break;  // SI,  RepTyE,   frequency information (8.1.8)
//case 24:                      break;  // SI,  RepTy?,   OE services (8.1.10)
//case 25:                      break;  // SI,  RepTyB,   OE announcement support (8.1.6.3)
//case 26:                      break;  // SI,  RepTyA/B, OE announcement switching (8.1.6.4)
  default:
    if (mUnhandledFig0Set.find(extension) == mUnhandledFig0Set.end()) // print message only once
    {
      const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - mLastTimePoint); // see issue https://github.com/tomneda/DABstar/issues/99
      if (mFibLoadingState >= EFibLoadingState::S5_DeferredDataLoaded) qDebug().noquote() << QString("FIG 0/%1 not handled (received after %2 ms after service start trigger)").arg(extension).arg(diff.count()); // print only if the summarized print was already done
      mUnhandledFig0Set.emplace(extension);
    }
  }

  if (mSIdForFastAudioSelection > 0 && (extension == 1 || extension == 2))
  {
    _process_fast_audio_selection();
  }
}

// Ensemble information, 6.4.1.
void FibDecoder::_process_Fig0s0(const u8 * const d)
{
  SFig0s0_EnsembleInformation fig0s0;
  fig0s0.EId = getBits(d, 16, 16);
  fig0s0.ChangeFlags = getBits_2(d, 16 + 16);
  fig0s0.AlarmFlag = getBits_1(d, 16 + 16 + 2);
  fig0s0.CIFCountHi = getBits_5(d, 16 + 19);
  fig0s0.CIFCountLo = getBits_8(d, 16 + 24);
  fig0s0.OccurrenceChange = getBits_8(d, 16 + 32);

  mCifCount_hi = fig0s0.CIFCountHi;
  mCifCount_lo = fig0s0.CIFCountLo;
  mCifCount = fig0s0.CIFCountHi * 250 + fig0s0.CIFCountLo;

  if (fig0s0.ChangeFlags == 0 && mPrevChangeFlag == 3)
  {
    qDebug() << "Change FIB configuration";
    std::swap(mpFibConfigFig0Curr, mpFibConfigFig0Next);
    mpFibConfigFig0Next->reset();
    // _reset(); // TODO: what has to be reset here
    emit signal_change_in_configuration();
  }

  mPrevChangeFlag = fig0s0.ChangeFlags;
}

i16 FibDecoder::_process_fig0_loop(const u8 * const d, const SFigHeader & iFH, i16 (FibDecoder::*fn)(const u8 *, i16, const SFigHeader &))
{
  i16 used = 2;
  while (used <= iFH.Length) // one byte in "used" is already included in the length
  {
    used = (this->*fn)(d, used, iFH);
  }
  return used;
}

// Basic Sub Channels Organization 6.2.1
i16 FibDecoder::_subprocess_Fig0s1(const u8 * const d, const i16 offset, const SFigHeader & iFH)
{
  i16 bitOffset = offset * 8;
  FibConfigFig0::SFig0s1_BasicSubChannelOrganization fig0s1;

  fig0s1.SubChId = getBits_6(d, bitOffset);

  FibConfigFig0 * const pConfig = _get_config_ptr(iFH.CN_Flag);

  if (const auto * const pFig0s1 = pConfig->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(fig0s1.SubChId);
      pFig0s1 == nullptr)
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
        if (fig0s1.SubChannelSize % table[fig0s1.ProtectionLevel] != 0)
        {
          qWarning() << "SubChannelSize" << fig0s1.SubChannelSize << "not divisible by" << table[fig0s1.ProtectionLevel];
        }
        fig0s1.BitRate = fig0s1.SubChannelSize / table[fig0s1.ProtectionLevel] * 8;
      }
      else if (fig0s1.Option == 0x1)
      {
        static constexpr i32 table[] = { 27, 21, 18, 15 };
        if (fig0s1.SubChannelSize % table[fig0s1.ProtectionLevel] != 0)
        {
          qWarning() << "SubChannelSize" << fig0s1.SubChannelSize << "not divisible by" << table[fig0s1.ProtectionLevel];
        }
        fig0s1.BitRate = fig0s1.SubChannelSize / table[fig0s1.ProtectionLevel] * 32;
        fig0s1.ProtectionLevel += (1 << 2); // TODO: (1 << 2) this is used to retrieve the option state in later stages, make this nicer
      }
      else qWarning() << "Option" << fig0s1.Option << "not supported";
      bitOffset += 32;
    }
    fig0s1.set_current_time();
    pConfig->Fig0s1_BasicSubChannelOrganizationVec.emplace_back(fig0s1);
    _retrigger_timer_data_loaded_fast("Fig0s1");
  }
  else
  {
    bitOffset += pFig0s1->ShortForm ? 24 : 32;
    const_cast<FibConfigFig0::SFig0s1_BasicSubChannelOrganization *>(pFig0s1)->set_current_time_2nd_call();
  }
  return (i16)(bitOffset / 8);  // we return bytes
}

// Service organization, 6.3.1. Bind channels to SIds.
i16 FibDecoder::_subprocess_Fig0s2(const u8 * const d, const i16 offset, const SFigHeader & iFH)
{
  i16 bitOffset = 8 * offset;
  FibConfigFig0::SFig0s2_BasicService_ServiceCompDef fig0s2;

  fig0s2.PD_Flag = iFH.PD_Flag;
  const u32 SId = (iFH.PD_Flag == 1 ? fig0s2.PD1.SId = getLBits(d, bitOffset, 32) // SId overlaps ecc and CountryId
                                    : fig0s2.PD0.SId = getLBits(d, bitOffset, 16));
  bitOffset += (iFH.PD_Flag == 1 ? 32 : 16);

  FibConfigFig0 * const pConfig = _get_config_ptr(iFH.CN_Flag);

  fig0s2.NumServiceComp = getBits_4(d, bitOffset + 4);
  bitOffset += 8;

  for (i16 servCombIdx = 0; servCombIdx < fig0s2.NumServiceComp; servCombIdx++)
  {
    if (const auto * pFig0s2 = pConfig->get_Fig0s2_BasicService_ServiceCompDef_of_SId_ScIdx(SId, servCombIdx);
        pFig0s2 == nullptr) // is SID on same index already known?
    {
      FibConfigFig0::SFig0s2_ServiceComponentDefinition & fig0s2_scd = fig0s2.ServiceComp_C;
      fig0s2.ServiceComp_C_index = servCombIdx;
      fig0s2_scd.TMId = getBits_2(d, bitOffset);

      if ((ETMId)fig0s2_scd.TMId == ETMId::StreamModeAudio)
      {
        // Audio
        fig0s2_scd.TMId00.ASCTy = getBits_6(d, bitOffset + 2);
        fig0s2_scd.TMId00.SubChId = getBits_6(d, bitOffset + 8);
      }
      else if ((ETMId)fig0s2_scd.TMId == ETMId::StreamModeData)
      {
        // MSC stream mode data
        fig0s2_scd.TMId01.DSCTy = getBits_6(d, bitOffset + 2);
        fig0s2_scd.TMId01.SubChId = getBits_6(d, bitOffset + 8);
      }
      else if ((ETMId)fig0s2_scd.TMId == ETMId::PacketModeData)
      {
        // MSC packet data
        fig0s2_scd.TMId11.SCId = getBits(d, bitOffset + 2, 12);
      }
      else
      {
        qWarning() << "TMid" << fig0s2_scd.TMId << "not supported";
      }
      fig0s2_scd.PS_Flag = getBits_1(d, bitOffset + 14);
      fig0s2_scd.CA_Flag = getBits_1(d, bitOffset + 15);

      // SId_element.comps.push_back(pConfig->Fig0s2_BasicService_ServiceCompDefVec.size());
      fig0s2.set_current_time();
      pConfig->Fig0s2_BasicService_ServiceCompDefVec.emplace_back(fig0s2);
      _retrigger_timer_data_loaded_fast("Fig0s2");
    }
    else
    {
      const_cast<FibConfigFig0::SFig0s2_BasicService_ServiceCompDef *>(pFig0s2)->set_current_time_2nd_call();
    }
    bitOffset += 16;
  }
  assert(bitOffset % 8 == 0); // only full bytes should occur
  return bitOffset / 8;    // in Bytes
}

// Service component in packet mode 6.3.2.
i16 FibDecoder::_subprocess_Fig0s3(const u8 * const d, const i16 used, const SFigHeader & iFH)
{
  i16 bitOffset = used * 8;
  FibConfigFig0::SFig0s3_ServiceComponentPacketMode fig0s3;

  fig0s3.SCId = getBits(d, bitOffset, 12);

  auto * const pConfig = _get_config_ptr(iFH.CN_Flag);

  if (const auto * const pFig0s3 = pConfig->get_Fig0s3_ServiceComponentPacketMode_of_SCId(fig0s3.SCId);
      pFig0s3 == nullptr)
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
    fig0s3.set_current_time();
    pConfig->Fig0s3_ServiceComponentPacketModeVec.emplace_back(fig0s3);
    _retrigger_timer_data_loaded_slow("Fig0s3");
  }
  else
  {
    bitOffset += 40 + (pFig0s3->CAOrg_Flag != 0 ? 16 : 0);
    const_cast<FibConfigFig0::SFig0s3_ServiceComponentPacketMode *>(pFig0s3)->set_current_time_2nd_call();
  }

  assert(bitOffset % 8 == 0); // only full bytes should occur
  return bitOffset / 8;
}

// Service component language 8.1.2
i16 FibDecoder::_subprocess_Fig0s5(const u8 * const d, const i16 offset, const SFigHeader & /*iFH*/)
{
  i16 bitOffset = offset * 8;

  FibConfigFig0::SFig0s5_ServiceComponentLanguage fig0s5;
  fig0s5.LS_Flag = getBits_1(d, bitOffset);

  if (fig0s5.LS_Flag == 0)
  {
    // short form
    fig0s5.SubChId = getBits_6(d, bitOffset + 2);
    fig0s5.Language = getBits_8(d, bitOffset + 8);

    if (const auto * const pFig0s5 = mpFibConfigFig0Curr->get_Fig0s5_ServiceComponentLanguage_of_SubChId(fig0s5.SubChId);
        pFig0s5 == nullptr)
    {
      fig0s5.set_current_time();
      mpFibConfigFig0Curr->Fig0s5_ServiceComponentLanguageVec.emplace_back(fig0s5); // TODO: really only currentConfig? (see 8.1.2)
      _retrigger_timer_data_loaded_slow("Fig0s5a");
    }
    else
    {
      const_cast<FibConfigFig0::SFig0s5_ServiceComponentLanguage *>(pFig0s5)->set_current_time_2nd_call();
    }

    bitOffset += 16;
  }
  else
  {
    // long form
    fig0s5.SCId = getBits(d, bitOffset + 4, 12);
    fig0s5.Language = getBits_8(d, bitOffset + 16);

    if (const auto * const pFig0s5 = mpFibConfigFig0Curr->get_Fig0s5_ServiceComponentLanguage_of_SCId(fig0s5.SCId);
        pFig0s5 == nullptr)
    {
      fig0s5.set_current_time();
      mpFibConfigFig0Curr->Fig0s5_ServiceComponentLanguageVec.emplace_back(fig0s5); // TODO: really only currentConfig? (see 8.1.2)
      _retrigger_timer_data_loaded_slow("Fig0s5b");
    }
    else
    {
      const_cast<FibConfigFig0::SFig0s5_ServiceComponentLanguage *>(pFig0s5)->set_current_time_2nd_call();
    }

    bitOffset += 24;
  }

  assert(bitOffset % 8 == 0); // only full bytes should occur
  return bitOffset / 8;
}

// Configuration information 6.4.2
void FibDecoder::_process_Fig0s7(const u8 * const d)
{
  i16 used = 2; // offset in bytes
  const SFigHeader fh = _get_fig_header(d);
  auto * const pConfig = _get_config_ptr(fh.CN_Flag);

  if (const auto * const pFig0s7 = pConfig->get_Fig0s7_ConfigurationInformation();
      pFig0s7 == nullptr)
  {
    FibConfigFig0::SFig0s7_ConfigurationInformation fig0s7;
    fig0s7.NumServices = getBits_6(d, used * 8);
    fig0s7.Count = getBits(d, used * 8 + 6, 10);
    fig0s7.set_current_time();
    pConfig->Fig0s7_ConfigurationInformationVec.emplace_back(fig0s7);
    _retrigger_timer_data_loaded_fast("Fig0s7");
  }
  else
  {
    const_cast<FibConfigFig0::SFig0s7_ConfigurationInformation *>(pFig0s7)->set_current_time_2nd_call(); // won't give up the "const" of the get_..-method
  }
}

// Service Component Global Definition 6.3.5
i16 FibDecoder::_subprocess_Fig0s8(const u8 * const d, const i16 used, const SFigHeader & iFH)
{
  i16 bitOffset = used * 8;
  const u32 SId = getLBits(d, bitOffset, iFH.PD_Flag == 1 ? 32 : 16);
  bitOffset += iFH.PD_Flag == 1 ? 32 : 16;

  FibConfigFig0::SFig0s8_ServiceCompGlobalDef fig0s8;
  fig0s8.SId = SId;
  fig0s8.PD_Flag = iFH.PD_Flag;
  fig0s8.LS_Flag = getBits_1(d, bitOffset + 8);
  fig0s8.Ext_Flag = getBits_1(d, bitOffset);
  fig0s8.SCIdS = getBits_4(d, bitOffset + 4);

  if (fig0s8.LS_Flag == 0) // short form
  {
    fig0s8.SubChId = getBits_6(d, bitOffset + 4 + 4 + 2);
    bitOffset += (fig0s8.Ext_Flag != 0 ? 16 + 8 : 16); // skip Rfa
  }
  else // long form
  {
    fig0s8.SCId = getBits(d, bitOffset + 4 + 4 + 1 + 3, 12);
    bitOffset += (fig0s8.Ext_Flag != 0 ? 24 + 8 : 24); // skip Rfa
  }

  auto * const pConfig = _get_config_ptr(iFH.CN_Flag);

  if (const auto * const pFig0s8 = pConfig->get_Fig0s8_ServiceCompGlobalDef_of_SId_SCIdS(fig0s8.SId, fig0s8.SCIdS);
      pFig0s8 == nullptr)
  {
    fig0s8.set_current_time();
    pConfig->Fig0s8_ServiceCompGlobalDefVec.emplace_back(fig0s8);
    _retrigger_timer_data_loaded_slow("Fig0s8");
  }
  else
  {
    const_cast<FibConfigFig0::SFig0s8_ServiceCompGlobalDef *>(pFig0s8)->set_current_time_2nd_call();
  }

  assert(bitOffset % 8 == 0); // only full bytes should occur
  return bitOffset / 8;
}

// Country, LTO & international table 8.1.3.2
void FibDecoder::_process_Fig0s9(const u8 * const d)
{
  constexpr i16 offset = 16;

  if (!mpFibConfigFig0Curr->Fig0s9_CountryLtoInterTabVec.empty()) // TODO: considering some change triggers?
  {
    mpFibConfigFig0Curr->Fig0s9_CountryLtoInterTabVec[0].set_current_time_2nd_call();
    return;
  }

  FibConfigFig0::SFig0s9_CountryLtoInterTab fig0s9;
  fig0s9.Ext_Flag = getBits_1(d, offset);
  fig0s9.Ensemble_LTO = getBits_5(d, offset + 3); // ignore signbit here
  const bool signBit = getBits_1(d, offset + 2) == 1;
  if (signBit) fig0s9.Ensemble_LTO *= -1;
  fig0s9.LTO_minutes = fig0s9.Ensemble_LTO * 30;
  fig0s9.Ensemble_ECC = getBits_8(d, offset + 8);
  fig0s9.InterTableId = getBits_8(d, offset + 16);

  fig0s9.set_current_time();
  mpFibConfigFig0Curr->Fig0s9_CountryLtoInterTabVec.emplace_back(fig0s9);
  _retrigger_timer_data_loaded_slow("Fig0s9");
}

void FibDecoder::_process_Fig0s10(const u8 * dd)
{
  constexpr i16 offset = 16;
  FibConfigFig0::SFig0s10_DateAndTime fig0s10;

  fig0s10.MJD = getLBits(dd, offset + 1, 17);
  fig0s10.LSI = getBits_1(dd, offset + 1 + 17);
  fig0s10.UTC_Flag = getBits_1(dd, offset + 1 + 17 + 1 + 1); // + Rfa
  fig0s10.UTC = getLBits(dd, offset + 1 + 17 + 1 + 1 + 1, (fig0s10.UTC_Flag == 1 ? 27 : 11));

  mModJulianDate = fig0s10.MJD;

  mUtcTimeSet.ModJulianDate = fig0s10.MJD;

  if (fig0s10.UTC_Flag == 1)
  {
    mUtcTimeSet.Hour = fig0s10.UTC1.Hours;
    mUtcTimeSet.Minute = fig0s10.UTC1.Minutes;
    mUtcTimeSet.Second = fig0s10.UTC1.Seconds;
  }
  else
  {
    mUtcTimeSet.Hour = fig0s10.UTC0.Hours;
    mUtcTimeSet.Minute = fig0s10.UTC0.Minutes;
    mUtcTimeSet.Second = -1; // -1: no seconds given (is hidden in time display)
  }

  if (const auto * const pFig0s9 = mpFibConfigFig0Curr->get_Fig0s9_CountryLtoInterTab();
      pFig0s9 != nullptr)
  {
    mUtcTimeSet.LTO_Minutes = pFig0s9->LTO_minutes;
    emit signal_fib_time_info(mUtcTimeSet);
  }
}

// User Application Information 6.3.6
i16 FibDecoder::_subprocess_Fig0s13(const u8 * const d, i16 used, const SFigHeader & iFH)
{
  i16 bitOffset = used * 8;
  FibConfigFig0::SFig0s13_UserApplicationInformation fig0s13;
  fig0s13.SId = getLBits(d, bitOffset, iFH.PD_Flag == 1 ? 32 : 16);
  bitOffset += iFH.PD_Flag == 1 ? 32 : 16;
  fig0s13.SCIdS = getBits_4(d, bitOffset);

  if (const auto pFig0s13 = mpFibConfigFig0Curr->get_Fig0s13_UserApplicationInformation_of_SId_SCIdS(fig0s13.SId, fig0s13.SCIdS);
      pFig0s13 == nullptr)
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
      FibConfigFig0::SFig0s13_UserApplicationInformation::SUserApp & userApp = fig0s13.UserAppVec[i];
      userApp.UserAppType = getBits(d, bitOffset, 11);
      userApp.UserAppDataLength = getBits_5(d, bitOffset + 11); // in bytes
      bitOffset += (11 + 5 + 8 * userApp.UserAppDataLength);
    }

    fig0s13.SizeBits = bitOffset - used * 8; // only store netto size
    auto * const pConfig = _get_config_ptr(iFH.CN_Flag);
    fig0s13.set_current_time();
    pConfig->Fig0s13_UserApplicationInformationVec.emplace_back(fig0s13);
    _retrigger_timer_data_loaded_slow("Fig0s13");
  }
  else
  {
    bitOffset = pFig0s13->SizeBits + used * 8;
    const_cast<FibConfigFig0::SFig0s13_UserApplicationInformation *>(pFig0s13)->set_current_time_2nd_call();
  }

  assert(bitOffset % 8 == 0); // only full bytes should occur
  return bitOffset / 8;
}

// FEC sub-channel organization 6.2.2
i16 FibDecoder::_subprocess_Fig0s14(const u8 * const d, const i16 used, const SFigHeader & fh)
{
  FibConfigFig0 * const pConfig = _get_config_ptr(fh.CN_Flag);
  FibConfigFig0::SFig0s14_SubChannelOrganization fig0s14;
  fig0s14.SubChId = getBits_6(d, used * 8);

  if (const auto * const pFig0s14 = pConfig->get_Fig0s14_SubChannelOrganization_of_SubChId(fig0s14.SubChId);
      pFig0s14 == nullptr)
  {
    fig0s14.FEC_scheme = getBits_2(d, used * 8 + 6);
    fig0s14.set_current_time();
    pConfig->Fig0s14_SubChannelOrganizationVec.emplace_back(fig0s14);
    _retrigger_timer_data_loaded_slow("Fig0s14");
  }
  else
  {
    const_cast<FibConfigFig0::SFig0s14_SubChannelOrganization *>(pFig0s14)->set_current_time_2nd_call();
  }

  return used + 1;
}

void FibDecoder::_process_Fig0s17(const u8 * const d)
{
  const i16 length = getBits_5(d, 3);
  i16 offset = 16;

  while (offset < length * 8)
  {
    FibConfigFig0::SFig0s17_ProgrammeType fig0s17;
    fig0s17.SId = getBits(d, offset, 16);

    // TODO: needs special handling for SD_Flag == 1?
    if (const auto * const pFig0s17 = mpFibConfigFig0Curr->get_Fig0s17_ProgrammeType_of_SId(fig0s17.SId);
        pFig0s17 == nullptr)
    {
      fig0s17.SD_Flag = getBits_1(d, offset + 16);
      fig0s17.IntCode = getBits_5(d, offset + 16 + 11);
      fig0s17.set_current_time();
      mpFibConfigFig0Curr->Fig0s17_ProgrammeTypeVec.emplace_back(fig0s17);
      // qDebug() << "Fig0s17: SId" << fig0s17.SId << "IntCode" << fig0s17.IntCode;
      _retrigger_timer_data_loaded_slow("Fig0s17");
    }
    else
    {
      const_cast<FibConfigFig0::SFig0s17_ProgrammeType *>(pFig0s17)->set_current_time_2nd_call();
    }

    offset += 32;
  }
}

// Announcement support 8.1.6.1
void FibDecoder::_process_Fig0s18(const u8 * const d)
{
  const SFigHeader fh = _get_fig_header(d);
  constexpr i16 used = 2;  // in Bytes
  i16 bitOffset = used * 8;

  FibConfigFig0 * const localBase = _get_config_ptr(fh.CN_Flag);

  while (bitOffset <= fh.Length * 8) // one byte in "used" is already included in the length
  {
    const u16 SId = getBits(d, bitOffset, 16);
    bitOffset += 16;
    const u16 asuFlags = getBits(d, bitOffset, 16);
    bitOffset += 16;
    const u8 nrClusters = getBits(d, bitOffset + 5, 3);
    bitOffset += 8;

    for (i32 i = 0; i < nrClusters; i++)
    {
      if (getBits(d, bitOffset + 8 * i, 8) == 0)
      {
        continue;
      }

      // if (it != ensemble->servicesMap.end() && it->second.hasName)
      {
        _set_cluster(localBase, getBits(d, bitOffset + 8 * i, 8), SId, asuFlags);
      }
    }
    bitOffset += nrClusters * 8;
  }
}

// Announcement switching 8.1.6.2
void FibDecoder::_process_Fig0s19(const u8 * const d)
{
  const SFigHeader fh = _get_fig_header(d);
  i16 used = 2;      // in Bytes
  i16 bitOffset = used * 8;

  FibConfigFig0 * const localBase = _get_config_ptr(fh.CN_Flag);

  while (bitOffset <= fh.Length * 8) // one byte in "used" is already included in the length
  {
    const u8 clusterId = getBits(d, bitOffset, 8);
    bitOffset += 8;
    const u16 AswFlags = getBits(d, bitOffset, 16);
    bitOffset += 16;
    const u8 newFlag = getBits(d, bitOffset, 1);
    (void)newFlag;
    bitOffset += 1;
    const u8 regionFlag = getBits(d, bitOffset, 1);
    bitOffset += 1;
    const u8 subChId = getBits(d, bitOffset, 6);
    bitOffset += 6;
    if (regionFlag == 1)
    {
      bitOffset += 2;  // skip Rfa
      const u8 regionId = getBits(d, bitOffset, 6);
      bitOffset += 6;
      (void)regionId;
    }

    if (mFibLoadingState < EFibLoadingState::S4_FullyPacketDataLoaded)
    {
      return;
    }

    Cluster * myCluster = _get_cluster(localBase, clusterId);

    if (myCluster == nullptr)
    {  // should not happen
      return;
    }

    if ((myCluster->flags & AswFlags) != 0)
    {
      myCluster->announcing++;

      if (myCluster->announcing == 5)
      {
        for (u16 i = 0; i < (u16)myCluster->servicesSIDs.size(); i++)
        {
          const u32 SId = myCluster->servicesSIDs[i];
          const QString name = mpFibConfigFig1->get_service_label_of_SId_from_all_Fig1(SId);
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
          const QString name = mpFibConfigFig1->get_service_label_of_SId_from_all_Fig1(SId);
          emit signal_stop_announcement(name, subChId);
        }
      }
    }
  }
}

// Frequency information (FI) 8.1.8
i16 FibDecoder::_subprocess_Fig0s21(const u8 * const d, const i16 used, const SFigHeader & iFH) const
{
  const i16 offset = used * 8;
  const i16 LengthFiList = getBits_5(d, offset + 11);
  i16 startOffset = offset + 16;
  const i16 upperOffset = startOffset + LengthFiList * 8;

  while (startOffset < upperOffset)
  {
    const u16 IdField = getBits(d, startOffset, 16);
    const u8  RandM = getBits_4(d, startOffset + 16);
    const u8  ContFlag = getBits_1(d, startOffset + 20);
    const u8  LengthFreqList = getBits_3(d, startOffset + 21);
    startOffset += 24; // Header

    QString str;
    QTextStream ts(&str);

    ts << "SIV " << iFH.CN_Flag << " OE " << iFH.OE_Flag << " RdM " << RandM << " CF " << ContFlag << " LFL " << LengthFreqList;
    //sl << QString("SIV %1 OE %2 R&M %3 ContFlag %4 Lenght %5 FreqList %6").arg(iFH.CN_Flag).arg(iFH.OE_Flag).arg(RandM).arg(ContFlag).arg(LengthFreqList);

    // TODO: LengthFreqList in length in bytes NOT numbers of frequency elements

    switch (RandM)
    {
    case 0b0000:
      ts << " 'DAB-Ens' EId " << hex_str(IdField);
      for (u8 idx = 0; idx < LengthFreqList; idx++)
      {
        const u16 ControlField = getBits_5(d, startOffset);
        const u32 Freq_kHz = 16 * getLBits(d, startOffset + 5, 19);
        ts << " [ CF " << hex_str(ControlField) << " Freq " << Freq_kHz << " kHz ]";
        startOffset += 24;
      }
      break;

    case 0b0110:
      ts << " 'DRM' ServId " << hex_str(IdField);
      for (u8 idx = 0; idx < LengthFreqList; idx++)
      {
        if (idx == 0)
        {
          const u8 IdField2 = getBits_8(d, startOffset);
          ts << " ID2 " << hex_str(IdField2);
          startOffset += 8;
        }
        const u8 multBit = getBits_1(d, startOffset);
        const u32 Freq_kHz = (multBit == 1 ? 10 : 1) * getLBits(d, startOffset + 1, 15);
        ts << " [ MB " << multBit << " Freq " << Freq_kHz << " kHz ]";
        startOffset += 16;
      }
      break;

    case 0b1000:
      for (u8 idx = 0; idx < LengthFreqList; idx++)
      {
        const u16 fmFreq_key = getBits_8(d, startOffset + 24 + idx * 8);
        const i32 fmFreq_kHz = 87500 + fmFreq_key * 100;
        qDebug() << "  FM with RDS b" << (idx+1) << "Frequency [kHz]" << fmFreq_kHz << "PI-Code" << IdField;
        startOffset += 8;
      }
      break;

    case 0b1110:
      ts << " 'AMSS' ServId " << hex_str(IdField);
      for (u8 idx = 0; idx < LengthFreqList; idx++)
      {
        const u8 IdField2 = getBits_8(d, startOffset);
        const u8 multBit = getBits_1(d, startOffset + 8);
        const u32 Freq_kHz = (multBit == 1 ? 10 : 1) * getLBits(d, startOffset + 8 + 1, 15);
        ts << " [ ID2 " << hex_str(IdField2) << " MB " << multBit << " Freq " << Freq_kHz << " kHz ]";
        startOffset += 24;
      }
      break;

    default:
      qWarning() << "  Invalid RandM field";
      startOffset += 24 /*Header*/ + LengthFreqList * 8;
    }

    //startOffset += 24 /*Header*/ + LengthFreqList * 8;
    qDebug().noquote() << str;
  }

  assert(upperOffset % 8 == 0); // only full bytes should occur
  return upperOffset / 8;
}
