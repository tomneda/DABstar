//
// Created by tomneda on 2025-09-15.
//
#include "fib-config-fig0.h"
#include "dab-constants.h"
#include <QDebug>
#include <QDateTime>

FibConfigFig0::FibConfigFig0()
{
  reset();

  // TODO: evaluate reasonable sizes to reserve
  // avoids memory reallocation that references are usable
  Fig0s1_BasicSubChannelOrganizationVec.reserve(64);
  Fig0s2_BasicService_ServiceCompDefVec.reserve(32);
  Fig0s3_ServiceComponentPacketModeVec.reserve(16);
  Fig0s5_ServiceComponentLanguageVec.reserve(8);
  Fig0s8_ServiceCompGlobalDefVec.reserve(32);
  Fig0s9_CountryLtoInterTabVec.reserve(1);
  Fig0s13_UserApplicationInformationVec.reserve(16);
  Fig0s14_SubChannelOrganizationVec.reserve(8);
  Fig0s17_ProgrammeTypeVec.reserve(8);
}

void FibConfigFig0::reset()
{
  Fig0s1_BasicSubChannelOrganizationVec.clear();
  Fig0s2_BasicService_ServiceCompDefVec.clear();
  Fig0s3_ServiceComponentPacketModeVec.clear();
  Fig0s5_ServiceComponentLanguageVec.clear();
  Fig0s8_ServiceCompGlobalDefVec.clear();
  Fig0s9_CountryLtoInterTabVec.clear();
  Fig0s13_UserApplicationInformationVec.clear();
  Fig0s14_SubChannelOrganizationVec.clear();
  Fig0s17_ProgrammeTypeVec.clear();
}

const FibConfigFig0::SFig0s1_BasicSubChannelOrganization * FibConfigFig0::get_Fig0s1_BasicSubChannelOrganization_of_SubChId(const i32 iSubChId) const
{
  for (auto & elem : Fig0s1_BasicSubChannelOrganizationVec)
  {
    if (elem.SubChId == iSubChId)
    {
      return &elem;
    }
  }
  return nullptr;
}

const FibConfigFig0::SFig0s2_BasicService_ServiceCompDef * FibConfigFig0::get_Fig0s2_BasicService_ServiceCompDef_of_SId(const u32 iSId) const
{
  for (auto & elem : Fig0s2_BasicService_ServiceCompDefVec)
  {
    const u32 SId = (elem.PD_Flag == 0 ? elem.PD0.SId : elem.PD1.SId);
    if (SId == iSId)
    {
      return &elem;
    }
  }
  return nullptr;
}

const FibConfigFig0::SFig0s2_BasicService_ServiceCompDef * FibConfigFig0::get_Fig0s2_BasicService_ServiceCompDef_of_SId_ScIdx(const u32 iSId, const i32 iScIdx) const
{
  for (auto & elem : Fig0s2_BasicService_ServiceCompDefVec)
  {
    const u32 SId = elem.get_SId();
    if (SId == iSId && elem.ServiceComp_C_index == iScIdx)
    {
      return &elem;
    }
  }
  return nullptr;
}

const FibConfigFig0::SFig0s2_BasicService_ServiceCompDef * FibConfigFig0::get_Fig0s2_BasicService_ServiceCompDef_of_SCId(i16 SCId) const
{
  for (auto & fig0s2 : Fig0s2_BasicService_ServiceCompDefVec)
  {
    if (fig0s2.ServiceComp_C.TMId == ETMId::PacketModeData && fig0s2.ServiceComp_C.TMId11.SCId == SCId)
    {
      return &fig0s2;
    }
  }
  return nullptr;
}

const FibConfigFig0::SFig0s2_BasicService_ServiceCompDef * FibConfigFig0::get_Fig0s2_BasicService_ServiceCompDef_of_SId_TMId(u32 iSId, u8 iTMId) const
{
  for (auto & fig0s2 : Fig0s2_BasicService_ServiceCompDefVec)
  {
    if (fig0s2.get_SId() == iSId && fig0s2.ServiceComp_C.TMId == iTMId)
    {
      return &fig0s2;
    }
  }
  return nullptr;
}

const FibConfigFig0::SFig0s3_ServiceComponentPacketMode * FibConfigFig0::get_Fig0s3_ServiceComponentPacketMode_of_SCId(const i32 iSCId) const
{
  for (auto & elem : Fig0s3_ServiceComponentPacketModeVec)
  {
    if (elem.SCId == iSCId)
    {
      return &elem;
    }
  }
  return nullptr;
}

