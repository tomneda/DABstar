//
// Created by tomneda on 2025-09-15.
//
#include "fib-config-fig1.h"
#include <QDateTime>
#include <QDebug>

FibConfigFig1::FibConfigFig1()
{
  reset();

  Fig1s0_EnsembleLabelVec.reserve(1);
  Fig1s1_ProgrammeServiceLabelVec.reserve(64);
  Fig1s4_ServiceComponentLabelVec.reserve(64);
  Fig1s5_DataServiceLabelVec.reserve(64);
}

void FibConfigFig1::reset()
{
  Fig1s0_EnsembleLabelVec.clear();
  Fig1s1_ProgrammeServiceLabelVec.clear();
  Fig1s4_ServiceComponentLabelVec.clear();
  Fig1s5_DataServiceLabelVec.clear();

  serviceLabel_To_SId_SCIdS_Map.clear();
}

const FibConfigFig1::SFig1s1_ProgrammeServiceLabel * FibConfigFig1::get_Fig1s1_ProgrammeServiceLabel_of_SId(const u32 SId) const
{
  for (auto & elem : Fig1s1_ProgrammeServiceLabelVec)
  {
    if (elem.SId == SId)
    {
      return &elem;
    }
  }
  return nullptr;
}

const FibConfigFig1::SFig1s4_ServiceComponentLabel * FibConfigFig1::get_Fig1s4_ServiceComponentLabel_of_SId_SCIdS(u32 SId, i8 SCIdS) const
{
  for (auto & elem : Fig1s4_ServiceComponentLabelVec)
  {
    if (elem.SId == SId && elem.SCIdS == SCIdS)
    {
      return &elem;
    }
  }
  return nullptr;
}

const FibConfigFig1::SFig1s5_DataServiceLabel * FibConfigFig1::get_Fig1s5_DataServiceLabel_of_SId(u32 SId) const
{
  for (auto & elem : Fig1s5_DataServiceLabelVec)
  {
    if (elem.SId == SId)
    {
      return &elem;
    }
  }
  return nullptr;
}

const QString & FibConfigFig1::get_service_label_of_SId_from_all_Fig1(const u32 iSId) const
{
  static const QString emptyString;

  if (const auto * pFig1s1 = get_Fig1s1_ProgrammeServiceLabel_of_SId(iSId);
      pFig1s1 != nullptr)
  {
    return pFig1s1->Name;
  }

  if (const auto * pFig1s5 = get_Fig1s5_DataServiceLabel_of_SId(iSId);
      pFig1s5 != nullptr)
  {
    return pFig1s5->Name;
  }

  // if (const auto * pFig1s4 = get_Fig1s4_ServiceComponentLabel_of_SId_SCIdS(iSId);
  //     pFig1s4 != nullptr)
  // {
  //   return pFig1s4.Name;
  // }
  return emptyString;
}

const FibConfigFig1::SSId_SCIdS * FibConfigFig1::get_SId_SCIdS_from_service_label(const QString & s) const
{
  const auto it = serviceLabel_To_SId_SCIdS_Map.find(s);

  if (it != serviceLabel_To_SId_SCIdS_Map.end())
  {
    return &(it->second);
  }

  return nullptr;
}

void FibConfigFig1::print_Fig1s0_EnsembleLabel(SStatistic & ioS, const bool iCollectStatisticsOnly)
{
  if (iCollectStatisticsOnly)
  {
    for (const auto & e : Fig1s0_EnsembleLabelVec)
    {
      get_statistics(e, ioS);
    }
    return;
  }

  qInfo();
  qInfo() << "--- Fig1s0_EnsembleLabel ---  Size" << Fig1s0_EnsembleLabelVec.size() << " Capacity" << Fig1s0_EnsembleLabelVec.capacity();
  for (const auto & e : Fig1s0_EnsembleLabelVec)
  {
    qInfo() << print_duration_and_get_statistics(e, ioS).toStdString().c_str() // this way the quotes are omitted
            << "Name" << e.Name
            << "NameShort" << e.NameShort
            << "EId" << hex_str(e.EId).toStdString().c_str() // C-string gets now quotes in output
            << "Charset" << e.Charset
            << "CharFlagField" << e.CharFlagField;
  }
}

void FibConfigFig1::print_Fig1s1_ProgrammeServiceLabelVec(SStatistic & ioS, const bool iCollectStatisticsOnly)
{
  if (iCollectStatisticsOnly)
  {
    for (const auto & e : Fig1s1_ProgrammeServiceLabelVec)
    {
      get_statistics(e, ioS);
    }
    return;
  }

  qInfo();
  qInfo() << "--- Fig1s1_ProgrammeServiceLabel ---  Size" << Fig1s1_ProgrammeServiceLabelVec.size() << " Capacity" << Fig1s1_ProgrammeServiceLabelVec.capacity();
  for (const auto & e : Fig1s1_ProgrammeServiceLabelVec)
  {
    qInfo() << print_duration_and_get_statistics(e, ioS).toStdString().c_str() // this way the quotes are omitted
            << "Name" << e.Name
            << "NameShort" << e.NameShort
            << "SId" << hex_str(e.SId).toStdString().c_str() // C-string gets now quotes in output
            << "Charset" << e.Charset
            << "CharFlagField" << e.CharFlagField;
  }
}

void FibConfigFig1::print_Fig1s4_ServiceComponentLabel(SStatistic & ioS, const bool iCollectStatisticsOnly)
{
  if (iCollectStatisticsOnly)
  {
    for (const auto & e : Fig1s4_ServiceComponentLabelVec)
    {
      get_statistics(e, ioS);
    }
    return;
  }

  qInfo();
  qInfo() << "--- Fig1s4_ServiceComponentLabel ---  Size" << Fig1s4_ServiceComponentLabelVec.size() << " Capacity" << Fig1s4_ServiceComponentLabelVec.capacity();
  for (const auto & e : Fig1s4_ServiceComponentLabelVec)
  {
    qInfo() << print_duration_and_get_statistics(e, ioS).toStdString().c_str() // this way the quotes are omitted
            << "Name" << e.Name
            << "NameShort" << e.NameShort
            << "SId" << hex_str(e.SId).toStdString().c_str() // C-string gets now quotes in output
            << "SCIdS" << e.SCIdS
            << "PD_Flag" << e.PD_Flag
            << "Charset" << e.Charset
            << "CharFlagField" << e.CharFlagField;
  }
}

void FibConfigFig1::print_Fig1s5_DataServiceLabel(SStatistic & ioS, const bool iCollectStatisticsOnly)
{
  if (iCollectStatisticsOnly)
  {
    for (const auto & e : Fig1s5_DataServiceLabelVec)
    {
      get_statistics(e, ioS);
    }
    return;
  }

  qInfo();
  qInfo() << "--- Fig1s5_DataServiceLabel ---  Size" << Fig1s5_DataServiceLabelVec.size() << " Capacity" << Fig1s5_DataServiceLabelVec.capacity();
  for (const auto & e : Fig1s5_DataServiceLabelVec)
  {
    qInfo() << print_duration_and_get_statistics(e, ioS).toStdString().c_str() // this way the quotes are omitted
            << "Name" << e.Name
            << "NameShort" << e.NameShort
            << "SId" << hex_str(e.SId).toStdString().c_str() // C-string gets now quotes in output
            << "Charset" << e.Charset
            << "CharFlagField" << e.CharFlagField;
  }
}
