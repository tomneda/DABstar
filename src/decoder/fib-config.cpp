//
// Created by tomneda on 2025-08-20.
//

#include "fib-config.h"
#include <QDebug>

FibConfig::FibConfig()
{
  Fig0s1_BasicSubChannelOrganizationVec.reserve(64); // avoids memory reallocation that references are usable
  Fig0s2_BasicService_ServiceComponentDefinitionVec.reserve(32); // TODO:
  Fig0s3_ServiceComponentPacketModeVec.reserve(16); // TODO:
  Fig0s8_ServiceComponentGlobalDefinitionVec.reserve(16);
  Fig0s13_UserApplicationInformationVec.reserve(16);
  Fig0s14_SubChannelOrganizationVec.reserve(8);

  reset();
}

void FibConfig::reset()
{
  Fig0s1_BasicSubChannelOrganizationVec.clear();
  Fig0s2_BasicService_ServiceComponentDefinitionVec.clear();
  Fig0s3_ServiceComponentPacketModeVec.clear();
  Fig0s8_ServiceComponentGlobalDefinitionVec.clear();
  Fig0s13_UserApplicationInformationVec.clear();
  Fig0s14_SubChannelOrganizationVec.clear();


  // old style
  subChDescMapFig0_5.clear();
  subChDescMapFig0_14.clear();
  mapServiceCompDesc_SCId.clear();
  mapServiceCompDesc_SId.clear();
  mapServiceCompDesc_SId_SubChId.clear();
  mapServiceCompDesc_SId_SCIdS.clear();
  mapServiceCompDesc_SId_CompNr.clear();
  serviceCompPtrStorage.clear();
}

FibConfig::SFig0s1_BasicSubChannelOrganization * FibConfig::get_Fig0s1_BasicSubChannelOrganization_of_SubChId(const i32 iSubChId)
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

FibConfig::SFig0s2_BasicService_ServiceComponentDefinition * FibConfig::get_Fig0s2_BasicService_ServiceComponentDefinition_of_SId(const u32 iSId)
{
  for (auto & elem : Fig0s2_BasicService_ServiceComponentDefinitionVec)
  {
    const u32 SId = (elem.PD_Flag == 0 ? elem.PD0.SId : elem.PD1.SId);
    if (SId == iSId)
    {
      return &elem;
    }
  }
  return nullptr;
}

FibConfig::SFig0s2_BasicService_ServiceComponentDefinition * FibConfig::get_Fig0s2_BasicService_ServiceComponentDefinition_of_SId_ScIdx(const u32 iSId, const i32 iScIdx)
{
  for (auto & elem : Fig0s2_BasicService_ServiceComponentDefinitionVec)
  {
    const u32 SId = (elem.PD_Flag == 0 ? elem.PD0.SId : elem.PD1.SId);
    if (SId == iSId && elem.ServiceComp_C_index == iScIdx)
    {
      return &elem;
    }
  }
  return nullptr;
}

FibConfig::SFig0s3_ServiceComponentPacketMode * FibConfig::get_Fig0s3_ServiceComponentPacketMode_of_SCId(const i32 iSCId)
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

FibConfig::SFig0s8_ServiceComponentGlobalDefinition * FibConfig::get_Fig0s8_ServiceComponentGlobalDefinition_of_SId(const u32 iSId)
{
  for (auto & elem : Fig0s8_ServiceComponentGlobalDefinitionVec)
  {
    if (elem.SId == iSId)
    {
      return &elem;
    }
  }
  return nullptr;
}

FibConfig::SFig0s13_UserApplicationInformation * FibConfig::get_Fig0s13_UserApplicationInformation_of_SId_SCIdS(const u32 iSId, const i32 iSCIdS)
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


FibConfig::SFig0s14_SubChannelOrganization * FibConfig::get_Fig0s14_SubChannelOrganization_of_SubChId(const i32 iSubChId)
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

// FibConfig::SId_struct * FibConfig::get_SIdTable_of_SId(const u32 iSId)
// {
//   for (auto & elem : SId_table)
//   {
//     if (elem.SId == iSId)
//     {
//       return &elem;
//     }
//   }
//   return nullptr;
// }