const FibConfigFig0::SFig0s5_ServiceComponentLanguage * FibConfigFig0::get_Fig0s5_ServiceComponentLanguage_of_SubChId(u8 iSubChId) const
{
  for (auto & fig0s5 : Fig0s5_ServiceComponentLanguageVec)
  {
    if (fig0s5.LS_Flag == 0 && fig0s5.SubChId == iSubChId)
    {
      return &fig0s5;
    }
  }
  return nullptr;
}

const FibConfigFig0::SFig0s5_ServiceComponentLanguage * FibConfigFig0::get_Fig0s5_ServiceComponentLanguage_of_SCId(u8 iSCId) const
{
  for (auto & fig0s5 : Fig0s5_ServiceComponentLanguageVec)
  {
    if (fig0s5.LS_Flag == 1 && fig0s5.SCId == iSCId)
    {
      return &fig0s5;
    }
  }
  return nullptr;
}

const FibConfigFig0::SFig0s8_ServiceCompGlobalDef * FibConfigFig0::get_Fig0s8_ServiceCompGlobalDef_of_SId_SCIdS(u32 iSId, u8 iSCIdS) const
{
  for (auto & fig0s8 : Fig0s8_ServiceCompGlobalDefVec)
  {
    if (fig0s8.SId == iSId && fig0s8.SCIdS == iSCIdS)
    {
      return &fig0s8;
    }
  }
  return nullptr;
}

const FibConfigFig0::SFig0s8_ServiceCompGlobalDef * FibConfigFig0::get_Fig0s8_ServiceCompGlobalDef_of_SId(const u32 iSId) const
{
  for (auto & elem : Fig0s8_ServiceCompGlobalDefVec)
  {
    if (elem.SId == iSId)
    {
      return &elem;
    }
  }
  return nullptr;
}

const FibConfigFig0::SFig0s9_CountryLtoInterTab * FibConfigFig0::get_Fig0s9_CountryLtoInterTab() const
{
  if (!Fig0s9_CountryLtoInterTabVec.empty())
  {
    return &Fig0s9_CountryLtoInterTabVec[0];
  }
  return nullptr;
}

const FibConfigFig0::SFig0s13_UserApplicationInformation * FibConfigFig0::get_Fig0s13_UserApplicationInformation_of_SId_SCIdS(const u32 iSId, const i32 iSCIdS) const
{
  for (auto & elem : Fig0s13_UserApplicationInformationVec)
  {
    if (elem.SId == iSId && elem.SCIdS == iSCIdS)
    {
      return &elem;
    }
  }
  return nullptr;
}

const FibConfigFig0::SFig0s14_SubChannelOrganization * FibConfigFig0::get_Fig0s14_SubChannelOrganization_of_SubChId(const i32 iSubChId) const
{
  for (auto & elem : Fig0s14_SubChannelOrganizationVec)
  {
    if (elem.SubChId == iSubChId)
    {
      return &elem;
    }
  }
  return nullptr;
}

const FibConfigFig0::SFig0s17_ProgrammeType * FibConfigFig0::get_Fig0s17_ProgrammeType_of_SId(u16 iSId) const
{
  for (auto & fig0s17 : Fig0s17_ProgrammeTypeVec)
  {
    if (fig0s17.SId == iSId)
    {
      return &fig0s17;
    }
  }
  return nullptr;
}

QString FibConfigFig0::format_time(const SFigBase & iFigBase) const
{
  const auto curTimePoint = std::chrono::steady_clock::now();
  const auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(curTimePoint - iFigBase.TimePoint);
  const auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(iFigBase.TimePoint2ndCall - iFigBase.TimePoint);
  return QString("[%1 ms][%2 ms]").arg(duration1.count(), 5, 10).arg(duration2.count(), 5, 10);
}

