#include "fib-decoder.h"
#include "dab-tables.h"
#include <QTextStream>
#include <QDebug>

template<typename T> inline QString hex_str(const T iVal) { return QString("0x%1").arg(iVal, 0, 16); }
template<typename T> inline QString dec_hex_str(const T iVal) { return QString("%1/0x%2").arg(iVal).arg(iVal, 0, 16); }

QStringList FibDecoder::get_fib_content_str_list(i32 & oNumCols) const
{
  oNumCols = 12; // maximum necessary columns to print following content
  
  /*
   * Meaning of the first letter (for colorization):
   * C: Caption
   * H: Header
   * E: Empty Line
   * D: Data
   * I: Information
   */
  QStringList out;

  out << "H;EnsembleLabel;ShortEnsLabel;EId";
  for (const auto & fig1s0 : mpFibConfigFig1->Fig1s0_EnsembleLabelVec) // should only have one element
  {
    out << "D;" + fig1s0.Name + ";" + fig1s0.NameShort + ";" + hex_str(fig1s0.EId);
  }

  out << "E"; // empty line
  out << "C;Audio services:";
  out << "H;ServiceLabel;ShortServLabel;ServiceId;SubChannel;StartAddr [CU];Length [CU];Protection;CodeRate;BitRate [kbps];DabType;Language;ProgramType";

  for (const auto & fig0s2 : mpFibConfigFig0Curr->Fig0s2_BasicService_ServiceCompDefVec)
  {
    if (fig0s2.ServiceComp_C.TMId == ETMId::StreamModeAudio) // skip non-audio elements
    {
      out << "D;" + _get_audio_data_str(fig0s2);
    }
  }

  out << "E"; // empty line
  out << "C;Primary data services:";
  out << "H;ServiceLabel;ShortServLabel;ServiceId;SubChannel;StartAddr [CU];Length [CU];Protection;CodeRate;FEC_Scheme;AppType;PacketAddr;DSCTy";

  for (const auto & fig0s2 : mpFibConfigFig0Curr->Fig0s2_BasicService_ServiceCompDefVec)
  {
    if (fig0s2.ServiceComp_C.TMId == ETMId::PacketModeData && fig0s2.PD_Flag == 1) // choose primary packet data elements
    {
      out << "D;" + _get_packet_data_str(fig0s2);
    }
  }

  out << "E"; // empty line
  out << "C;Secondary data services:";
  out << "H;ServiceLabel;ShortServLabel;ServiceId;SubChannel;StartAddr [CU];Length [CU];Protection;CodeRate;FEC_Scheme;AppType;PacketAddr;DSCTy";

  for (const auto & fig0s2 : mpFibConfigFig0Curr->Fig0s2_BasicService_ServiceCompDefVec)
  {
    if (fig0s2.ServiceComp_C.TMId == ETMId::PacketModeData && fig0s2.PD_Flag == 0) // choose secondary packet data elements
    {
      out << "D;" + _get_packet_data_str(fig0s2);
    }
  }

  return out;
}

QString FibDecoder::_get_audio_data_str(const FibConfigFig0::SFig0s2_BasicService_ServiceCompDef & iFig0s2) const
{
  SAudioData ad;
  SAudioDataAddOns adao;
  if (!_get_data_for_audio_service(iFig0s2, &ad) || !_get_data_for_audio_service_addon(iFig0s2, &adao))
  {
    return "-;-;-;-;-;-;-;-;-;-;-;-";
  }

  QString str;
  QTextStream ts(&str);

  ts << ad.serviceLabel << ";";
  ts << ad.serviceLabelShort << ";";
  ts << hex_str(ad.SId) << ";";
  ts << ad.SubChId << ";";
  ts << ad.CuStartAddr << ";";
  ts << ad.CuSize << ";";
  ts << getProtectionLevel(ad.shortForm, ad.protLevel) << ";";
  ts << getCodeRate(ad.shortForm, ad.protLevel) << ";";
  ts << ad.bitRate << ";";
  ts << dec_hex_str(ad.ASCTy) << (ad.ASCTy == 0x3F ? " (DAB+);" : " (DAB);");
  ts << (adao.language.has_value() ?getLanguage(adao.language.value()) : "-") << ";";
  ts << (adao.programType.has_value() ? QString::number(adao.programType.value()) + " (" + getProgramType(adao.programType.value()) + ")" : "-");
  // ts << "-"; // TODO: FM frequencies

  return str;
}

QString FibDecoder::_get_packet_data_str(const FibConfigFig0::SFig0s2_BasicService_ServiceCompDef & iFig0s2) const
{
  SPacketData pd;
  if (!_get_data_for_packet_service(iFig0s2, 0, &pd))
  {
    return "Inconsistent data;-;" + hex_str(iFig0s2.get_SId()) + ";-;-;-;-;-;-;-;-;-;";
    // return "-;-;-;-;-;-;-;-;-;-;-;-;";
  }

  QString str;
  QTextStream ts(&str);

  ts << pd.serviceLabel << ";";
  ts << pd.serviceLabelShort << ";";
  ts << hex_str(pd.SId) << ";";
  ts << pd.SubChId << ";";
  ts << pd.CuStartAddr << ";";
  ts << pd.CuSize << ";";
  ts << getProtectionLevel(pd.shortForm, pd.protLevel) << ";";
  ts << getCodeRate(pd.shortForm, pd.protLevel) << ";";
  ts << QString::number(pd.FEC_scheme) << ";";

  assert(!pd.appTypeVec.empty()); // must have at least one element
  for (i16 i = 0; i < (i16)pd.appTypeVec.size(); ++i)
  {
    ts << dec_hex_str(pd.appTypeVec[i]);
    ts << (i < (i16)pd.appTypeVec.size() - 1 ? "," : ";");
  }

  ts << pd.PacketAddress << ";";
  ts << pd.DSCTy << " (" << get_DSCTy_AppType(pd.DSCTy, pd.appTypeVec[0]) << ")";

  return str;
}
