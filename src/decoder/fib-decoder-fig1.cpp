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
#include <QDebug>

// Cover the different possible labels, section 5.2
void FibDecoder::_process_Fig1(const u8 * const d)
{
  const u8 extension = getBits_3(d, 8 + 5);  // FIG Header + Charset + Rfu

  /* RepTy: Repetition Type:
    A) 10 times per second; -> fast
    B) once per second;     -> fast
    C) once every 10 seconds;
    D) less frequently than every 10 seconds;
    E) all information within 2 minutes.
    F) all information within 1 minutes.
  */

  switch (extension)
  {
  case 0: _process_Fig1s0(d); break;  // RepTyB, ensemble name
  case 1: _process_Fig1s1(d); break;  // RepTyB, service name
  case 4: _process_Fig1s4(d); break;  // RepTyB, service component label
  case 5: _process_Fig1s5(d); break;  // RepTyB, data service label
//case 6: _process_Fig1s6(d); break;  // RepTyB, XPAD label - 8.1.14.4
  default:
    if (mUnhandledFig1Set.find(extension) == mUnhandledFig1Set.end()) // print message only once
    {
      const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - mLastTimePoint); // see issue https://github.com/tomneda/DABstar/issues/99
      if (mFibLoadingState >= EFibLoadingState::S5_DeferredDataLoaded) qDebug().noquote() << QString("FIG 1/%1 not handled (received after %2 ms after service start trigger)").arg(extension).arg(diff.count()); // print only if the summarized print was already done
      mUnhandledFig1Set.emplace(extension);
    }
  }
}

// Name of the ensemble
void FibDecoder::_process_Fig1s0(const u8 * const d)
{
  FibConfigFig1::SFig1s0_EnsembleLabel fig1s0;
  fig1s0.EId = getBits(d, 16, 16);

  if (!_extract_character_set_label(fig1s0, d, 32))
  {
    return;
  }

  if (mpFibConfigFig1->Fig1s0_EnsembleLabelVec.empty())
  {
    fig1s0.set_current_time();
    mpFibConfigFig1->Fig1s0_EnsembleLabelVec.emplace_back(fig1s0);
    // _retrigger_timer_data_loaded_fast("Fig1s0");
    emit signal_name_of_ensemble(fig1s0.EId, fig1s0.Name, fig1s0.NameShort);
  }
  else
  {
    mpFibConfigFig1->Fig1s0_EnsembleLabelVec[0].set_current_time_2nd_call();
  }
}

// Name of service
void FibDecoder::_process_Fig1s1(const u8 * const d)
{
  FibConfigFig1::SFig1s1_ProgrammeServiceLabel fig1s1;
  fig1s1.SId = getBits(d, 16, 16);

  if (!_extract_character_set_label(fig1s1, d, 16 + 16))
  {
    return;
  }

  if (const auto * const pFig1s1 = mpFibConfigFig1->get_Fig1s1_ProgrammeServiceLabel_of_SId(fig1s1.SId);
      pFig1s1 == nullptr)
  {
    fig1s1.set_current_time();
    mpFibConfigFig1->Fig1s1_ProgrammeServiceLabelVec.emplace_back(fig1s1);
    FibConfigFig1::SSId_SCIdS SId_SCIdS{fig1s1.SId, -1}; // -1 comes from FIG1/5 and unspecified
    mpFibConfigFig1->serviceLabel_To_SId_SCIdS_Map.try_emplace(fig1s1.Name, SId_SCIdS);
    _retrigger_timer_data_loaded_fast("Fig1s1");
  }
  else
  {
    const_cast<FibConfigFig1::SFig1s1_ProgrammeServiceLabel *>(pFig1s1)->set_current_time_2nd_call(); // won't give up the "const" of the get_..-method
  }
}

// Service component label 8.1.14.3
void FibDecoder::_process_Fig1s4(const u8 * const d)
{
  FibConfigFig1::SFig1s4_ServiceComponentLabel fig1s4;
  fig1s4.PD_Flag = getBits_1(d, 16);
  fig1s4.SCIdS = getBits_4(d, 20);
  fig1s4.SId = (fig1s4.PD_Flag ? getLBits(d, 24, 32) : getLBits(d, 24, 16));
  const i16 offset = fig1s4.PD_Flag ? 56 : 40;

  if (const auto * const pFig1s4 = mpFibConfigFig1->get_Fig1s4_ServiceComponentLabel_of_SId_SCIdS(fig1s4.SId, fig1s4.SCIdS);
      pFig1s4 == nullptr)
  {
    if (!_extract_character_set_label(fig1s4, d, offset))
    {
      return;
    }

    fig1s4.set_current_time();
    mpFibConfigFig1->Fig1s4_ServiceComponentLabelVec.emplace_back(fig1s4);
    FibConfigFig1::SSId_SCIdS SId_SCIdS{fig1s4.SId, fig1s4.SCIdS};
    mpFibConfigFig1->serviceLabel_To_SId_SCIdS_Map.try_emplace(fig1s4.Name, SId_SCIdS);
    _retrigger_timer_data_loaded_slow("Fig1s4");
  }
  else
  {
    const_cast<FibConfigFig1::SFig1s4_ServiceComponentLabel *>(pFig1s4)->set_current_time_2nd_call(); // won't give up the "const" of the get_..-method
  }
}

// Data service label 8.1.14.2
void FibDecoder::_process_Fig1s5(const u8 * const d)
{
  FibConfigFig1::SFig1s5_DataServiceLabel fig1s5;
  constexpr i16 offset = 48;
  fig1s5.SId = getLBits(d, 16, 32);

  if (const auto * const pFig1s5 = mpFibConfigFig1->get_Fig1s5_DataServiceLabel_of_SId(fig1s5.SId);
      pFig1s5 == nullptr)
  {
    if (!_extract_character_set_label(fig1s5, d, offset))
    {
      return;
    }

    fig1s5.set_current_time();
    mpFibConfigFig1->Fig1s5_DataServiceLabelVec.emplace_back(fig1s5);
    FibConfigFig1::SSId_SCIdS SId_SCIdS{fig1s5.SId, -5}; // -5 comes from FIG1/5 and unspecified
    mpFibConfigFig1->serviceLabel_To_SId_SCIdS_Map.try_emplace(fig1s5.Name, SId_SCIdS);
    _retrigger_timer_data_loaded_slow("Fig1s5");
  }
  else
  {
    const_cast<FibConfigFig1::SFig1s5_DataServiceLabel *>(pFig1s5)->set_current_time_2nd_call(); // won't give up the "const" of the get_..-method
  }
}

// XPAD label - 8.1.14.4
// void FibDecoder::_process_Fig1s6(const u8 * const d) const
// {
//   const u8 PD_bit = getBits_1(d, 16);
//   const u8 SCIdS = getBits_4(d, 20);
//
//   u32 SId = 0;
//   i16 offset = 0;
//   u8 XPAD_apptype;
//
//   if (PD_bit)
//   {  // 32 bits identifier for XPAD label
//     SId = getLBits(d, 24, 32);
//     XPAD_apptype = getBits_5(d, 59);
//     offset = 64;
//   }
//   else
//   {  // 16 bit identifier for XPAD label
//     SId = getLBits(d, 24, 16);
//     XPAD_apptype = getBits_5(d, 43);
//     offset = 48;
//   }
//
//   (void)SId;
//   (void)SCIdS;
//   (void)XPAD_apptype;
//   (void)offset;
// }