void FibConfigFig0::print_Fig0s1_BasicSubChannelOrganization() const
{
  qInfo();
  qInfo() << "--- Fig0s1_BasicSubChannelOrganization ---  Size" << Fig0s1_BasicSubChannelOrganizationVec.size() << " Capacity" << Fig0s1_BasicSubChannelOrganizationVec.capacity();
  for (auto & e : Fig0s1_BasicSubChannelOrganizationVec)
  {
    qInfo() << format_time(e).toStdString().c_str() // this way the quotes are omitted
            << "SubChId" << e.SubChId
            << "StartAddr" << e.StartAddr
            << "ProtLevel" << e.ProtectionLevel
            << "SubChSize" << e.SubChannelSize
            << "BitRate" << e.BitRate
            << "ShortForm" << e.ShortForm
            << "Option" << e.Option
            << "TabSw" << e.TableSwitch
            << "TabIdx" << e.TableIndex;
  }
}

void FibConfigFig0::print_Fig0s2_BasicService_ServiceCompDef() const
{
  qInfo();
  qInfo() << "--- Fig0s2_BasicService_ServiceCompDef ---  Size" << Fig0s2_BasicService_ServiceCompDefVec.size() << " Capacity" << Fig0s2_BasicService_ServiceCompDefVec.capacity();
  for (const auto & e : Fig0s2_BasicService_ServiceCompDefVec)
  {
    QStringList l;
    l << format_time(e);
    l << QString("PD_Flag %1").arg(e.PD_Flag);

    if (e.PD_Flag == 0)
    {
      l << "SId " + hex_str(e.PD0.SId);
      l << QString("(CountryId %1").arg(e.PD0.CountryId);
      l << QString("ServRef %1)").arg(e.PD0.ServiceReference);
    }
    else
    {
      l << "SId " + hex_str(e.PD1.SId);
      l << QString("(EEC %1").arg(e.PD1.EEC);
      l << QString("CountryId %1").arg(e.PD1.CountryId);
      l << QString("ServRef %1)").arg(e.PD1.ServiceReference);
    }

    l << QString("NumSC %1").arg(e.NumServiceComp);
    l << QString("SCIdx %1").arg(e.ServiceComp_C_index);
    l << QString("TMId %1").arg(e.ServiceComp_C.TMId);

    if (e.ServiceComp_C.TMId == 0)
    {
      l << QString("SubChId %1").arg(e.ServiceComp_C.TMId00.SubChId);
      l << QString("ASCTy %1").arg(e.ServiceComp_C.TMId00.ASCTy);
    }
    else if (e.ServiceComp_C.TMId == 1)
    {
      l << QString("SubChId %1").arg(e.ServiceComp_C.TMId01.SubChId);
      l << QString("DSCTy %1").arg(e.ServiceComp_C.TMId01.DSCTy);
    }
    else if (e.ServiceComp_C.TMId == 3)
    {
      l << QString("SCId %1").arg(e.ServiceComp_C.TMId11.SCId);
    }
    l << QString("PS_FLag %1").arg(e.ServiceComp_C.PS_Flag);
    l << QString("CA_FLag %1").arg(e.ServiceComp_C.CA_Flag);

    qInfo().noquote() << l.join(' ');
  }
}

void FibConfigFig0::print_Fig0s3_ServiceComponentPacketMode() const
{
  qInfo();
  qInfo() << "--- Fig0s3_ServiceComponentPacketMode ---  Size" << Fig0s3_ServiceComponentPacketModeVec.size() << " Capacity" << Fig0s3_ServiceComponentPacketModeVec.capacity();
  for (auto & e : Fig0s3_ServiceComponentPacketModeVec)
  {
    qInfo() << format_time(e).toStdString().c_str() // this way the quotes are omitted
            << "SCId" << e.SCId
            << "CAOrg_Flag" << e.CAOrg_Flag
            << "DG_Flag" << e.DG_Flag
            << "DSCTy" << e.DSCTy
            << "SubChId" << e.SubChId
            << "PacketAddress" << e.PacketAddress
            << "CAOrg" << (e.CAOrg_Flag != 0 ? e.CAOrg : -1);
  }
}

void FibConfigFig0::print_Fig0s5_ServiceComponentLanguage() const
{
  qInfo();
  qInfo() << "--- Fig0s5_ServiceComponentLanguage ---  Size" << Fig0s5_ServiceComponentLanguageVec.size() << " Capacity" << Fig0s5_ServiceComponentLanguageVec.capacity();
  for (auto & e : Fig0s5_ServiceComponentLanguageVec)
  {
    qInfo() << format_time(e).toStdString().c_str() // this way the quotes are omitted
            << "LS_Flag" << e.LS_Flag
            << "SubChId" << e.SubChId
            << "SCId" << e.SCId
            << "Language" << e.Language;
  }
}

