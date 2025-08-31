#include "fib-decoder.h"
#include "dab-tables.h"

i32 FibDecoder::get_scan_width() const
{
  QString s1 = get_audio_header();
  QString s2 = get_packet_header();
  QStringList l1 = s1.split(";");
  QStringList l2 = s2.split(";");
  return l1.size() >= l2.size() ? l1.size() - 1 : l2.size() - 1;
}

QStringList FibDecoder::basic_print() const
{
  QStringList out;
  bool hasContents = false;

  for (const auto & pScd : currentConfig->serviceCompPtrStorage)
  {
    if (pScd->TMid != ETMId::StreamModeAudio) // skip non-audio elements
    {
      continue;
    }

    if (!hasContents)
    {
      out << get_audio_header();
      hasContents = true;
    }

    out << get_audio_data(*pScd);
  }

  hasContents = false;

  for (const auto & pScd : currentConfig->serviceCompPtrStorage)
  {
    if (pScd->TMid != ETMId::PacketModeData) // skip non-packet elements
    {
      continue;
    }

    if (get_sub_channel_of(*pScd) == "")
    {
      continue;
    }

    if (!hasContents)
    {
      out << "\n";
      out << get_packet_header();
      hasContents = true;
    }

    out << get_packet_data(*pScd);
  }
  return out;
}

//
//
QString FibDecoder::get_service_name(const ServiceComponentDescriptor & iScd) const
{
  const i32 sid = iScd.SId;
  const auto it = ensemble->servicesMap.find(sid);

  if (it != ensemble->servicesMap.end())
  {
    return it->second.serviceLabel;
  }
  return "";
}

QString FibDecoder::get_service_id_of(const ServiceComponentDescriptor & iScd) const
{
  return QString::number(iScd.SId, 16);
}

QString FibDecoder::get_sub_channel_of(const ServiceComponentDescriptor & iScd) const
{
  const i32 subChannel = iScd.subChannelId;
  if (subChannel < 0)
  {
    return "";
  }
  return QString::number(subChannel);
}

QString FibDecoder::get_start_address_of(const ServiceComponentDescriptor & iScd) const
{
  const auto * const p = currentConfig->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(iScd.subChannelId);
  const i32 startAddr = p != nullptr ? p->StartAddr : 0;
  return QString::number(startAddr);
}

QString FibDecoder::get_length_of(const ServiceComponentDescriptor & iScd) const
{
  const auto * const p = currentConfig->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(iScd.subChannelId);
  const i32 subChannelSize = p != nullptr ? p->SubChannelSize : 0;
  return QString::number(subChannelSize);
}

QString FibDecoder::get_prot_level_of(const ServiceComponentDescriptor & iScd) const
{
  const auto * const p = currentConfig->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(iScd.subChannelId);
  if (p != nullptr)
  {
    return getProtectionLevel(p->ShortForm, p->ProtectionLevel); // TODO: refactor getProtectionLevel(), use Option field
  }
  else
  {
    return getProtectionLevel(false, 0);
  }
}

QString FibDecoder::get_code_rate_of(const ServiceComponentDescriptor & iScd) const
{
  const auto * const p = currentConfig->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(iScd.subChannelId);
  if (p != nullptr)
  {
    return getCodeRate(p->ShortForm, p->ProtectionLevel); // TODO: refactor getCodeRate(), use Option field
  }
  else
  {
    return getCodeRate(false, 0);
  }
}

QString FibDecoder::get_bit_rate_of(const ServiceComponentDescriptor & iScd) const
{
  const auto * const p = currentConfig->get_Fig0s1_BasicSubChannelOrganization_of_SubChId(iScd.subChannelId);
  const i32 bitRate = p != nullptr ? p->BitRate : 0;
  return QString::number(bitRate);
}

QString FibDecoder::get_dab_type(const ServiceComponentDescriptor & iScd) const
{
  const i32 dabType = iScd.ASCTy;
  return dabType == 077 ? "DAB+" : "DAB";
}