void FibConfig::print_Fig0s1_BasicSubChannelOrganization()
{
  qInfo();
  qInfo() << "--- Fig0s1_BasicSubChannelOrganization ---  Size" << Fig0s1_BasicSubChannelOrganizationVec.size() << " Capacity" << Fig0s1_BasicSubChannelOrganizationVec.capacity();
  for (auto & e : Fig0s1_BasicSubChannelOrganizationVec)
  {
    qInfo()
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

void FibConfig::print_Fig0s2_BasicService_ServiceComponentDefinition()
{
  qInfo();
  qInfo() << "--- Fig0s2_BasicService_ServiceComponentDefinition ---  Size" << Fig0s2_BasicService_ServiceComponentDefinitionVec.size() << " Capacity" << Fig0s2_BasicService_ServiceComponentDefinitionVec.capacity();
  for (auto & e : Fig0s2_BasicService_ServiceComponentDefinitionVec)
  {
    QStringList l;
    l << QString("PD_Flag %1").arg(e.PD_Flag);

    if (e.PD_Flag == 0)
    {
      l << QString("SId %1").arg(e.PD0.SId);
      l << QString("(CountryId %1").arg(e.PD0.CountryId);
      l << QString("ServRef %1)").arg(e.PD0.ServiceReference);
    }
    else
    {
      l << QString("SId %1").arg(e.PD1.SId);
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

void FibConfig::print_Fig0s3_ServiceComponentPacketMode()
{
  qInfo();
  qInfo() << "--- Fig0s3_ServiceComponentPacketMode ---  Size" << Fig0s3_ServiceComponentPacketModeVec.size() << " Capacity" << Fig0s3_ServiceComponentPacketModeVec.capacity();
  for (auto & e : Fig0s3_ServiceComponentPacketModeVec)
  {
    qInfo() << "SCId" << e.SCId
            << "CAOrg_Flag" << e.CAOrg_Flag
            << "DG_Flag" << e.DG_Flag
            << "DSCTy" << e.DSCTy
            << "SubChId" << e.SubChId
            << "PacketAddress" << e.PacketAddress
            << "CAOrg" << (e.CAOrg_Flag != 0 ? e.CAOrg : -1);
  }
}

void FibConfig::print_Fig0s8_ServiceComponentGlobalDefinition()
{
  qInfo();
  qInfo() << "--- Fig0s8_ServiceComponentGlobalDefinition ---  Size" << Fig0s8_ServiceComponentGlobalDefinitionVec.size() << " Capacity" << Fig0s8_ServiceComponentGlobalDefinitionVec.capacity();
  for (auto & e : Fig0s8_ServiceComponentGlobalDefinitionVec)
  {
    QStringList l;
    l << QString("PD_Flag %1").arg(e.PD_Flag);
    l << QString("SId %1").arg(e.SId);
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

void FibConfig::print_Fig0s13_UserApplicationInformation()
{
  qInfo();
  qInfo() << "--- Fig0s13_UserApplicationInformation ---  Size" << Fig0s13_UserApplicationInformationVec.size() << " Capacity" << Fig0s13_UserApplicationInformationVec.capacity();
  for (auto & e : Fig0s13_UserApplicationInformationVec)
  {
    QStringList l;
    l << QString("PD_Flag %1").arg(e.PD_Flag);
    l << QString("SId %1").arg(e.SId);
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

void FibConfig::print_Fig0s14_SubChannelOrganization()
{
  qInfo();
  qInfo() << "--- Fig0s14_SubChannelOrganizationVec ---  Size" << Fig0s14_SubChannelOrganizationVec.size() << " Capacity" << Fig0s14_SubChannelOrganizationVec.capacity();
  for (auto & e : Fig0s14_SubChannelOrganizationVec)
  {
    qInfo() << "SubChId" << e.SubChId
            << "FEC_scheme" <<  e.FEC_scheme;
  }
}

// void FibConfig::print_SIdTable_of_SId()
// {
//   qInfo();
//   qInfo() << "--- SId_table ---  Size" << SId_table.size() << " Capacity" << SId_table.capacity();
//   for (auto & e : SId_table)
//   {
//   }
// }