void FibConfigFig0::print_Fig0s8_ServiceCompGlobalDef() const
{
  qInfo();
  qInfo() << "--- Fig0s8_ServiceCompGlobalDef ---  Size" << Fig0s8_ServiceCompGlobalDefVec.size() << " Capacity" << Fig0s8_ServiceCompGlobalDefVec.capacity();
  for (const auto & e : Fig0s8_ServiceCompGlobalDefVec)
  {
    QStringList l;
    l << format_time(e);
    l << QString("PD_Flag %1").arg(e.PD_Flag);
    l << "SId " + hex_str(e.SId);
    l << QString("Ext_Flag %1").arg(e.Ext_Flag);
    l << QString("SCIdS %1").arg(e.SCIdS);
    l << QString("LS_Flag %1").arg(e.LS_Flag);

    if (e.LS_Flag == 0) // short form
    {
      l << QString("SubChId %1").arg(e.SubChId);
    }
    else // long form
    {
      l << QString("SCId %1").arg(e.SCId);
    }

    qInfo().noquote() << l.join(' ');
  }
}

void FibConfigFig0::print_Fig0s9_CountryLtoInterTab() const
{
  qInfo();
  qInfo() << "--- Fig0s9_CountryLtoInterTab ---  Size" << Fig0s9_CountryLtoInterTabVec.size() << " Capacity" << Fig0s9_CountryLtoInterTabVec.capacity();
  for (auto & e : Fig0s9_CountryLtoInterTabVec)
  {
    qInfo() << format_time(e).toStdString().c_str() // this way the quotes are omitted
            << "Ext_Flag" << e.Ext_Flag
            << "Ensemble_LTO" << e.Ensemble_LTO
            << "Ensemble_ECC" << e.Ensemble_ECC
            << "InterTableId" << e.InterTableId
            << "LTO_Minutes" << e.LTO_minutes;
  }
}

void FibConfigFig0::print_Fig0s13_UserApplicationInformation() const
{
  qInfo();
  qInfo() << "--- Fig0s13_UserApplicationInformation ---  Size" << Fig0s13_UserApplicationInformationVec.size() << " Capacity" << Fig0s13_UserApplicationInformationVec.capacity();
  for (auto & e : Fig0s13_UserApplicationInformationVec)
  {
    QStringList l;
    l << format_time(e);
    l << QString("PD_Flag %1").arg(e.PD_Flag);
    l << "SId " + hex_str(e.SId);
    l << QString("SCIdS %1").arg(e.SCIdS);
    l << QString("NumUserApps %1").arg(e.NumUserApps);

    for (i16 i = 0; i < std::min(e.NumUserApps, (i16)e.UserAppVec.size()); i++)
    {
      const auto & u = e.UserAppVec[i];
      l << QString("[UserAppType %1").arg(u.UserAppType);
      l << QString("UserAppDataLength %1").arg(u.UserAppDataLength);
      l << QString("XPadData %1]").arg(u.XPadData);
    }
    qInfo().noquote() << l.join(' ');
  }
}

void FibConfigFig0::print_Fig0s14_SubChannelOrganization() const
{
  qInfo();
  qInfo() << "--- Fig0s14_SubChannelOrganizationVec ---  Size" << Fig0s14_SubChannelOrganizationVec.size() << " Capacity" << Fig0s14_SubChannelOrganizationVec.capacity();
  for (auto & e : Fig0s14_SubChannelOrganizationVec)
  {
    qInfo() << format_time(e).toStdString().c_str() // this way the quotes are omitted
            << "SubChId" << e.SubChId
            << "FEC_scheme" <<  e.FEC_scheme;
  }
}

void FibConfigFig0::print_Fig0s17_ProgrammeType() const
{
  qInfo();
  qInfo() << "--- Fig0s17_ProgrammeType ---  Size" << Fig0s17_ProgrammeTypeVec.size() << " Capacity" << Fig0s17_ProgrammeTypeVec.capacity();
  for (const auto & e : Fig0s17_ProgrammeTypeVec)
  {
    qInfo().noquote() << format_time(e).toStdString().c_str() // this way the quotes are omitted
                      << "SId" << hex_str(e.SId)
                      << "SD_Flag" << e.SD_Flag
                      << "IntCode" << e.IntCode;
  } 
}