QString FibDecoder::get_language_of(const ServiceComponentDescriptor & iScd) const
{
  const i32 subChannel = iScd.subChannelId;
  const auto it = currentConfig->subChDescMapFig0_5.find(subChannel);
  const i32 language = it != currentConfig->subChDescMapFig0_5.end() ? it->second.language : 0;
  return getLanguage(language);
}

QString FibDecoder::get_program_type_of(const ServiceComponentDescriptor & iScd) const
{
  const i32 sid = iScd.SId;
  const auto it = ensemble->servicesMap.find(sid);

  if (it != ensemble->servicesMap.end())
  {
    const i32 programType = it->second.programType;
    return getProgramType(programType);
  }
  return "";
}

QString FibDecoder::get_fm_freq_of(const ServiceComponentDescriptor & iScd) const
{
  const i32 sid = iScd.SId;
  const auto it = ensemble->servicesMap.find(sid);

  if (it != ensemble->servicesMap.end())
  {
    return it->second.fmFrequency != -1 ? QString::number(it->second.fmFrequency) : "    ";
  }
  return "";
}

QString FibDecoder::get_app_type_of(const ServiceComponentDescriptor & iScd) const
{
  const i32 appType = iScd.appType;
  return QString::number(appType);
}

QString FibDecoder::get_FEC_scheme(const ServiceComponentDescriptor & iScd) const
{
  const i32 subChannel = iScd.subChannelId;
  const auto it = currentConfig->subChDescMapFig0_14.find(subChannel);
  const i32 FEC_scheme = it != currentConfig->subChDescMapFig0_14.end() ? it->second.FEC_scheme : 0;
  return QString::number(FEC_scheme);
}

QString FibDecoder::get_packet_address(const ServiceComponentDescriptor & iScd) const
{
  const i32 packetAddr = iScd.packetAddress;
  return QString::number(packetAddr);
}

QString FibDecoder::get_DSCTy(const ServiceComponentDescriptor & iScd) const
{
  switch (iScd.DSCTy)
  {
  case 60: return "MOT data";
  case 59: return "IP data";
  case 44: return "Journaline data";
  case  5: return "TDC Data";
  default: return "Unknow data";
  }
}

//
QString FibDecoder::get_audio_header() const
{
  return QString("ServiceName") + ";" + "ServiceId" + ";" + "SubChannel" + ";" + "StartAddr [CU]"+ ";"
               + "Length [CU]" + ";" + "Protection" + ";" + "CodeRate" + ";" + "BitRate" + ";"
               + "DabType" + ";" + "Language" + ";" + "ProgramType" + ";" + "FmFreq" + ";";
}

QString FibDecoder::get_audio_data(const ServiceComponentDescriptor & iScd) const
{
  return get_service_name(iScd) + ";" + get_service_id_of(iScd) + ";" + get_sub_channel_of(iScd) + ";" + get_start_address_of(iScd) + ";"
       + get_length_of(iScd) + ";" + get_prot_level_of(iScd) + ";" + get_code_rate_of(iScd) + ";" + get_bit_rate_of(iScd) + ";"
       + get_dab_type(iScd) + ";" + get_language_of(iScd) + ";" + get_program_type_of(iScd) + ";" + get_fm_freq_of(iScd) + ";";
}

//
QString FibDecoder::get_packet_header() const
{
  return QString("ServiceName") + ";" + "ServiceId" + ";" + "SubChannel" + ";" + "StartAddr [CU]" + ";"
               + "Length [CU]" + ";" + "Protection" + ";" + "CodeRate" + ";" + "AppType" + ";"
               + "FEC_Scheme" + ";" + "PacketAddr" + ";" + "DSCTy" + ";";
}

QString FibDecoder::get_packet_data(const ServiceComponentDescriptor & iScd) const
{
  return get_service_name(iScd) + ";" + get_service_id_of(iScd) + ";" + get_sub_channel_of(iScd) + ";" + get_start_address_of(iScd) + ";"
       + get_length_of(iScd) + ";" + get_prot_level_of(iScd) + ";" + get_code_rate_of(iScd) + ";" + get_app_type_of(iScd) + ";"
       + get_FEC_scheme(iScd) + ";" + get_packet_address(iScd) + ";" + get_DSCTy(iScd) + ";";
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
